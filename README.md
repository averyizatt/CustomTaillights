# Custom Taillights

Custom taillight controller for the **Fox Body Mustang** using an **ESP32-S3** and custom WS2812B LED assemblies — one per side, each made of three chained serpentine segments (380 LEDs total per side).  
Stock 12 V signals are read through two independent **4-channel optocoupler modules** (one per side) for full electrical isolation.  
A **MCP2515 CAN bus interface** allows other ECUs to monitor state and issue commands.  
Built with **PlatformIO**, **FastLED**, and **autowp-mcp2515**.

---

## Hardware

| Item | Detail |
|------|--------|
| MCU | ESP32-S3 DevKitC-1 |
| LEDs per side | 380 × WS2812B (3 chained segments) |
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
| Left panel DIN | 20 |
| Right panel DIN | 19 |
| Left brake (opto O1) | 5 |
| Left running/park (opto O2) | 6 |
| Left turn (opto O3) | 4 |
| Left reverse (opto O4) | 7 |
| Right brake (opto O1) | 46 |
| Right running/park (opto O2) | 9 |
| Right turn (opto O3) | 3 |
| Right reverse (opto O4) | 10 |
| CAN SCK | 12 |
| CAN MOSI | 13 |
| CAN MISO | 14 |
| CAN CS | 15 |
| CAN INT | 16 |

Optocoupler outputs pull their GPIO **LOW** when the stock 12 V signal is active; all input pins use `INPUT_PULLUP`.  
All assignments and timing constants are centralised in `src/config.h`.

---

## Project structure

```
CustomTaillights/
├── platformio.ini           # PlatformIO build config (ESP32-S3, FastLED, MCP2515)
└── src/
    ├── main.cpp             # setup() / loop() entry point
    ├── config.h             # Pin defs, segment constants, timing, CAN IDs, thermal thresholds
    ├── states.h             # LightState enum + resolveSideState()
    ├── inputs.h / .cpp      # Debounced dual-4-channel optocoupler reader (atomic snapshots)
    ├── taillight.h / .cpp   # Per-side LED panel controller (segment-aware pixel writes)
    ├── animations.h / .cpp  # Animation base class + built-in animations
    ├── canbus.h / .cpp      # MCP2515 CAN TX/RX — state broadcast & command handling
    ├── thermal.h / .cpp     # ESP32-S3 die-temperature monitor with brightness derating
    ├── faults.h             # Diagnostic fault codes broadcast on CAN ID 0x102
    └── font5x.h / .cpp      # 5-pixel-tall bitmap font for matrix text rendering
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

## CAN bus interface

The MCP2515 runs at **500 kbps** by default. Three CAN frame IDs are used:

| ID | Direction | Purpose |
|----|-----------|---------|
| `0x100` | TX | State broadcast — left/right `LightState`, raw input flags, die temp, derate amount |
| `0x101` | RX | Commands — brightness override, animation override, clear override |
| `0x102` | TX | Fault frames — fault code, severity, and up to 2 data bytes |

### Command frame format (ID `0x101`)

| Byte 0 (cmd) | Payload | Effect |
|-------------|---------|--------|
| `0x01` | Byte 1 = brightness (0–255) | Set global LED brightness |
| `0x02` | Byte 1 = left state, Byte 2 = right state | Force animation override |
| `0x03` | — | Clear animation override |

### Fault codes (ID `0x102`)

| Code | Name | Severity | Notes |
|------|------|----------|-------|
| `0x01` | `FAULT_THERMAL_WARN` | WARNING | Die temp ≥ `TEMP_DERATE_START_C` (65 °C) |
| `0x02` | `FAULT_THERMAL_CRITICAL` | CRITICAL | Die temp ≥ `TEMP_SHUTDOWN_C` (85 °C) |
| `0x03` | `FAULT_CAN_BUS_OFF` | CRITICAL | MCP2515 bus-off condition |
| `0x04` | `FAULT_INPUT_STUCK_BOOT` | WARNING | Input active at power-on (possible short) |
| `0x05` | `FAULT_WDT_RESET` | WARNING | Watchdog reset on previous boot |
| `0x06` | `FAULT_PANIC_RESET` | CRITICAL | Panic/exception reset on previous boot |
| `0x07` | `FAULT_BROWNOUT_RESET` | WARNING | Brownout/power-loss reset on previous boot |

---

## Thermal management

The ESP32-S3 on-die temperature sensor is polled each loop iteration. Brightness is linearly derated between **65 °C** and **80 °C**, and hard-clamped to `BRIGHTNESS_MIN_SAFETY` (30) above **85 °C** — LEDs stay on at minimum brightness so brake and turn signals remain visible even during thermal shutdown.

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
