#pragma once

// ---------------------------------------------------------------------------
// config.h
// Board-level pin assignments and LED segment constants.
// All hardware-specific values live here so nothing else needs to change
// when the wiring is revised.
// ---------------------------------------------------------------------------

// ── LED segments ─────────────────────────────────────────────────────────────
//
// Each physical taillight (left or right) is made of THREE chained segments:
//
//   Segment 0 — SEG_TOP_STRIP : 21 cols × 5 rows = 105 LEDs
//     Wiring: row-major serpentine, data-in  top-left  (row 0, col  0)
//                                  data-out bottom-left (physical connector
//                                  position; last pixel is row 4, col 20
//                                  per standard L→R serpentine)
//
//   Segment 1 — SEG_BOT_STRIP : 21 cols × 5 rows = 105 LEDs
//     Same wiring as SEG_TOP_STRIP.
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

// Data pins for each side
static constexpr int  PIN_LED_LEFT  = 20;  // GPIO20 → left  taillight DIN
static constexpr int  PIN_LED_RIGHT = 19;  // GPIO19 → right taillight DIN

// Global brightness (0-255).  Keep well below 255 to limit current draw.
static constexpr uint8_t BRIGHTNESS_DEFAULT    = 128;
static constexpr uint8_t BRIGHTNESS_DIM        =  40;  // running-light level
// Minimum brightness enforced under all fault conditions so safety-critical
// signals (brake, turn) remain visible even if the MCU is overheating.
static constexpr uint8_t BRIGHTNESS_MIN_SAFETY =  30;

// ── Power budget ─────────────────────────────────────────────────────────────
// FastLED measures the estimated draw each frame and scales brightness down
// automatically if the calculated current would exceed the budget.
// WS2812B worst-case: 60 mA per LED at full white × 760 LEDs = 45.6 A.
// Reserve ~500 mA for the ESP32-S3 and logic; assign the rest to LEDs.
static constexpr uint8_t  LED_VOLTAGE         =   5;      // volts (5 V rail)
static constexpr uint32_t LED_POWER_BUDGET_MA = 14500;    // mA  (14.5 A of the 15 A supply)

// FastLED colour order for these panels
#define LED_COLOR_ORDER GRB
#define LED_CHIPSET     WS2812B

// ── Optocoupler inputs ───────────────────────────────────────────────────────
// Two 4-channel optocouplers, one piggybacked onto each side's bulb connectors.
// Each coupler output pulls its GPIO LOW when the stock bulb supply is ON.
// All GPIOs are configured INPUT_PULLUP so lines are HIGH (inactive) at rest.
//
// Left-side opto  (GPIO 4-7)
//   CH1 → left  brake bulb        → GPIO 4
//   CH2 → left  running/park bulb → GPIO 5
//   CH3 → left  turn-signal bulb  → GPIO 6
//   CH4 → left  reverse bulb      → GPIO 7
static constexpr int PIN_LEFT_BRAKE   =  4;
static constexpr int PIN_LEFT_RUNNING =  5;
static constexpr int PIN_LEFT_TURN    =  6;
static constexpr int PIN_LEFT_REVERSE =  7;

// Right-side opto (GPIO 8-11)
//   CH1 → right brake bulb        → GPIO 8
//   CH2 → right running/park bulb → GPIO 9
//   CH3 → right turn-signal bulb  → GPIO 10
//   CH4 → right reverse bulb      → GPIO 11
static constexpr int PIN_RIGHT_BRAKE   =  8;
static constexpr int PIN_RIGHT_RUNNING =  9;
static constexpr int PIN_RIGHT_TURN    = 10;
static constexpr int PIN_RIGHT_REVERSE = 11;

// Logic level when the stock signal is ACTIVE (optocoupler pulls low)
static constexpr int OPT_ACTIVE_LEVEL = LOW;

// Debounce time in milliseconds
static constexpr unsigned long DEBOUNCE_MS      = 20;  // turn, running
static constexpr unsigned long DEBOUNCE_FAST_MS =  5;  // brake, reverse — 5 ms is
                                                        // imperceptible but rejects
                                                        // automotive contact bounce

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
static constexpr int   TEMP_DERATE_START_C     =   65;  // begin linear brightness reduction
static constexpr int   TEMP_DERATE_END_C       =   80;  // max derating applied here
static constexpr int   TEMP_SHUTDOWN_C         =   85;  // above this: BRIGHTNESS_MIN_SAFETY

// ── MCP2515 CAN bus (SPI) ────────────────────────────────────────────────────
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
static constexpr int PIN_CAN_SCK  = 12;
static constexpr int PIN_CAN_MOSI = 13;
static constexpr int PIN_CAN_MISO = 14;
static constexpr int PIN_CAN_CS   = 15;
static constexpr int PIN_CAN_INT  = 16;

// ── CAN message IDs ──────────────────────────────────────────────────────────
// 11-bit standard frame IDs used by this controller.
//
//  0x100  (TX) — periodic taillight state broadcast (every 100 ms)
//  0x101  (RX) — command frame addressed to this ECU
static constexpr uint32_t CAN_ID_STATE_BROADCAST = 0x100;
static constexpr uint32_t CAN_ID_COMMAND         = 0x101;

// How often (ms) the taillight state is broadcast on the bus
static constexpr unsigned long CAN_BROADCAST_INTERVAL_MS = 100;

// ── Undervoltage lockout (UVLO) ───────────────────────────────────────────────
// A resistor divider on PIN_VMON converts the car 12 V rail to ADC range.
// Recommended values: R1 = 100 kΩ, R2 = 22 kΩ  (ratio ≈ 0.180)
//   14.4 V (alternator)  →  2.59 V at ADC  ✓
//   16.0 V (load dump)   →  2.88 V at ADC  ✓  (within 3.1 V limit)
//    9.0 V (cranking low) →  1.62 V at ADC
//
// The LEDs are suppressed until the voltage has been above VMON_ON_THRESH_V
// continuously for VMON_STABLE_MS milliseconds.  This lets the MCU boot and
// run the WDT normally during a crank sag without flashing the brake lights.
static constexpr int   PIN_VMON          =  2;     // GPIO2 — ADC1_CH1
static constexpr float VMON_R1_KOHM      = 100.0f; // upper divider leg (kΩ)
static constexpr float VMON_R2_KOHM      =  22.0f; // lower divider leg (kΩ)
static constexpr float VMON_ADC_VREF     =   3.10f; // ESP32-S3 ADC Vref at 11 dB atten
static constexpr float VMON_ON_THRESH_V  =  11.5f; // minimum rail voltage to enable output
static constexpr float VMON_OFF_THRESH_V =  10.5f; // hysteresis: blank LEDs if rail drops here
static constexpr unsigned long VMON_STABLE_MS  = 500;  // must be above ON threshold this long
static constexpr unsigned long VMON_SAMPLE_MS  =  50;  // ADC poll interval

// ── Animation timing ─────────────────────────────────────────────────────────
// How often the main loop calls the active animation's update() method.
static constexpr unsigned long FRAME_INTERVAL_MS = 16;   // ~60 fps

// Turn-signal blink period (total on+off cycle), milliseconds
static constexpr unsigned long TURN_BLINK_PERIOD_MS = 600;
