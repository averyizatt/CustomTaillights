#pragma once

// ---------------------------------------------------------------------------
// config.h
// Board-level pin assignments and LED segment constants.
// All hardware-specific values live here so nothing else needs to change
// when the wiring is revised.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <can_contract/can_protocol.h>

// ── LED segments ─────────────────────────────────────────────────────────────
//
// Each physical taillight (left or right) is made of THREE chained segments:
//
//   Segment 0 — SEG_TOP_STRIP : 21 cols × 5 rows = 105 LEDs
//     Wiring: row-major serpentine.
//     Driver side  : data-in top-RIGHT (inner edge, row 0 right→left).
//     Passenger side: data-in top-LEFT  (inner edge, row 0 left→right).
//     Both sides are physical mirrors of each other about the car centre-line.
//
//   Segment 1 — SEG_BOT_STRIP : 21 cols × 5 rows = 105 LEDs
//     Same wiring as SEG_TOP_STRIP (chained from SEG_TOP_STRIP data-out).
//
//   Segment 2 — SEG_MAIN      : 17 cols × 10 rows = 170 LEDs
//     Wiring: row-major serpentine starting at the bottom row,
//             data-in  bottom-left (row 9, col 0)
//             data-out top-left    (row 0, col 0)   ← verified: 10 rows,
//             starting L→R at row 9; row 0 is 9 rows up (odd) → R→L → exits
//             at (0, 0) = top-left. ✓
//
//   Chain order: SEG_TOP_STRIP → SEG_BOT_STRIP → SEG_MAIN
//   Pixel offsets in the buffer:
//     SEG_TOP_STRIP : [  0 ..  104]
//     SEG_BOT_STRIP : [105 ..  209]
//     SEG_MAIN      : [210 ..  379]
//   Total per side  : 380 LEDs

static constexpr int SEG_TOP_STRIP = 0;
static constexpr int SEG_BOT_STRIP = 1;
static constexpr int SEG_MAIN      = 2;
static constexpr int NUM_SEGMENTS  = 3;

static constexpr int STRIP_ROWS  = 5;
static constexpr int STRIP_COLS  = 21;
static constexpr int STRIP_LEDS  = STRIP_ROWS * STRIP_COLS;   // 105

static constexpr int MAIN_ROWS   = 10;
static constexpr int MAIN_COLS   = 17;
static constexpr int MAIN_LEDS   = MAIN_ROWS * MAIN_COLS;     // 170

static constexpr int LEDS_PER_SIDE = 2 * STRIP_LEDS + MAIN_LEDS;  // 380

// Segment start offsets in the per-side pixel buffer
static constexpr int SEG_OFFSET[NUM_SEGMENTS] = {
    0,               // SEG_TOP_STRIP
    STRIP_LEDS,      // SEG_BOT_STRIP  (105)
    2 * STRIP_LEDS,  // SEG_MAIN       (210)
};

// Both the DevKit and custom-PCB targets use this exact physical matrix.
// Fail the build if a later board-specific edit accidentally changes it.
static_assert(NUM_SEGMENTS == 3, "Taillight must have exactly three segments");
static_assert(STRIP_ROWS == 5 && STRIP_COLS == 21 && STRIP_LEDS == 105,
              "Top and bottom strip geometry must remain 21x5 (105 LEDs)");
static_assert(MAIN_ROWS == 10 && MAIN_COLS == 17 && MAIN_LEDS == 170,
              "Main-panel geometry must remain 17x10 (170 LEDs)");
static_assert(SEG_OFFSET[0] == 0 && SEG_OFFSET[1] == 105 && SEG_OFFSET[2] == 210,
              "Taillight segment offsets must remain 0, 105, and 210");
static_assert(LEDS_PER_SIDE == 380,
              "Each taillight must remain exactly 380 LEDs");

// \u2500\u2500 Diffuser types per segment \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
// SEG_TOP_STRIP : clear plastic  \u2014 colours appear true
// SEG_BOT_STRIP : red diffuser   \u2014 only the red channel passes through
// SEG_MAIN      : red diffuser   \u2014 only the red channel passes through
//
// applySegDiffuser() is called automatically by TailLight's pixel-writing
// methods, so animations can write intended colours without worrying about
// which diffuser sits in front of the segment they are targeting.
#ifdef FASTLED_VERSION
inline CRGB applySegDiffuser(int seg, CRGB c) {
    if (seg == SEG_BOT_STRIP || seg == SEG_MAIN) {
        return CRGB(c.r, 0, 0);  // red diffuser: only red channel passes
    }
    return c;  // SEG_TOP_STRIP: clear, no filtering
}
#endif

// Data pins for each side and the spare PCB output.
#if defined(CUSTOM_TAILLIGHTS_PCB)
static constexpr int  PIN_LED_DRIVER    = 4;   // PCB LEDDATA1
static constexpr int  PIN_LED_PASSENGER = 6;   // PCB LEDDATA3
static constexpr int  PIN_LED_AUX       = 5;   // PCB LEDDATA2 (spare)
#else
static constexpr int  PIN_LED_DRIVER    = 20;  // GPIO20 → driver-side taillight DIN
static constexpr int  PIN_LED_PASSENGER = 19;  // GPIO19 → passenger-side taillight DIN
static constexpr int  PIN_LED_AUX       = -1;  // not routed in the DevKit build
#endif

// Global brightness (0-255).  Keep well below 255 to limit current draw.
static constexpr uint8_t BRIGHTNESS_DEFAULT    = 128;
static constexpr uint8_t BRIGHTNESS_DIM        =  25;  // running-light level
static constexpr uint8_t RUNNING_BRIGHTNESS_MAX_PERCENT = 35;
static constexpr uint8_t BRAKE_ANIM_MIN_SCALE  = 180;  // keep brake effects visibly above running

// Per-side brightness trim (0-255, 255 = full, no reduction).
// If one side appears brighter than the other due to LED binning or wiring
// differences, reduce the brighter side's value until both match visually.
// Driver-side is US left; passenger-side is US right.
static constexpr uint8_t BRIGHTNESS_SCALE_DRIVER    = 255;
static constexpr uint8_t BRIGHTNESS_SCALE_PASSENGER = 255;
// Minimum brightness enforced under all fault conditions so safety-critical
// signals (brake, turn) remain visible even if the MCU is overheating.
static constexpr uint8_t BRIGHTNESS_MIN_SAFETY =  30;

// ── Power budget ─────────────────────────────────────────────────────────────
// FastLED measures the estimated draw each frame and scales brightness down
// automatically if the calculated current would exceed the budget.
// WS2812B worst-case: 60 mA per LED at full white × 760 LEDs = 45.6 A.
// Reserve ~500 mA for the ESP32-S3 and logic; assign the rest to LEDs.
static constexpr uint8_t  LED_VOLTAGE         =   5;      // volts (5 V rail)
static constexpr uint32_t LED_POWER_BUDGET_MA = 12000;    // mA  (12.0 A soft cap to reduce supply/transient stress)

// FastLED colour order for these panels
#define LED_COLOR_ORDER GRB
#define LED_CHIPSET     WS2812B

// ── Optocoupler inputs ───────────────────────────────────────────────────────
// Custom PCB: six active-LOW optocouplers, with INPUT_PULLUP at rest.
// Original DevKit: two active-HIGH four-channel modules, INPUT_PULLDOWN.
//
// Signal → opto channel mapping (same on both sides):
//   O1 → Brake
//   O2 → Reverse
//   O3 → Turn
//   O4 → Running/Park
//
// ── Driver side (US left) ─────────────────────────────────────────────────
//   O1 → brake        → GPIO 5
//   O2 → reverse      → GPIO 7
//   O3 → turn         → GPIO 4
//   O4 → running/park → GPIO 6
#if defined(CUSTOM_TAILLIGHTS_PCB)
// Physical connector order for diagnostics, independent of signal assignment.
static constexpr int PCB_OPTO_PINS[6] = {7, 15, 16, 17, 18, 8};
// The PCB exposes six optocoupler outputs. Brake, running and reverse are
// vehicle-wide signals, so each shared input is intentionally used by both
// side state machines. OPTGPIO6 remains available for future use.
static constexpr int PIN_DRIVER_BRAKE   = 15;  // PCB OPTOGPIO2
static constexpr int PIN_DRIVER_RUNNING =  7;  // PCB OPTOGPIO1
static constexpr int PIN_DRIVER_TURN    = 16;  // PCB OPTOGPIO3
static constexpr int PIN_DRIVER_REVERSE = 18;  // PCB OPTOGPIO5
#else
static constexpr int PIN_DRIVER_BRAKE   =  5;
static constexpr int PIN_DRIVER_RUNNING =  6;
static constexpr int PIN_DRIVER_TURN    =  4;
static constexpr int PIN_DRIVER_REVERSE =  7;
#endif

// ── Passenger side (US right) ───────────────────────────────────────────────
// Passenger-side opto
//   O1 → brake        → GPIO 46
//   O2 → reverse      → GPIO 10
//   O3 → turn         → GPIO 3
//   O4 → running/park → GPIO 9
#if defined(CUSTOM_TAILLIGHTS_PCB)
static constexpr int PIN_PASSENGER_BRAKE   = 15;  // shared OPTOGPIO2
static constexpr int PIN_PASSENGER_RUNNING =  7;  // shared OPTOGPIO1
static constexpr int PIN_PASSENGER_TURN    = 17;  // PCB OPTOGPIO4
static constexpr int PIN_PASSENGER_REVERSE = 18;  // shared OPTOGPIO5
static constexpr int PIN_OPTO_AUX           =  8;  // PCB OPTOGPIO6 (spare input)
static constexpr int PIN_SPARE_1            = 46;  // PCB SPARE1
static constexpr int PIN_SPARE_2            =  9;  // PCB SPARE2
#else
static constexpr int PIN_PASSENGER_BRAKE   = 46;
static constexpr int PIN_PASSENGER_RUNNING =  9;
static constexpr int PIN_PASSENGER_TURN    =  3;
static constexpr int PIN_PASSENGER_REVERSE = 10;
static constexpr int PIN_OPTO_AUX           = -1;
static constexpr int PIN_SPARE_1            = -1;
static constexpr int PIN_SPARE_2            = -1;
#endif

// Logic level when the stock signal is ACTIVE.
// Input pulls in Inputs::begin() must bias the pin to the opposite (idle) level.
#ifndef HIGH
static constexpr int HIGH = 1;
#endif
#ifndef LOW
static constexpr int LOW = 0;
#endif
#if defined(CUSTOM_TAILLIGHTS_PCB)
static constexpr int OPT_ACTIVE_LEVEL = LOW;
#else
static constexpr int OPT_ACTIVE_LEVEL = HIGH;
#endif

// Debounce time in milliseconds
static constexpr unsigned long DEBOUNCE_MS      = 20;  // turn
static constexpr unsigned long DEBOUNCE_FAST_MS =  5;  // brake, reverse — 5 ms is
                                                        // imperceptible but rejects
                                                        // automotive contact bounce
static constexpr unsigned long DEBOUNCE_RUNNING_MS = 35; // extra filtering for noisy running-light feeds

// Turn / hazard blink validation.  A steady ON turn input is not a blink; it
// must keep producing transitions in the expected automotive flasher range.
static constexpr unsigned long BLINK_MIN_EDGE_MS  = 120;
static constexpr unsigned long BLINK_MAX_EDGE_MS  = 900;
static constexpr unsigned long BLINK_EXPIRE_MS    = 1800;
static constexpr unsigned long HAZARD_SYNC_MS     = 150;

// ── Real-time safety infrastructure ─────────────────────────────────────────
// Hardware Task Watchdog.  Both Core 0 (input task) and Core 1 (render task)
// must call esp_task_wdt_reset() within this window, or the MCU performs a
// clean panic reset and restarts from setup().
static constexpr uint32_t WDT_TIMEOUT_S = 3;

// Input task poll period (ms).  High-priority FreeRTOS task pinned to Core 0
// so it is completely independent of the LED render time on Core 1.
static constexpr uint32_t INPUT_TASK_PERIOD_MS = 1;

// ── Thermal management ───────────────────────────────────────────────────────
// Temperature thresholds use the ESP32-S3 on-die sensor (temperatureRead()).
// The sensor reads ~5–10 °C above ambient inside the package under load;
// these thresholds are chosen conservatively for an enclosed automotive install.
static constexpr int   TEMP_SAMPLE_INTERVAL_MS = 5000;  // how often to sample
static constexpr int   TEMP_DERATE_START_C     =   75;  // begin linear brightness reduction
static constexpr int   TEMP_DERATE_END_C       =   80;  // max derating applied here
static constexpr int   TEMP_SHUTDOWN_C         =   85;  // above this: BRIGHTNESS_MIN_SAFETY

// ── MCP2515 CAN bus (SPI) ────────────────────────────────────────────────────
// Disabled by default so the taillight controller still boots and renders
// signals on the bench if the MCP2515 module is missing, unpowered, or wired
// differently. Set true only after CAN hardware is installed and verified.
#if defined(CUSTOM_TAILLIGHTS_PCB)
static constexpr bool CAN_ENABLED = true;
#else
static constexpr bool CAN_ENABLED = false;
#endif

// The common blue MCP2515 breakout connects to a custom SPI bus so it does
// not conflict with any other peripheral.
//
//   MCP2515 pin → ESP32-S3 GPIO
//   SCK         → GPIO 12
//   SI (MOSI)   → GPIO 13
//   SO (MISO)   → GPIO 14
//   CS          → GPIO 15
//   INT         → GPIO 16   (active-LOW; pulled high on the module)
//   VCC         → 5 V  (most modules have a 3.3 V regulator on board)
//   GND         → GND
//
// CAN bus bitrate — 500 kbit/s is standard for most automotive applications.
// Change to CAN_250KBPS if the network is running at 250 kbit/s.
#if defined(CUSTOM_TAILLIGHTS_PCB)
static constexpr int PIN_CAN_SCK  = 41;  // MCP2515 SCK
static constexpr int PIN_CAN_MOSI = 40;  // MCP2515 SI (controller input)
static constexpr int PIN_CAN_MISO = 37;  // MCP2515 SO (controller output)
static constexpr int PIN_CAN_CS   = 38;  // MCP2515 CS
static constexpr int PIN_CAN_INT  = -1;  // PCB rework: GPIO35 isolated; driver polls MCP2515
#else
static constexpr int PIN_CAN_SCK  = 12;
static constexpr int PIN_CAN_MOSI = 13;
static constexpr int PIN_CAN_MISO = 14;
static constexpr int PIN_CAN_CS   = 15;
static constexpr int PIN_CAN_INT  = 16;
#endif

// ── CAN message IDs ──────────────────────────────────────────────────────────
// 11-bit standard frame IDs used by this controller.
//
//  0x100  (TX) — periodic taillight state broadcast (every 100 ms)
//  0x101  (RX) — command frame addressed to this ECU
//  0x102  (TX) — diagnostic fault broadcast (on-demand, not periodic)
static constexpr uint32_t CAN_ID_STATE_BROADCAST = can_protocol::ID_TAILLIGHT_STATE;
static constexpr uint32_t CAN_ID_COMMAND         = can_protocol::ID_TAILLIGHT_COMMAND;
static constexpr uint32_t CAN_ID_FAULT           = can_protocol::ID_TAILLIGHT_FAULT;

// How often (ms) the taillight state is broadcast on the bus
static constexpr unsigned long CAN_BROADCAST_INTERVAL_MS = 100;


// Rest-mode UI test pulse timing.
static constexpr unsigned long REST_PULSE_HALF_CYCLE_MS = 300;

// ── Animation timing ─────────────────────────────────────────────────────────
// How often the main loop calls the active animation's update() method.
static constexpr unsigned long FRAME_INTERVAL_MS = 16;   // ~60 fps
// Allow two sequential 380-pixel transmissions plus reset intervals.
static constexpr unsigned long LED_SHOW_MIN_INTERVAL_MS = 25;  // 1000/40

// Turn-signal blink period (total on+off cycle), milliseconds
static constexpr unsigned long TURN_BLINK_PERIOD_MS = 600;

// ── Onboard status LED (WS2812B) ─────────────────────────────────────────────
// A single WS2812B pixel on the DevKit board used as a system health indicator.
//
//   Board                   Onboard LED GPIO
//   ─────────────────────── ────────────────
//   ESP32-C3-DevKitM-1      GPIO 8   ← default
//   ESP32-S3-DevKitC-1 v1.1 GPIO 48
//
// Change PIN_STATUS_LED to match whichever board you are running.
//
// STATUS_LED_BRIGHT sets the per-controller scale (0–255).  It is applied
// independently of the global taillight brightness so thermal derating never
// dims the indicator below a readable level.
static constexpr int     PIN_STATUS_LED    =  48;  // GPIO48 on ESP32-S3-DevKitC-1 v1.1
static constexpr uint8_t STATUS_LED_BRIGHT = 180;  // per-controller cap (0-255)

// How long (ms) to display FAULT_HISTORY blinking after a boot caused by a
// prior WDT or panic reset, before settling to the current health state.
static constexpr unsigned long FAULT_DISPLAY_MS = 10000;

// ── Crank holdoff ────────────────────────────────────────────────────────────
// After every boot the controller waits this long with LEDs blank before
// proceeding to the self-test and main loop.  If voltage sags again during
// engine cranking and the ESP32 browns out, it simply resets and restarts
// the holdoff.  Set to 0 to disable.
static constexpr unsigned long CRANK_HOLDOFF_MS = 1500;
