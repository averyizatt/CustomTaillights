// ---------------------------------------------------------------------------
// main.cpp
// ESP32-S3 Custom Foxbody Mustang Taillight Controller
//
// Hardware overview
// ─────────────────
//  • Two taillights (left / right), each composed of THREE chained segments:
//      Seg 0 — SEG_TOP_STRIP : 21 cols × 5  rows = 105 LEDs
//      Seg 1 — SEG_BOT_STRIP : 21 cols × 5  rows = 105 LEDs
//      Seg 2 — SEG_MAIN      : 17 cols × 10 rows = 170 LEDs
//      Total per side: 380 LEDs
//      – Left  taillight → GPIO PIN_LED_DRIVER  (20)
//      – Right taillight → GPIO PIN_LED_PASSENGER (19)
//  • 4-channel optocoupler (stock 12 V → 3.3 V isolation)
//      – CH1 Left turn   → GPIO PIN_OPT_LEFT_TURN  (4)
//      – CH2 Right turn  → GPIO PIN_OPT_RIGHT_TURN (5)
//      – CH3 Brake       → GPIO PIN_OPT_BRAKE      (6)
//      – CH4 Reverse     → GPIO PIN_OPT_REVERSE    (7)
//
// Adding new animations
// ──────────────────────
//  1. Subclass Animation in animations.h / animations.cpp.
//  2. Register it in AnimationRegistry::get() for the desired LightState(s).
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "inputs.h"
#include "states.h"
#include "taillight.h"
#include "animations.h"
#include "canbus.h"
#include "thermal.h"
#include "font5x.h"
#include "faults.h"

// ── Pixel buffers (owned by main, shared with TailLight objects) ─────────────
CRGB ledsDriver [LEDS_PER_SIDE];
CRGB ledsPassenger[LEDS_PER_SIDE];

// ── Subsystem objects ────────────────────────────────────────────────────────
Inputs    inputs;
TailLight driverPanel (ledsDriver,  true);
TailLight passengerPanel(ledsPassenger, false);
CANBus         canBus;
ThermalManager thermal;

// ── Timing ───────────────────────────────────────────────────────────────────
static unsigned long lastFrameMs = 0;

// ── Boot-fault state (set in logAndCountReset, reported after CAN is up) ───────
static esp_reset_reason_t g_bootReason       = ESP_RST_UNKNOWN;
static uint32_t           g_bootFaultCount   = 0;

// Stuck input flags saved by st_checkInputPins() before CAN is initialised
static uint8_t g_stuckDriver  = 0;  // bit0=brake bit1=running bit2=turn bit3=reverse
static uint8_t g_stuckPassenger = 0;

// ===========================================================================
// NVS fault counter + boot reason logging
// Persists reset counts to flash (NVS) so crashes are visible even without
// a Serial monitor attached during normal operation.
// ===========================================================================
static void logAndCountReset() {
    Preferences prefs;
    prefs.begin("tailfaults", /*readOnly=*/false);

    esp_reset_reason_t reason = esp_reset_reason();

    // Increment the relevant counter
    switch (reason) {
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT: {
            uint32_t n = prefs.getUInt("wdt", 0) + 1;
            prefs.putUInt("wdt", n);
            Serial.printf("  [FAULT] WDT reset — count now %u\n", n);
            g_bootFaultCount = n;
            break;
        }
        case ESP_RST_PANIC: {
            uint32_t n = prefs.getUInt("panic", 0) + 1;
            prefs.putUInt("panic", n);
            Serial.printf("  [FAULT] Panic reset \u2014 count now %u\n", n);
            g_bootFaultCount = n;
            break;
        }
        case ESP_RST_BROWNOUT: {
            uint32_t n = prefs.getUInt("brownout", 0) + 1;
            prefs.putUInt("brownout", n);
            Serial.printf("  [FAULT] Brownout reset \u2014 count now %u\n", n);
            g_bootFaultCount = n;
            break;
        }
        case ESP_RST_POWERON:
        case ESP_RST_SW:
            // Normal boot — no fault, no NVS write needed.
            // (Writing a counter on every power-on would wear flash with no
            // safety benefit; fault resets above are worth persisting.)
            break;
        default:
            break;
    }

    // Always print the full fault log on boot so a connected Serial monitor
    // immediately shows any accumulated crash history
    Serial.printf("  Fault log  — PowerOn: %u  WDT: %u  Panic: %u  Brownout: %u\n",
        prefs.getUInt("poweron",  0),
        prefs.getUInt("wdt",      0),
        prefs.getUInt("panic",    0),
        prefs.getUInt("brownout", 0));

    const char* reasonStr = "UNKNOWN";
    switch (reason) {
        case ESP_RST_POWERON:  reasonStr = "POWER-ON";  break;
        case ESP_RST_SW:       reasonStr = "SW-RESET";  break;
        case ESP_RST_PANIC:    reasonStr = "PANIC";     break;
        case ESP_RST_WDT:      reasonStr = "WDT";       break;
        case ESP_RST_TASK_WDT: reasonStr = "TASK-WDT";  break;
        case ESP_RST_BROWNOUT: reasonStr = "BROWNOUT";  break;
        case ESP_RST_DEEPSLEEP:reasonStr = "DEEP-SLEEP"; break;
        default: break;
    }
    Serial.printf("  This boot  — reason: %s\n", reasonStr);

    prefs.end();

    g_bootReason = reason;  // save for CAN fault broadcast after canBus.begin()
}

// ---------------------------------------------------------------------------
// Shutdown handler — registered with the IDF so it runs before any reset.
// Blanks both LED panels so they do not freeze on a random animation frame
// during a WDT reset or panic.  Keep this function minimal: complex code
// may not execute reliably in a degraded system state.
// ---------------------------------------------------------------------------
static void onSystemShutdown() {
    fill_solid(ledsDriver,  LEDS_PER_SIDE, CRGB::Black);
    fill_solid(ledsPassenger, LEDS_PER_SIDE, CRGB::Black);
    FastLED.show();
}

// ===========================================================================
// Startup self-test helpers
// All run once inside setup(), before normal animation begins.
// ===========================================================================

// ---------------------------------------------------------------------------
// Print full hardware config to Serial so the installer can verify at a glance.
// ---------------------------------------------------------------------------
static void st_printConfig() {
    Serial.println(F("========================================="));
    Serial.println(F("  Foxbody Taillight Controller — boot"));
    Serial.println(F("========================================="));
    Serial.print(F("  FW build       : ")); Serial.println(F(__DATE__ " " __TIME__));
    Serial.println(F("  --- LED outputs ---"));
    Serial.print(F("  Left  DIN      : GPIO")); Serial.println(PIN_LED_DRIVER);
    Serial.print(F("  Right DIN      : GPIO")); Serial.println(PIN_LED_PASSENGER);
    Serial.print(F("  LEDs per side  : ")); Serial.println(LEDS_PER_SIDE);
    Serial.print(F("  SEG_TOP_STRIP  : ")); Serial.print(STRIP_COLS);
    Serial.print(F(" cols x "));            Serial.print(STRIP_ROWS);
    Serial.print(F(" rows = "));            Serial.print(STRIP_LEDS); Serial.println(F(" px"));
    Serial.print(F("  SEG_BOT_STRIP  : ")); Serial.print(STRIP_COLS);
    Serial.print(F(" cols x "));            Serial.print(STRIP_ROWS);
    Serial.print(F(" rows = "));            Serial.print(STRIP_LEDS); Serial.println(F(" px"));
    Serial.print(F("  SEG_MAIN       : ")); Serial.print(MAIN_COLS);
    Serial.print(F(" cols x "));            Serial.print(MAIN_ROWS);
    Serial.print(F(" rows = "));            Serial.print(MAIN_LEDS);  Serial.println(F(" px"));
    Serial.print(F("  Frame rate     : ~"));
    Serial.print(1000UL / FRAME_INTERVAL_MS); Serial.println(F(" fps"));
    Serial.println(F("  --- Inputs (active-LOW via optocouplers) ---"));
    Serial.println(F("  Left opto:"));
    Serial.print(F("    Brake        : GPIO")); Serial.println(PIN_DRIVER_BRAKE);
    Serial.print(F("    Running      : GPIO")); Serial.println(PIN_DRIVER_RUNNING);
    Serial.print(F("    Turn         : GPIO")); Serial.println(PIN_DRIVER_TURN);
    Serial.print(F("    Reverse      : GPIO")); Serial.println(PIN_DRIVER_REVERSE);
    Serial.println(F("  Right opto:"));
    Serial.print(F("    Brake        : GPIO")); Serial.println(PIN_PASSENGER_BRAKE);
    Serial.print(F("    Running      : GPIO")); Serial.println(PIN_PASSENGER_RUNNING);
    Serial.print(F("    Turn         : GPIO")); Serial.println(PIN_PASSENGER_TURN);
    Serial.print(F("    Reverse      : GPIO")); Serial.println(PIN_PASSENGER_REVERSE);
    Serial.println(F("========================================="));
}

// ---------------------------------------------------------------------------
// Verify all four input lines are idle (HIGH) at power-on.
// An active line at boot likely means a wiring short or a signal already
// present — flag it but do not halt.
// Returns true if all inputs are idle.
// ---------------------------------------------------------------------------
static bool st_checkInputPins() {
    const int   pins[8]  = { PIN_DRIVER_BRAKE,  PIN_DRIVER_RUNNING,  PIN_DRIVER_TURN,  PIN_DRIVER_REVERSE,
                              PIN_PASSENGER_BRAKE, PIN_PASSENGER_RUNNING, PIN_PASSENGER_TURN, PIN_PASSENGER_REVERSE };
    const char* names[8] = { "L-BRAKE", "L-RUN", "L-TURN", "L-REV",
                              "R-BRAKE", "R-RUN", "R-TURN", "R-REV" };
    bool allOk = true;

    for (int i = 0; i < 8; i++) {
        bool active = (digitalRead(pins[i]) == OPT_ACTIVE_LEVEL);
        if (active) {
            Serial.print(F("  [WARN] ")); Serial.print(names[i]);
            Serial.println(F(" is ACTIVE at boot — check wiring"));
            allOk = false;            // Save in the boot-fault globals so we can report over CAN later
            if (i < 4) g_stuckDriver  |= (1 << i);
            else        g_stuckPassenger |= (1 << (i - 4));        } else {
            Serial.print(F("  [OK]   ")); Serial.print(names[i]);
            Serial.println(F(" idle"));
        }
    }
    return allOk;
}

// ---------------------------------------------------------------------------
// Flash one segment on both sides with `colour` for `holdMs`, then blank.
// Directly writes the raw pixel buffers so no TailLight state is disturbed.
// ---------------------------------------------------------------------------
static void st_flashSegment(int seg, CRGB colour, uint32_t holdMs) {
    int count = (seg == SEG_MAIN) ? MAIN_LEDS : STRIP_LEDS;
    int base  = SEG_OFFSET[seg];
    for (int i = base; i < base + count; i++) {
        ledsDriver[i]  = colour;
        ledsPassenger[i] = colour;
    }
    FastLED.show();
    delay(holdMs);
    for (int i = base; i < base + count; i++) {
        ledsDriver[i]  = CRGB::Black;
        ledsPassenger[i] = CRGB::Black;
    }
    FastLED.show();
    delay(100);
}

// ---------------------------------------------------------------------------
// Segment identification flash.
// Each segment lights up in a distinct colour so you can confirm which
// physical section corresponds to which buffer segment.
//
//   SEG_TOP_STRIP → white
//   SEG_BOT_STRIP → cyan
//   SEG_MAIN      → amber
//
// Both left and right sides flash simultaneously.
// ---------------------------------------------------------------------------
static void st_segmentIDTest() {
    Serial.println(F("  Segment 0 — TOP_STRIP (white)"));
    st_flashSegment(SEG_TOP_STRIP, CRGB(200, 200, 200), 500);

    Serial.println(F("  Segment 1 — BOT_STRIP (cyan)"));
    st_flashSegment(SEG_BOT_STRIP, CRGB(0, 200, 200),   500);

    Serial.println(F("  Segment 2 — MAIN      (amber)"));
    st_flashSegment(SEG_MAIN,      CRGB(255, 100, 0),   500);
}

// ---------------------------------------------------------------------------
// Pixel chaser — runs a white comet from pixel 0 to pixel 379 on both sides
// simultaneously, verifying every LED lights up and that the pixel order
// matches the expected physical layout.
// ---------------------------------------------------------------------------
static void st_chaserTest() {
    static constexpr int     TAIL    = 10;   // comet tail length
    static constexpr uint8_t DIM_STEP = 255 / TAIL;
    // Total positions = pixels + tail so the comet fully exits the strip
    const int total = LEDS_PER_SIDE + TAIL;

    for (int pos = 0; pos < total; pos++) {
        fill_solid(ledsDriver,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, CRGB::Black);

        for (int t = 0; t < TAIL; t++) {
            int idx = pos - t;
            if (idx >= 0 && idx < LEDS_PER_SIDE) {
                uint8_t bright = 255 - (uint8_t)(t * DIM_STEP);
                ledsDriver[idx]  = CRGB(bright, bright, bright);
                ledsPassenger[idx] = CRGB(bright, bright, bright);
            }
        }
        FastLED.show();
        delay(6);  // ~166 px/sec — visually readable chase speed
    }
}

// ---------------------------------------------------------------------------
// RGB colour verify — flash all LEDs red, then green, then blue in sequence.
// A wrong colour order in the FastLED colour-order define will be immediately
// obvious.
// ---------------------------------------------------------------------------
static void st_colorVerify() {
    const CRGB cols[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };
    const char* names[3] = { "RED", "GREEN", "BLUE" };
    for (int i = 0; i < 3; i++) {
        Serial.print(F("  Colour: ")); Serial.println(names[i]);
        fill_solid(ledsDriver,  LEDS_PER_SIDE, cols[i]);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, cols[i]);
        FastLED.show();
        delay(350);
    }
    FastLED.clear(true);
}

// ---------------------------------------------------------------------------
// Top-level self-test entry point — called once from setup().
// ---------------------------------------------------------------------------
static void runStartupSelfTest() {
    Serial.println(F("[self-test] ---- BEGIN ----"));

    // 1. Print hardware config
    st_printConfig();

    // 2. Input pin sanity
    Serial.println(F("[self-test] Checking input pins ..."));
    bool inputsOk = st_checkInputPins();

    // 3. Segment identification
    Serial.println(F("[self-test] Segment ID flash ..."));
    st_segmentIDTest();

    // 4. Pixel chaser (380 px per side)
    Serial.println(F("[self-test] Pixel chaser (verify order & count) ..."));
    st_chaserTest();

    // 5. RGB colour verify
    Serial.println(F("[self-test] RGB colour verify ..."));
    st_colorVerify();

    // 6. Result
    Serial.println(F(""));
    if (inputsOk) {
        Serial.println(F("[self-test] ---- PASS ----"));
    } else {
        Serial.println(F("[self-test] ---- DONE (see warnings above) ----"));
    }
    Serial.println(F(""));
}

// ===========================================================================
// Animation demo — plays once after the startup self-test.
// Five scenes across all 380 LEDs per side.  Uses delay() like the self-test.
// ===========================================================================

// ===========================================================================
// Startup animation — quick automotive rear-light sequence
// Total ≈ 900 ms.  Replaces the old 9-second demo reel.
//
// Phase 1 (~380 ms): Red sequential fill sweeps outward on all three
//   segments simultaneously.  Left panel fills col 0 → 20; right panel
//   fills col 20 → 0.  Segments scale proportionally so they finish together.
//
// Phase 2 (~200 ms): White comet sweeps across the top strip (clear diffuser)
//   outward on each side, leaving a dim-red base.
//
// Phase 3 (~300 ms): Brightness fades from full red to black.
// ===========================================================================
static void runStartupAnim() {
    // ── Phase 1: outward sequential red fill ─────────────────────────────────
    for (int step = 0; step <= STRIP_COLS + 2; step++) {
        fill_solid(ledsDriver,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, CRGB::Black);

        for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
            int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
            int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
            int limit   = constrain((step * segCols) / STRIP_COLS, 0, segCols - 1);

            for (int col = 0; col <= limit; col++) {
                for (int row = 0; row < segRows; row++) {
                    driverPanel.setPixel(seg, row, col,                    CRGB(255, 0, 0));
                    passengerPanel.setPixel(seg, row, segCols - 1 - col, CRGB(255, 0, 0));
                }
            }
        }
        FastLED.show();
        delay(17);
    }

    // ── Phase 2: white comet on top strip (clear diffuser) ───────────────────
    static constexpr int COMET_TAIL = 7;
    for (int head = 0; head < STRIP_COLS + COMET_TAIL; head++) {
        // Leave red on segs 1+2; repaint top strip only
        for (int col = 0; col < STRIP_COLS; col++) {
            for (int row = 0; row < STRIP_ROWS; row++) {
                driverPanel.setPixel( SEG_TOP_STRIP, row, col,                    CRGB(20, 0, 0));
                passengerPanel.setPixel(SEG_TOP_STRIP, row, col,                    CRGB(20, 0, 0));
            }
        }
        for (int t = 0; t < COMET_TAIL; t++) {
            int   idxL = head - t;
            int   idxR = (STRIP_COLS - 1) - (head - t);
            uint8_t b  = (uint8_t)(255 - (255 * t / COMET_TAIL));
            if (idxL >= 0 && idxL < STRIP_COLS) {
                for (int row = 0; row < STRIP_ROWS; row++)
                    driverPanel.setPixel( SEG_TOP_STRIP, row, idxL, CRGB(b, b, b));
            }
            if (idxR >= 0 && idxR < STRIP_COLS) {
                for (int row = 0; row < STRIP_ROWS; row++)
                    passengerPanel.setPixel(SEG_TOP_STRIP, row, idxR, CRGB(b, b, b));
            }
        }
        FastLED.show();
        delay(8);
    }

    // ── Phase 3: fade to black ────────────────────────────────────────────────
    for (int step = 15; step >= 0; step--) {
        uint8_t scale = (uint8_t)(step * 17);
        for (int i = 0; i < LEDS_PER_SIDE; i++) {
            ledsDriver[i]  = CRGB((uint8_t)((ledsDriver[i].r  * scale) >> 8), 0, 0);
            ledsPassenger[i] = CRGB((uint8_t)((ledsPassenger[i].r * scale) >> 8), 0, 0);
        }
        FastLED.show();
        delay(18);
    }
    fill_solid(ledsDriver,  LEDS_PER_SIDE, CRGB::Black);
    fill_solid(ledsPassenger, LEDS_PER_SIDE, CRGB::Black);
    FastLED.show();
}

// ===========================================================================
// Bench test cycle — cycles through every LightState in sequence so you can
// verify each animation looks correct without a car harness.
//
// ENTRY: Hold PIN_DRIVER_RUNNING (GPIO 6) active at power-on.
//        The cycle starts automatically after the startup self-test.
//
// Each state is held for the indicated duration while Serial prints the
// state name.  After all states have been shown the cycle ends and normal
// operation resumes.
// ===========================================================================
static void runTestCycle() {
    struct TestStep {
        LightState left;
        LightState right;
        const char* label;
        uint32_t    durationMs;
    };

    static const TestStep steps[] = {
        { LightState::RUNNING,    LightState::RUNNING,    "RUNNING (parking)",          2500 },
        { LightState::BRAKE,      LightState::BRAKE,      "BRAKE",                      2500 },
        { LightState::TURN,       LightState::OFF,        "LEFT TURN SIGNAL",           3500 },
        { LightState::OFF,        LightState::TURN,       "RIGHT TURN SIGNAL",          3500 },
        { LightState::BRAKE_TURN, LightState::BRAKE,      "BRAKE + LEFT TURN",          3500 },
        { LightState::BRAKE,      LightState::BRAKE_TURN, "BRAKE + RIGHT TURN",         3500 },
        { LightState::REVERSE,    LightState::REVERSE,    "REVERSE",                    2500 },
        { LightState::HAZARD,     LightState::HAZARD,     "HAZARD",                     3500 },
        { LightState::OFF,        LightState::OFF,        "OFF",                        1000 },
    };

    Serial.println(F(""));
    Serial.println(F("[TEST] ====================================="));
    Serial.println(F("[TEST]   BENCH TEST CYCLE — STARTING"));
    Serial.println(F("[TEST] ====================================="));
    Serial.println(F("[TEST] PIN_DRIVER_RUNNING held at boot."));
    Serial.println(F("[TEST] Cycling through all light states."));
    Serial.println(F(""));

    // Ensure animation system is ready for this panel
    driverPanel.begin();
    passengerPanel.begin();

    const int numSteps = (int)(sizeof(steps) / sizeof(steps[0]));
    for (int i = 0; i < numSteps; i++) {
        const TestStep& s = steps[i];
        Serial.print(F("[TEST] [")); Serial.print(i + 1); Serial.print(F("/"));
        Serial.print(numSteps); Serial.print(F("] "));
        Serial.println(s.label);

        unsigned long start = millis();
        while (millis() - start < s.durationMs) {
            unsigned long now = millis();
            driverPanel.update(s.left,  now);
            passengerPanel.update(s.right, now);
            FastLED.show();
            delay(16);
        }
    }

    Serial.println(F(""));
    Serial.println(F("[TEST] ====================================="));
    Serial.println(F("[TEST]   BENCH TEST CYCLE — COMPLETE"));
    Serial.println(F("[TEST]   Resuming normal operation."));
    Serial.println(F("[TEST] ====================================="));
    Serial.println(F(""));

    driverPanel.fill(CRGB::Black);
    passengerPanel.fill(CRGB::Black);
    FastLED.show();
    delay(200);
}

// ── Scene 1: Rainbow river (2 s) ─────────────────────────────────────────────
// A hue rainbow scrolls across both panels.  Left and right scroll in opposite
// directions so the two panels appear to "meet" in the middle.
static void demo_rainbowRiver() {
    const int durationMs = 2000;
    const int frameMs    = 16;
    for (int t = 0; t < durationMs; t += frameMs) {
        uint8_t offsetL = (uint8_t)((t * 3) & 0xFF);
        uint8_t offsetR = (uint8_t)(255 - ((t * 3) & 0xFF));
        for (int i = 0; i < LEDS_PER_SIDE; i++) {
            uint8_t hueL = offsetL + (uint8_t)(i * 255 / LEDS_PER_SIDE);
            uint8_t hueR = offsetR + (uint8_t)(i * 255 / LEDS_PER_SIDE);
            ledsDriver[i]  = CHSV(hueL, 230, 200);
            ledsPassenger[i] = CHSV(hueR, 230, 200);
        }
        FastLED.show();
        delay(frameMs);
    }
    FastLED.clear(true);
    delay(80);
}

// ── Scene 2: Cyan mirror bolt (≈3 s) ─────────────────────────────────────────
// Two comets emerge from the pixel-buffer centre and race to the ends;
// then two more race inward from the ends back to the centre.
static void demo_mirrorBolt() {
    const int TAIL    = 10;
    const int frameMs = 7;
    const int CENTER  = LEDS_PER_SIDE / 2;

    auto drawBolt = [](int rIdx, int lIdx, uint8_t b) {
        if (rIdx >= 0 && rIdx < LEDS_PER_SIDE) {
            ledsDriver[rIdx]  = CRGB(0, b, b);
            ledsPassenger[rIdx] = CRGB(0, b, b);
        }
        if (lIdx >= 0 && lIdx < LEDS_PER_SIDE) {
            ledsDriver[lIdx]  = CRGB(0, b, b);
            ledsPassenger[lIdx] = CRGB(0, b, b);
        }
    };

    // Outward — from centre toward both ends
    for (int head = 0; head <= CENTER + TAIL; head++) {
        fill_solid(ledsDriver,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, CRGB::Black);
        for (int t = 0; t < TAIL; t++) {
            int offset = head - t;
            if (offset < 0) continue;
            uint8_t b = 255 - (uint8_t)(t * 255 / TAIL);
            drawBolt(CENTER + offset, CENTER - offset, b);
        }
        FastLED.show();
        delay(frameMs);
    }

    // Inward — from both ends toward centre
    for (int head = 0; head <= CENTER + TAIL; head++) {
        fill_solid(ledsDriver,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, CRGB::Black);
        for (int t = 0; t < TAIL; t++) {
            int offset = head - t;
            if (offset < 0) continue;
            uint8_t b = 255 - (uint8_t)(t * 255 / TAIL);
            drawBolt((LEDS_PER_SIDE - 1) - offset, offset, b);
        }
        FastLED.show();
        delay(frameMs);
    }
    FastLED.clear(true);
    delay(80);
}

// ── Scene 3: Red theater chase (1.5 s) ───────────────────────────────────────
// Every third pixel lights up in deep red; left panel chases forward,
// right panel chases backward so they mirror each other.
static void demo_theaterChase() {
    const int durationMs = 1500;
    const int frameMs    = 55;
    int step = 0;
    for (int t = 0; t < durationMs; t += frameMs, step++) {
        for (int i = 0; i < LEDS_PER_SIDE; i++) {
            ledsDriver[i]  = ((i + step)          % 3 == 0) ? CRGB(220, 30, 0) : CRGB::Black;
            ledsPassenger[i] = ((i - step + 3 * 100) % 3 == 0) ? CRGB(220, 30, 0) : CRGB::Black;
        }
        FastLED.show();
        delay(frameMs);
    }
    FastLED.clear(true);
    delay(80);
}

// ── Scene 4: Magenta breathe (1.6 s, 2 pulses) ───────────────────────────────
// Smooth sine-wave brightness using FastLED's sin8() — no floating point needed.
static void demo_breathe() {
    const int durationMs = 1600;
    const int frameMs    = 16;
    for (int t = 0; t < durationMs; t += frameMs) {
        // sin8 input cycles 0-255 over an 800 ms period
        uint8_t sinInput = (uint8_t)((t % 800) * 256 / 800);
        uint8_t b        = sin8(sinInput);  // 0-255 smooth sine
        CRGB col(b, 0, (uint8_t)(b >> 1));  // deep magenta / purple
        fill_solid(ledsDriver,  LEDS_PER_SIDE, col);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    FastLED.clear(true);
    delay(80);
}

// ── Scene 5: Foxbody fire-flash finale (≈1.1 s) ───────────────────────────────
// Amber ramp-up → white burst → slow fade — like the taillights igniting.
static void demo_fireFlash() {
    const int frameMs = 16;
    // Amber ramp (400 ms)
    for (int t = 0; t < 400; t += frameMs) {
        uint8_t p = (uint8_t)(255 * t / 400);
        CRGB col(255, (uint8_t)(p >> 2), 0);
        fill_solid(ledsDriver,  LEDS_PER_SIDE, col);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    // White burst (250 ms)
    for (int t = 0; t < 250; t += frameMs) {
        uint8_t p = (uint8_t)(255 * t / 250);
        CRGB col(255, p, p);
        fill_solid(ledsDriver,  LEDS_PER_SIDE, col);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    // Fade out (450 ms)
    for (int t = 0; t < 450; t += frameMs) {
        uint8_t p = 255 - (uint8_t)(255 * t / 450);
        CRGB col(p, p, p);
        fill_solid(ledsDriver,  LEDS_PER_SIDE, col);
        fill_solid(ledsPassenger, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    FastLED.clear(true);
}

// ── Scene 6: Scrolling text on the top strip ─────────────────────────────────
// The top strip has a clear diffuser so text appears in its true colour.
// To change the text, colour, or speed, edit the font5x_scroll() call below.
// Full A–Z, 0–9, and punctuation are available — see font5x.h for details.
static void demo_scrollText() {
    font5x_scroll(ledsDriver, ledsPassenger,
                  "MADE BY AVERY IZATT",  // text to display
                  CRGB(220, 220, 220),    // white on clear diffuser
                  CRGB(30, 0, 0),         // dim red on red-diffuser segments
                  45);                    // ms per column step
}

// ---------------------------------------------------------------------------
// Top-level demo entry point — called once from setup().
// ---------------------------------------------------------------------------
static void runAnimationDemo() {
    Serial.println(F("[demo] ---- BEGIN ----"));
    Serial.println(F("[demo] Scene 1: Rainbow river"));
    demo_rainbowRiver();
    Serial.println(F("[demo] Scene 2: Mirror bolt"));
    demo_mirrorBolt();
    Serial.println(F("[demo] Scene 3: Theater chase"));
    demo_theaterChase();
    Serial.println(F("[demo] Scene 4: Breathe"));
    demo_breathe();
    Serial.println(F("[demo] Scene 5: Fire flash"));
    demo_fireFlash();
    Serial.println(F("[demo] Scene 6: Scrolling text"));
    demo_scrollText();
    Serial.println(F("[demo] ---- END ----"));
}

// ===========================================================================
// Core 0 input task
// ===========================================================================
// Polls all 8 optocoupler channels every INPUT_TASK_PERIOD_MS (1 ms) on
// Core 0, completely independent of the LED render time on Core 1.
// Both this task and loop() are subscribed to the hardware Task Watchdog;
// if either hangs, the MCU performs a clean reset within WDT_TIMEOUT_S.
// ---------------------------------------------------------------------------
static void inputTaskFn(void* /*param*/) {
    // Register with the Task Watchdog.  Must be called after
    // esp_task_wdt_init() which runs at the end of setup().
    esp_task_wdt_add(nullptr);

    TickType_t lastWake = xTaskGetTickCount();
    for (;;) {
        inputs.update();
        esp_task_wdt_reset();
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(INPUT_TASK_PERIOD_MS));
    }
    // Unreachable — FreeRTOS tasks must not return.
}

// ===========================================================================
// Arduino entry points
// ===========================================================================

// ---------------------------------------------------------------------------
void setup() {
    // ── Disable unused RF subsystems ─────────────────────────────────────────
    // This device communicates exclusively over CAN bus.  Disabling WiFi and
    // BLE frees ~80 mA of idle current, frees heap, and eliminates the RF
    // drivers as a potential crash source in the automotive EMI environment.
    WiFi.mode(WIFI_OFF);   // safe to call before WiFi is ever started
    btStop();              // no-op if BLE never started; disables it otherwise

    Serial.begin(115200);

    // ── Boot reason + NVS fault counter ──────────────────────────────────────
    // Must run before any Serial output that overwrites the reason line.
    logAndCountReset();

    // ── Graceful LED blank on any future reset ────────────────────────────────
    // The handler clears both panels before the MCU restarts, so a WDT or
    // panic does not leave LEDs frozen on a random animation frame.
    esp_register_shutdown_handler(onSystemShutdown);

    // Register LED panels with FastLED
    FastLED.addLeds<LED_CHIPSET, PIN_LED_DRIVER,  LED_COLOR_ORDER>(ledsDriver,  LEDS_PER_SIDE);
    FastLED.addLeds<LED_CHIPSET, PIN_LED_PASSENGER, LED_COLOR_ORDER>(ledsPassenger, LEDS_PER_SIDE);
    FastLED.setBrightness(BRIGHTNESS_DEFAULT);
    FastLED.clear(true);
    // Disable temporal dithering: it adds CPU jitter and is unsuitable for
    // safety-critical lighting where consistent brightness is required.
    FastLED.setDither(DISABLE_DITHER);
    // Cap per-frame current draw to LED_POWER_BUDGET_MA (14.5 A).
    // FastLED calculates estimated draw from pixel colours each frame and
    // scales global brightness down automatically if the budget would be
    // exceeded.  This runs inside FastLED.show() — no manual work needed.
    FastLED.setMaxPowerInVoltsAndMilliamps(LED_VOLTAGE, LED_POWER_BUDGET_MA);
    // ── Crank holdoff ────────────────────────────────────────────────────────
    // Keep LEDs blank for CRANK_HOLDOFF_MS after boot.  If the car is being
    // cranked and a momentary voltage sag resets the ESP32, it will restart
    // the holdoff and wait again.  The WDT is not yet active here so a
    // simple delay loop is safe.
    if (CRANK_HOLDOFF_MS > 0) {
        Serial.printf("[boot] crank holdoff — waiting %lu ms for supply to stabilise ...\n",
                      CRANK_HOLDOFF_MS);
        const unsigned long holdStart = millis();
        while ((millis() - holdStart) < CRANK_HOLDOFF_MS) {
            delay(10);  // yield; LEDs are already blank from FastLED.clear(true) above
        }
        Serial.println(F("[boot] holdoff complete — supply stable"));
    }
    // Initialise animation registry
    AnimationRegistry::init();

    // Initialise optocoupler inputs (sets pin modes — must run before self-test)
    inputs.begin();

    // ── Bench test cycle entry check ─────────────────────────────────────────
    // Hold PIN_DRIVER_RUNNING (GPIO 6) active while powering on to run a full
    // state walk-through before entering normal operation.  Useful for
    // verifying every animation on the bench without a car harness.
    // The check is intentionally placed before the self-test so Serial output
    // from runTestCycle() appears immediately after pin-mode init.
    const bool testModeRequested = (digitalRead(PIN_DRIVER_RUNNING) == OPT_ACTIVE_LEVEL);

    // ── Startup self-test ────────────────────────────────────────────────────
    // Runs the segment ID flash, pixel chaser, and colour verify (~5 s).
    // Only runs in bench mode (PIN_DRIVER_RUNNING held at power-on) so normal
    // in-car boots reach the main loop — and functional brake lights — as
    // fast as possible.  Never run the blocking self-test on every power cycle
    // of a safety-critical device.
    if (testModeRequested) {
        runStartupSelfTest();
    }

    // ── Startup animation ────────────────────────────────────────────────────
    // Quick automotive sequential sweep (~900 ms).  Runs on every boot so
    // there is a brief visual confirmation the panels are alive.
    runStartupAnim();

    // ── Bench test cycle ─────────────────────────────────────────────────────
    // Runs only if PIN_DRIVER_RUNNING was held active at power-on.
    if (testModeRequested) {
        runTestCycle();
    }
    // ────────────────────────────────────────────────────────────────────────

    // Initialise taillight panels (sets idle animation)
    driverPanel.begin();
    passengerPanel.begin();

    // Initialise CAN bus (MCP2515 via SPI)
    canBus.begin();

    // ── Boot fault reporting over CAN ────────────────────────────────────────
    // Faults detected before CAN was up are reported here as one-shots.
    switch (g_bootReason) {
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            canBus.reportFault(FAULT_WDT_RESET, FAULT_SEV_WARNING,
                               (uint8_t)(g_bootFaultCount & 0xFF));
            break;
        case ESP_RST_PANIC:
            canBus.reportFault(FAULT_PANIC_RESET, FAULT_SEV_CRITICAL,
                               (uint8_t)(g_bootFaultCount & 0xFF));
            break;
        case ESP_RST_BROWNOUT:
            canBus.reportFault(FAULT_BROWNOUT_RESET, FAULT_SEV_WARNING,
                               (uint8_t)(g_bootFaultCount & 0xFF));
            break;
        default:
            break;
    }
    if (g_stuckDriver != 0 || g_stuckPassenger != 0) {
        canBus.reportFault(FAULT_INPUT_STUCK_BOOT, FAULT_SEV_WARNING,
                           g_stuckDriver, g_stuckPassenger);
    }

    // Initialise thermal manager (reads initial die temperature)
    thermal.begin();

    // ── Hardware Task Watchdog ────────────────────────────────────────────────
    // Initialised AFTER the startup demo so the long delay() sequences do
    // not cause a premature watchdog reset.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    {
        const esp_task_wdt_config_t wdtCfg = {
            .timeout_ms     = WDT_TIMEOUT_S * 1000,
            .idle_core_mask = 0,
            .trigger_panic  = true,
        };
        esp_task_wdt_reconfigure(&wdtCfg);
    }
#else
    esp_task_wdt_init(WDT_TIMEOUT_S, /*panic=*/true);
#endif
    // Subscribe the render task (loop) to the watchdog.
    esp_task_wdt_add(nullptr);

    // ── Input task (Core 0, priority 4) ──────────────────────────────────────────
    // Polls all 8 opto channels at 1 ms, independent of LED render time.
    // Priority 4 = above normal; below FreeRTOS timer daemon (prio 5).
    xTaskCreatePinnedToCore(
        inputTaskFn,      // function
        "inputs",         // name (for debugger)
        2048,             // stack words (bytes on Xtensa)
        nullptr,          // param
        4,                // priority
        nullptr,          // handle (not needed)
        0                 // Core 0
    );
    // ────────────────────────────────────────────────────────────────────────

    Serial.println(F("[taillight] ready — entering main loop"));
}

// ---------------------------------------------------------------------------
void loop() {
    unsigned long nowMs = millis();

    // Feed the hardware Task Watchdog.  If this call stops arriving within
    // WDT_TIMEOUT_S seconds the MCU performs a clean panic reset.
    esp_task_wdt_reset();

    // ── Thermal management ──────────────────────────────────────────────────
    thermal.tick(nowMs);

    // ── Thermal fault reporting ─────────────────────────────────────────────
    // Report on state change only to avoid spamming the CAN bus.
    {
        static uint8_t prevThermalFault = FAULT_NONE;
        uint8_t newFault = FAULT_NONE;
        float   t        = thermal.tempC();
        if (t >= TEMP_SHUTDOWN_C)      newFault = FAULT_THERMAL_CRITICAL;
        else if (t >= TEMP_DERATE_START_C) newFault = FAULT_THERMAL_WARN;

        if (newFault != prevThermalFault) {
            if (newFault != FAULT_NONE) {
                uint8_t sev = (newFault == FAULT_THERMAL_CRITICAL)
                              ? FAULT_SEV_CRITICAL : FAULT_SEV_WARNING;
                canBus.reportFault(newFault, sev,
                                   (uint8_t)constrain((int)t, 0, 255));
            }
            prevThermalFault = newFault;
        }
    }

    // Determine target brightness: prefer CAN override, else default.
    // Always pass through thermal derating — it is never bypassed, even
    // by a CAN command, so safety-critical lights always remain visible.
    uint8_t targetBrightness = canBus.brightnessChanged()
                             ? canBus.brightness()
                             : BRIGHTNESS_DEFAULT;
    uint8_t safeBrightness = thermal.applyBrightness(targetBrightness);
    FastLED.setBrightness(safeBrightness);
    if (canBus.brightnessChanged()) canBus.clearBrightnessChanged();

    // Input is polled by the input task on Core 0.  Read the atomic snapshots
    // (single-byte loads — guaranteed atomic on Xtensa LX7) so we always see
    // a coherent set of flags from a single debounce cycle.
    const uint8_t ds = inputs.driverSnapshot();
    const uint8_t ps = inputs.passengerSnapshot();

    LightState driverState = resolveSideState(
        ds & 0x01, ds & 0x02, ds & 0x04, ds & 0x08,
        ps & 0x04
    );
    LightState passengerState = resolveSideState(
        ps & 0x01, ps & 0x02, ps & 0x04, ps & 0x08,
        ds & 0x04
    );

    // ── CAN bus tick (TX broadcast + RX command processing) ─────────────────
    canBus.tick(driverState, passengerState, inputs, thermal);

    // ── State override priority (highest → lowest) ───────────────────────────
    //  1. Custom animation (Cmd 0x04) — plays to completion, then auto-clears
    //  2. Animation override (Cmd 0x02) — holds until Cmd 0x03
    //  3. Normal input-driven state
    if (canBus.hasCustomAnim()) {
        // Safety: physical brake and reverse signals are never suppressed by a
        // custom animation.  bit0 = brake, bit3 = reverse (see inputs.h).
        if (!(ds & 0x01) && !(ds & 0x08)) driverState  = LightState::CUSTOM;
        if (!(ps & 0x01) && !(ps & 0x08)) passengerState = LightState::CUSTOM;

        // Auto-clear once both sides' animations report done
        bool leftDone  = false;
        bool rightDone = false;
        switch (AnimationRegistry::customSlot()) {
            case AnimationRegistry::CustomSlot::SCROLL_TEXT:
                leftDone = rightDone = AnimationRegistry::scrollText().isDone();
                break;
            case AnimationRegistry::CustomSlot::FLASH:
                leftDone = rightDone = AnimationRegistry::flash().isDone();
                break;
            default:
                leftDone = rightDone = true;
                break;
        }
        if (leftDone && rightDone) {
            canBus.clearCustomAnim();
            AnimationRegistry::setCustomSlot(AnimationRegistry::CustomSlot::NONE);
        }
    } else if (canBus.hasOverride()) {
        // Safety: never suppress an active brake or reverse signal via a CAN
        // command — these are safety-critical lights.  Apply the override only
        // when the physical brake and reverse channels are both inactive on
        // each respective side.  bit0 = brake, bit3 = reverse (see inputs.h).
        if (!(ds & 0x01) && !(ds & 0x08)) driverState  = canBus.overrideDriver();
        if (!(ps & 0x01) && !(ps & 0x08)) passengerState = canBus.overridePassenger();
    }

    // Throttle animation updates to FRAME_INTERVAL_MS (~60 fps)
    if (nowMs - lastFrameMs >= FRAME_INTERVAL_MS) {
        lastFrameMs = nowMs;

        driverPanel.update(driverState, nowMs);
        passengerPanel.update(passengerState, nowMs);

        FastLED.show();
    }
}
