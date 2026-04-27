# Custom Taillights

Custom taillight controller for the **Fox Body Mustang** using an **ESP32-S3** (or ESP32-C3) and custom WS2812B LED assemblies — one per side, each made of three chained serpentine segments (380 LEDs total per side).  
Stock 12 V signals are read through two independent **4-channel optocoupler modules** (one per side) for full electrical isolation.  
A **MCP2515 CAN bus interface** allows other ECUs to monitor state and issue commands.  
Built with **PlatformIO**, **FastLED**, and **autowp-mcp2515**.

---

## Hardware

| Item | Detail |
|------|--------|
| MCU | ESP32-S3 DevKitC-1 (or ESP32-C3 DevKitM-1) |
| LEDs per side | 380 × WS2812B (3 chained segments) |
| Onboard status LED | 1 × WS2812B (built into DevKit) |
| Input isolation | Two 4-channel optocoupler modules (one per side) |
| CAN interface | MCP2515 via SPI |
| Power | 5 V (LEDs) + 3.3 V (ESP32) |

### LED segment layout (per side)

Each taillight is driven from a single data pin through three chained segments:

| Index | Name | Size | Diffuser | Pixel offset |
|-------|------|------|----------|-------------|
| 0 | `SEG_TOP_STRIP` | 21 × 5 = 105 LEDs | Clear (true colour) | 0 – 104 |
| 1 | `SEG_BOT_STRIP` | 21 × 5 = 105 LEDs | Red (red channel only) | 105 – 209 |
| 2 | `SEG_MAIN` | 17 × 10 = 170 LEDs | Red (red channel only) | 210 – 379 |

The `applySegDiffuser()` helper in `config.h` filters colours automatically so animations can write intended colours without accounting for which diffuser covers a segment.

### Pin assignments

| Signal | GPIO |
|--------|------|
| Driver panel DIN | 20 |
| Passenger panel DIN | 19 |
| Driver brake (opto O1) | 5 |
| Driver running/park (opto O2) | 6 |
| Driver turn (opto O3) | 4 |
| Driver reverse (opto O4) | 7 |
| Passenger brake (opto O1) | 46 |
| Passenger running/park (opto O2) | 9 |
| Passenger turn (opto O3) | 3 |
| Passenger reverse (opto O4) | 10 |
| CAN SCK | 12 |
| CAN MOSI | 13 |
| CAN MISO | 14 |
| CAN CS | 15 |
| CAN INT | 16 |
| Onboard status LED | 48 (S3) / 8 (C3) |

Optocoupler outputs pull their GPIO **HIGH** when the stock 12 V signal is active (`OPT_ACTIVE_LEVEL = HIGH`); all input pins use `INPUT_PULLDOWN`.  
All assignments and timing constants are centralised in `src/config.h`.

---

## Project structure

```
CustomTaillights/
├── platformio.ini            # PlatformIO build config (ESP32-S3, FastLED, MCP2515)
└── src/
    ├── main.cpp              # setup() / loop() entry point
    ├── config.h              # Pin defs, segment constants, timing, CAN IDs, thermal thresholds
    ├── states.h              # LightState enum + resolveSideState()
    ├── inputs.h / .cpp       # Debounced dual-4-channel optocoupler reader (atomic snapshots)
    ├── taillight.h / .cpp    # Per-side LED panel controller (segment-aware pixel writes)
    ├── animations.h / .cpp   # Animation base class + built-in animations
    ├── canbus.h / .cpp       # MCP2515 CAN TX/RX — state broadcast & command handling
    ├── thermal.h / .cpp      # ESP32-S3 die-temperature monitor with brightness derating
    ├── status_led.h / .cpp   # Onboard WS2812B health indicator
    ├── faults.h              # Diagnostic fault codes broadcast on CAN ID 0x102
    └── font5x.h / .cpp       # 5-pixel-tall bitmap font for matrix text rendering
```

---

## Light states

Each side resolves its own `LightState` independently from its four debounced input channels.  
Priority (highest → lowest): **HAZARD > BRAKE_TURN > BRAKE > TURN > REVERSE > RUNNING > OFF**

| State | Description |
|-------|-------------|
| `OFF` | No signals active — all LEDs off |
| `RUNNING` | Parking / running light active — dim red |
| `BRAKE` | Brake pedal pressed — bright red |
| `TURN` | This side's turn signal active — amber sweep |
| `REVERSE` | Reverse gear engaged — white |
| `BRAKE_TURN` | Brake + turn active simultaneously on same side |
| `HAZARD` | Both sides' turn signals active at once — amber flash |

---

## Safety features

### Brake & reverse always win
CAN animation overrides (commands `0x02` and `0x04`) are **never** allowed to suppress an active physical brake or reverse signal. If the optocoupler on either side detects brake or reverse, that side's state is always resolved from hardware — no external command can override it.

### Hardware Task Watchdog
Both the render task (Core 1 / `loop()`) and the input polling task (Core 0) are subscribed to the hardware WDT with a **3-second** timeout. If either task hangs for any reason the MCU performs a clean panic reset and restarts from `setup()`. The crash reason is logged to NVS flash so it survives the reset and can be reported over CAN on the next boot.

### Dual-core architecture
Input polling runs at 1 ms on Core 0, completely independent of LED render time on Core 1. Inputs are exchanged between cores via atomic single-byte snapshots — no mutex or critical section needed.

### Crank holdoff
After every boot the controller waits **1.5 seconds** with LEDs blank before proceeding. If a voltage sag during engine cranking resets the ESP32, it simply restarts the holdoff. Set `CRANK_HOLDOFF_MS` in `config.h` to adjust (or `0` to disable). The onboard status LED blinks blue during this window.

### WiFi & BLE disabled
This device communicates exclusively over CAN bus. WiFi and BLE are disabled at the start of `setup()` to reduce idle current (~80 mA saved), free heap, and eliminate the RF drivers as a crash source in the automotive EMI environment.

### NVS write minimisation
Flash is only written when a fault reset (WDT, panic, brownout) occurs — not on every normal power-on. This keeps NVS erase cycles to a minimum over the life of the device.

---

## Onboard status LED

The DevKit's built-in WS2812B pixel reflects the current system health at a glance. It uses its own FastLED controller scale (`STATUS_LED_BRIGHT = 40`) so it is never dimmed by taillight brightness adjustments or thermal derating.

| Colour | Pattern | Meaning |
|--------|---------|---------|
| Blue | Slow blink (400 ms) | Crank holdoff — waiting for supply to stabilise |
| Green | Solid | All systems normal |
| Orange | Blink (300 ms) | CAN bus offline / MCP2515 not responding |
| Yellow | Solid | Temperature derating active (die ≥ 65 °C) |
| Red | Solid | Thermal shutdown — LEDs at safety minimum (die ≥ 85 °C) |
| Red | Fast blink (150 ms) | Prior WDT/panic resets in NVS — clears after 10 s |
| Red | Solid (brief, on reset) | Watchdog firing / system restarting |

Set `PIN_STATUS_LED` in `config.h` to match your board:

| Board | GPIO |
|-------|------|
| ESP32-S3 DevKitC-1 v1.1 | 48 |
| ESP32-C3 DevKitM-1 | 8 |

---

## CAN bus interface

The MCP2515 runs at **500 kbps** by default. Three CAN frame IDs are used:

| ID | Direction | Purpose |
|----|-----------|---------|
| `0x100` | TX | State broadcast — driver/passenger `LightState`, raw input flags, die temp, derate amount |
| `0x101` | RX | Commands — brightness override, animation override, clear override |
| `0x102` | TX | Fault frames — fault code, severity, and up to 2 data bytes |

### Command frame format (ID `0x101`)

| Byte 0 (cmd) | Payload | Effect |
|-------------|---------|--------|
| `0x01` | Byte 1 = brightness (0–255) | Set global LED brightness |
| `0x02` | Byte 1 = driver state, Byte 2 = passenger state | Force animation override (brake/reverse still wins) |
| `0x03` | — | Clear animation override |
| `0x04` | Byte 1 = custom anim ID | Trigger a custom animation (plays to completion, then auto-clears) |

### Fault codes (ID `0x102`)

| Code | Name | Severity | Notes |
|------|------|----------|-------|
| `0x01` | `FAULT_THERMAL_WARN` | WARNING | Die temp ≥ `TEMP_DERATE_START_C` (65 °C) |
| `0x02` | `FAULT_THERMAL_CRITICAL` | CRITICAL | Die temp ≥ `TEMP_SHUTDOWN_C` (85 °C) |
| `0x03` | `FAULT_CAN_BUS_OFF` | CRITICAL | MCP2515 bus-off condition |
| `0x04` | `FAULT_INPUT_STUCK_BOOT` | WARNING | Input active at power-on (possible wiring short) |
| `0x05` | `FAULT_WDT_RESET` | WARNING | Watchdog reset on previous boot |
| `0x06` | `FAULT_PANIC_RESET` | CRITICAL | Panic/exception reset on previous boot |
| `0x07` | `FAULT_BROWNOUT_RESET` | WARNING | Brownout/power-loss reset on previous boot |

Fault frames are reported once on state change — not spammed every broadcast cycle. Boot faults (WDT, panic, brownout from the *previous* session) are re-reported after CAN comes online so a monitoring ECU always sees them.

---

## Thermal management

The ESP32-S3 on-die temperature sensor is sampled every 5 seconds. Brightness is linearly derated between **65 °C** and **80 °C**, and hard-clamped to `BRIGHTNESS_MIN_SAFETY` (30) above **85 °C** — LEDs stay on at minimum brightness so brake and turn signals remain visible even at shutdown temperature. Thermal derating is never bypassed by a CAN brightness command.

---

## Boot sequence

```
Power on
  │
  ├─ WiFi / BLE disabled
  ├─ Boot reason read; WDT/panic/brownout count written to NVS
  ├─ Shutdown handler registered (blanks LEDs before any reset)
  ├─ FastLED initialised, LEDs blanked
  │
  ├─ Crank holdoff (1500 ms, status LED blinks blue)
  │    └─ If brownout occurs during crank → reset → holdoff restarts
  │
  ├─ [Bench mode only — hold driver running line at power-on]
  │    └─ Full startup self-test (segment ID, pixel chaser, colour verify) ~5 s
  │
  ├─ Startup animation (~900 ms sequential red sweep)
  │
  ├─ [Bench mode only]
  │    └─ Test cycle — cycles through all light states
  │
  ├─ Taillight panels initialised
  ├─ CAN bus online — boot faults broadcast
  ├─ Thermal manager started
  ├─ Hardware WDT armed (3 s timeout, both cores)
  ├─ Input task launched on Core 0 (1 ms poll, priority 4)
  │
  └─ Main loop running (Core 1)
```

---

## Adding a new animation

1. Subclass `Animation` in `animations.h` / `animations.cpp`:

```cpp
class AnimMyEffect : public Animation {
public:
    void begin(TailLight& side, LightState state) override;   // optional
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
    void end(TailLight& side) override;                        // optional
};
```

2. Register it in `AnimationRegistry::get()` for the desired `LightState`.

Animations write colours using the segment-aware `TailLight` API; `applySegDiffuser()` is called automatically so you can use true colours regardless of which physical diffuser covers the target segment.

---

## Building & flashing

```bash
# Install PlatformIO CLI first, then:
cd CustomTaillights
pio run                    # compile
pio run --target upload    # flash over USB
pio device monitor         # open serial monitor (115200 baud)
```

### Bench self-test mode

Hold the **driver-side running/park** optocoupler input active while applying power. The controller will run a full segment ID flash, pixel chaser, and RGB colour verify (~5 s), followed by a full light-state cycle. Release the line to resume normal operation. This mode is intentionally skipped on every normal in-car boot so brake lights are live as quickly as possible.
