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
//      – Left  taillight → GPIO PIN_LED_LEFT  (20)
//      – Right taillight → GPIO PIN_LED_RIGHT (19)
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
#include "voltage_monitor.h"

// ── Pixel buffers (owned by main, shared with TailLight objects) ─────────────
CRGB ledsLeft [LEDS_PER_SIDE];
CRGB ledsRight[LEDS_PER_SIDE];

// ── Subsystem objects ────────────────────────────────────────────────────────
Inputs    inputs;
TailLight leftPanel (ledsLeft,  true);
TailLight rightPanel(ledsRight, false);
CANBus         canBus;
ThermalManager thermal;
VoltageMonitor voltMon;

// ── Timing ───────────────────────────────────────────────────────────────────
static unsigned long lastFrameMs  = 0;
static bool          vmonWasReady = true;  // tracks UVLO transition for one-shot blank

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
            break;
        }
        case ESP_RST_PANIC: {
            uint32_t n = prefs.getUInt("panic", 0) + 1;
            prefs.putUInt("panic", n);
            Serial.printf("  [FAULT] Panic reset — count now %u\n", n);
            break;
        }
        case ESP_RST_BROWNOUT: {
            uint32_t n = prefs.getUInt("brownout", 0) + 1;
            prefs.putUInt("brownout", n);
            Serial.printf("  [FAULT] Brownout reset — count now %u\n", n);
            break;
        }
        case ESP_RST_POWERON:
        case ESP_RST_SW: {
            uint32_t n = prefs.getUInt("poweron", 0) + 1;
            prefs.putUInt("poweron", n);
            // Normal boot — no fault message
            break;
        }
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
}

// ---------------------------------------------------------------------------
// Shutdown handler — registered with the IDF so it runs before any reset.
// Blanks both LED panels so they do not freeze on a random animation frame
// during a WDT reset or panic.  Keep this function minimal: complex code
// may not execute reliably in a degraded system state.
// ---------------------------------------------------------------------------
static void onSystemShutdown() {
    fill_solid(ledsLeft,  LEDS_PER_SIDE, CRGB::Black);
    fill_solid(ledsRight, LEDS_PER_SIDE, CRGB::Black);
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
    Serial.print(F("  Left  DIN      : GPIO")); Serial.println(PIN_LED_LEFT);
    Serial.print(F("  Right DIN      : GPIO")); Serial.println(PIN_LED_RIGHT);
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
    Serial.print(F("    Brake        : GPIO")); Serial.println(PIN_LEFT_BRAKE);
    Serial.print(F("    Running      : GPIO")); Serial.println(PIN_LEFT_RUNNING);
    Serial.print(F("    Turn         : GPIO")); Serial.println(PIN_LEFT_TURN);
    Serial.print(F("    Reverse      : GPIO")); Serial.println(PIN_LEFT_REVERSE);
    Serial.println(F("  Right opto:"));
    Serial.print(F("    Brake        : GPIO")); Serial.println(PIN_RIGHT_BRAKE);
    Serial.print(F("    Running      : GPIO")); Serial.println(PIN_RIGHT_RUNNING);
    Serial.print(F("    Turn         : GPIO")); Serial.println(PIN_RIGHT_TURN);
    Serial.print(F("    Reverse      : GPIO")); Serial.println(PIN_RIGHT_REVERSE);
    Serial.println(F("========================================="));
}

// ---------------------------------------------------------------------------
// Verify all four input lines are idle (HIGH) at power-on.
// An active line at boot likely means a wiring short or a signal already
// present — flag it but do not halt.
// Returns true if all inputs are idle.
// ---------------------------------------------------------------------------
static bool st_checkInputPins() {
    const int   pins[8]  = { PIN_LEFT_BRAKE,  PIN_LEFT_RUNNING,  PIN_LEFT_TURN,  PIN_LEFT_REVERSE,
                              PIN_RIGHT_BRAKE, PIN_RIGHT_RUNNING, PIN_RIGHT_TURN, PIN_RIGHT_REVERSE };
    const char* names[8] = { "L-BRAKE", "L-RUN", "L-TURN", "L-REV",
                              "R-BRAKE", "R-RUN", "R-TURN", "R-REV" };
    bool allOk = true;

    for (int i = 0; i < 8; i++) {
        bool active = (digitalRead(pins[i]) == OPT_ACTIVE_LEVEL);
        if (active) {
            Serial.print(F("  [WARN] ")); Serial.print(names[i]);
            Serial.println(F(" is ACTIVE at boot — check wiring"));
            allOk = false;
        } else {
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
        ledsLeft[i]  = colour;
        ledsRight[i] = colour;
    }
    FastLED.show();
    delay(holdMs);
    for (int i = base; i < base + count; i++) {
        ledsLeft[i]  = CRGB::Black;
        ledsRight[i] = CRGB::Black;
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
        fill_solid(ledsLeft,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsRight, LEDS_PER_SIDE, CRGB::Black);

        for (int t = 0; t < TAIL; t++) {
            int idx = pos - t;
            if (idx >= 0 && idx < LEDS_PER_SIDE) {
                uint8_t bright = 255 - (uint8_t)(t * DIM_STEP);
                ledsLeft[idx]  = CRGB(bright, bright, bright);
                ledsRight[idx] = CRGB(bright, bright, bright);
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
        fill_solid(ledsLeft,  LEDS_PER_SIDE, cols[i]);
        fill_solid(ledsRight, LEDS_PER_SIDE, cols[i]);
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
            ledsLeft[i]  = CHSV(hueL, 230, 200);
            ledsRight[i] = CHSV(hueR, 230, 200);
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
            ledsLeft[rIdx]  = CRGB(0, b, b);
            ledsRight[rIdx] = CRGB(0, b, b);
        }
        if (lIdx >= 0 && lIdx < LEDS_PER_SIDE) {
            ledsLeft[lIdx]  = CRGB(0, b, b);
            ledsRight[lIdx] = CRGB(0, b, b);
        }
    };

    // Outward — from centre toward both ends
    for (int head = 0; head <= CENTER + TAIL; head++) {
        fill_solid(ledsLeft,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsRight, LEDS_PER_SIDE, CRGB::Black);
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
        fill_solid(ledsLeft,  LEDS_PER_SIDE, CRGB::Black);
        fill_solid(ledsRight, LEDS_PER_SIDE, CRGB::Black);
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
            ledsLeft[i]  = ((i + step)          % 3 == 0) ? CRGB(220, 30, 0) : CRGB::Black;
            ledsRight[i] = ((i - step + 3 * 100) % 3 == 0) ? CRGB(220, 30, 0) : CRGB::Black;
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
        fill_solid(ledsLeft,  LEDS_PER_SIDE, col);
        fill_solid(ledsRight, LEDS_PER_SIDE, col);
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
        fill_solid(ledsLeft,  LEDS_PER_SIDE, col);
        fill_solid(ledsRight, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    // White burst (250 ms)
    for (int t = 0; t < 250; t += frameMs) {
        uint8_t p = (uint8_t)(255 * t / 250);
        CRGB col(255, p, p);
        fill_solid(ledsLeft,  LEDS_PER_SIDE, col);
        fill_solid(ledsRight, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    // Fade out (450 ms)
    for (int t = 0; t < 450; t += frameMs) {
        uint8_t p = 255 - (uint8_t)(255 * t / 450);
        CRGB col(p, p, p);
        fill_solid(ledsLeft,  LEDS_PER_SIDE, col);
        fill_solid(ledsRight, LEDS_PER_SIDE, col);
        FastLED.show();
        delay(frameMs);
    }
    FastLED.clear(true);
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
    Serial.begin(115200);

    // ── Boot reason + NVS fault counter ──────────────────────────────────────
    // Must run before any Serial output that overwrites the reason line.
    logAndCountReset();

    // ── Graceful LED blank on any future reset ────────────────────────────────
    // The handler clears both panels before the MCU restarts, so a WDT or
    // panic does not leave LEDs frozen on a random animation frame.
    esp_register_shutdown_handler(onSystemShutdown);

    // Register LED panels with FastLED
    FastLED.addLeds<LED_CHIPSET, PIN_LED_LEFT,  LED_COLOR_ORDER>(ledsLeft,  LEDS_PER_SIDE);
    FastLED.addLeds<LED_CHIPSET, PIN_LED_RIGHT, LED_COLOR_ORDER>(ledsRight, LEDS_PER_SIDE);
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

    // Initialise animation registry
    AnimationRegistry::init();

    // Initialise optocoupler inputs (sets pin modes — must run before self-test)
    inputs.begin();

    // ── Startup self-test ────────────────────────────────────────────────────
    // Runs the segment ID flash, pixel chaser, and colour verify.
    // Takes ~5 seconds on first boot; harmless to have on every power cycle.
    runStartupSelfTest();

    // ── Animation demo ───────────────────────────────────────────────────────
    // Five creative scenes that play once after the self-test.
    // Total duration ≈ 9 seconds.  Safe to remove if boot time matters.
    runAnimationDemo();
    // ────────────────────────────────────────────────────────────────────────

    // Initialise taillight panels (sets idle animation)
    leftPanel.begin();
    rightPanel.begin();

    // Initialise CAN bus (MCP2515 via SPI)
    canBus.begin();

    // Initialise thermal manager (reads initial die temperature)
    thermal.begin();

    // Initialise undervoltage lockout (reads initial rail voltage)
    voltMon.begin();
    Serial.printf("[uvlo] rail at boot: %.2f V\n", voltMon.voltageV());
    Serial.printf("[uvlo] state: %s\n", voltMon.isReady() ? "READY" : "LOCKED");

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

    // ── Undervoltage lockout ────────────────────────────────────────────────
    voltMon.tick(nowMs);
    if (!voltMon.isReady()) {
        // Rail too low (e.g. mid-crank). Blank LEDs once on the transition into
        // the locked state, then just return. Calling FastLED.show() every loop
        // iteration would hammer the SPI bus at full CPU speed.
        if (vmonWasReady) {
            FastLED.clear(true);  // blank + show once
            vmonWasReady = false;
        }
        return;
    }
    vmonWasReady = true;
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
    const uint8_t ls = inputs.leftSnapshot();
    const uint8_t rs = inputs.rightSnapshot();

    LightState leftState = resolveSideState(
        ls & 0x01, ls & 0x02, ls & 0x04, ls & 0x08,
        rs & 0x04
    );
    LightState rightState = resolveSideState(
        rs & 0x01, rs & 0x02, rs & 0x04, rs & 0x08,
        ls & 0x04
    );

    // ── CAN bus tick (TX broadcast + RX command processing) ─────────────────
    canBus.tick(leftState, rightState, inputs, thermal);

    // Apply animation override if commanded over CAN
    if (canBus.hasOverride()) {
        leftState  = canBus.overrideLeft();
        rightState = canBus.overrideRight();
    }

    // Throttle animation updates to FRAME_INTERVAL_MS (~60 fps)
    if (nowMs - lastFrameMs >= FRAME_INTERVAL_MS) {
        lastFrameMs = nowMs;

        leftPanel.update(leftState, nowMs);
        rightPanel.update(rightState, nowMs);

        FastLED.show();
    }
}
