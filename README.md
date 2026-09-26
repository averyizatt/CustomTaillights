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

The custom PCB uses **active-LOW** optocoupler inputs with `INPUT_PULLUP`: HIGH is idle, LOW is active. The original DevKit profile retains active-HIGH inputs with `INPUT_PULLDOWN`.
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

### BLE disabled
BLE is disabled and de-initialized at the start of `setup()` to free heap and remove Bluetooth RF stack timing overhead. WiFi remains available for the web settings UI.

### LED timing hardening
- All LED hardware pushes route through a centralized output helper.
- Hardware updates are limited to **40 FPS max** (`LED_SHOW_MIN_INTERVAL_MS = 25`), allowing both 380-pixel panels to transmit sequentially.
- The output adapter uses a shared ESP32-S3 RMT5 DMA channel. Mixing legacy RMT4 with Arduino 3.x's new driver aborts before `setup()`.

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
| Yellow | Solid | Temperature derating active (die ≥ 75 °C) |
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
| `0x01` | `FAULT_THERMAL_WARN` | WARNING | Die temp ≥ `TEMP_DERATE_START_C` (75 °C) |
| `0x02` | `FAULT_THERMAL_CRITICAL` | CRITICAL | Die temp ≥ `TEMP_SHUTDOWN_C` (85 °C) |
| `0x03` | `FAULT_CAN_BUS_OFF` | CRITICAL | MCP2515 bus-off condition |
| `0x04` | `FAULT_INPUT_STUCK_BOOT` | WARNING | Input active at power-on (possible wiring short) |
| `0x05` | `FAULT_WDT_RESET` | WARNING | Watchdog reset on previous boot |
| `0x06` | `FAULT_PANIC_RESET` | CRITICAL | Panic/exception reset on previous boot |
| `0x07` | `FAULT_BROWNOUT_RESET` | WARNING | Brownout/power-loss reset on previous boot |

Fault frames are reported once on state change — not spammed every broadcast cycle. Boot faults (WDT, panic, brownout from the *previous* session) are re-reported after CAN comes online so a monitoring ECU always sees them.

---

## Thermal management

The ESP32-S3 on-die temperature sensor is sampled every 5 seconds. Brightness is linearly derated between **75 °C** and **80 °C**, and hard-clamped to `BRIGHTNESS_MIN_SAFETY` (30) above **85 °C** — LEDs stay on at minimum brightness so brake and turn signals remain visible even at shutdown temperature. Thermal derating is never bypassed by a CAN brightness command.

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

### Custom PCB build

The only main environment is `esp32-s3-pcb`; an unqualified build/upload selects
the custom PCB pinout and enables its onboard MCP2515. Host tests use the
separate `platformio-tests.ini` configuration.
It uses the same 21x5 top strip, 21x5 bottom strip, 17x10 main panel,
380-LED-per-side buffers, serpentine mapping, mirroring, and animations as the
original build:

```bash
pio run -e esp32-s3-pcb
pio run -e esp32-s3-pcb --target upload
```

PCB mapping used by this build:

| PCB net | GPIO | Firmware use |
|---------|------|--------------|
| LEDDATA1 / LEDDATA3 | 4 / 6 | Driver / passenger taillight |
| LEDDATA2 | 5 | Spare LED data output (`PIN_LED_AUX`), unused |
| OPTOGPIO1 / 2 | 7 / 15 | Shared running / brake input |
| OPTOGPIO3 / 4 | 16 / 17 | Driver / passenger turn input |
| OPTOGPIO5 | 18 | Shared reverse input |
| OPTOGPIO6 | 8 | Spare opto input (`PIN_OPTO_AUX`); diagnostics only |
| CAN_CS / MISO / MOSI / SCK | 38 / 37 / 40 / 41 | MCP2515 (SO = MISO, SI = MOSI) |
| CAN_INT | Disconnected | MCP2515 is polled; original GPIO35 trace is isolated |
| SPARE1 / SPARE2 | 46 / 9 | General-purpose spare I/O |

The temporary driver-to-DATA2 debugging mirror has been removed.
The spare outputs remain unconfigured. OPTO6 is read with an input pull-up for
diagnostics but selects no lighting function. If the PCB connector wiring assigns the
six opto channels differently, only the PCB block in `src/config.h` needs to be
reordered.

FastLED 3.10.3 still supplies colors, brightness, and power limiting. The custom
output adapter in `src/led_transport.cpp` replaces its asynchronous strip driver.
It uses ESP32-S3 SPI3 DMA for LED output, separate from CAN's SPI2 peripheral.
`platformio.ini` pins the tested Arduino 3.3.8 / IDF 5.5.4 toolchain so a fresh
installation selects the same compatible APIs.

Each complete frame is encoded before transmission, using 100/110 SPI bit
patterns at 2.5 MHz, as in Espressif's WS2812 SPI driver. The 3612-byte lamp
packet includes 307.2 us LOW before and after the pixels and fits in one DMA
descriptor. No interrupt-driven refill is needed during the waveform, unlike
the previous RMT ping-pong buffer. Only MOSI is routed to the current LED pin;
no SPI clock or chip-select pin is driven. Transfers finish before switching
to the next output, and idle outputs are disconnected and driven LOW without
using the pull-up-enabling GPIO reset function.

Completion waits are bounded to 30 ms. After a timeout, the transaction and
buffer remain untouched until a nonblocking completion check succeeds; the
next frame can then resume safely. Initialization failures leave the web UI
available and are exposed through the per-output error counters.

Legacy RMT remains prohibited. Rendering/transmission share one frame limiter.
Physical flicker and connector output still need confirmation on the controller;
successful transfer counters describe driver completion, not measured LED light.

The preview page's Diagnostics now shows each rendered state, non-black buffer
pixel count, completed transfers, and latest output error. The same data appears
as `output_*` fields in `GET /api/settings`, including GPIO assignments and
`output_transport` (`spi3-dma-full-frame`), also shown in Preview Diagnostics
under LED Transport to identify this firmware. Preview requests blocked by a physical
brake/reverse signal return HTTP 409 with an explanation instead of reporting
success. Software input buttons enable input testing and exit show/preview modes;
held software turn buttons directly request TURN (both together request HAZARD).
Explicit previews can override software tests, while physical brake/reverse
protection remains active. Web requests time out after four seconds.

Preview also contains **PCB Input Diagnostics** with raw HIGH/LOW readings for
OPTO1..OPTO6 (GPIO7, 15, 16, 17, 18, 8). Its six-digit snapshot runs left-to-right
from OPTO1 to OPTO6, with 1 meaning electrical HIGH, independent of debounce or
the assumed active polarity. Record all-off, running-only, brake-only, and
reverse-only snapshots to identify mapping and polarity before changing them.
The API exposes these as `pcb_input_pins`, `pcb_input_raw_levels`, and
`input_active_level`. Interpreted input masks now include signal names in the UI.
The confirmed PCB mapping is running GPIO7, brake GPIO15, driver turn GPIO16,
passenger turn GPIO17, and reverse GPIO18, all active LOW. With every signal off,
the raw snapshot should be `111111` and both interpreted masks should be `0x00`.
This also removes the false brake/reverse condition that blocked Wi-Fi previews;
an actually active brake or reverse signal still takes priority.

The MCP2515 starts in normal one-shot mode at 500 kbit/s with an 8 MHz oscillator.
Failed transmissions back off from 1 to 10 seconds, with one log on entering
backoff and one on recovery. TX completion is checked asynchronously, stalled
requests are aborted after 20 ms, and receive commands remain available during
TX backoff. Telemetry and fault frames share this bounded, best-effort path.
No controller resets or mode-change waits run in the lighting loop. A controller
that fails initialization stays disabled until restart; an initialized controller
can resume transmission when the CAN network becomes available without rebooting.
Normal startup and fault reporting remain available over Serial at 115200 baud.
Temporary CAN loopback/health diagnostics and optocoupler change logging have
been removed. OPTO6 remains unassigned. Thermal brightness protection is active:
dim above 75 C, maximum derating at 80 C, and safety-minimum brightness at 85 C.

### Bench self-test mode

Hold the **driver-side running/park** optocoupler input active while applying power. The controller will run a full segment ID flash, pixel chaser, and RGB colour verify (~5 s), followed by a full light-state cycle. Release the line to resume normal operation. This mode is intentionally skipped on every normal in-car boot so brake lights are live as quickly as possible.

## Integrated CAN schema 2

CAN is enabled in the integrated build. The actual firmware consumes the shared
contract and accepts command `0x05` for stock/sequential/show/demo and all 33
show options. Update this firmware, the main controller, and the Nano together.
See [CAN compatibility audit](../../docs/CAN_COMPATIBILITY_AUDIT.md) for wire
formats, changes, and hardware validation.
### Shared CAN compatibility

The schema 2 CCM contract is vendored in `include/can_contract/can_protocol.h`
so this repository also builds standalone. The integration checker verifies it
matches the comfort controller's canonical header. IDs remain 0x100/0x101/0x102
at 500 kbit/s. The seven state bytes are left state, right state, driver inputs,
passenger inputs, applied brightness, raw Celsius (0..255), and derating (0..255).
Update the comfort controller and taillights together.

CAN brightness overrides persist until replaced or restarted and always pass
through thermal limiting. Command 0x05 selects stock/sequential/show/demo;
command 0x03 clears animation overrides and stops show/demo mode. CAN transport
uses bounded polling and one-shot transmission so missing ACKs do not block lights.

### Matrix animation collection

Eight optional effects use the existing two 21x5 strips and 17x10 panel per side.
The passenger lamp mirrors through `TailLight::setPixel`, and the lower two
segments retain their red-diffuser filtering. Existing IDs and default selections
are unchanged.

| Menu | ID | Name | Behavior |
| --- | --- | --- | --- |
| Running | 4 | Contour Glide | Stable outlines with a slow perimeter highlight |
| Running | 5 | Fox Louvers | Three raked blades and strip rails with a soft sheen |
| Brake | 6 | Edge Lock | Immediate bright fill; edges settle inward to full solid within 240 ms |
| Turn | 6 | Arrowhead Sweep | Inboard-to-outboard chevron fill, hold, then off |
| Turn | 7 | Three-Bar Relay | Three outboard-latching groups, hold, then off |
| Show / Preview | 33 | Afterburner | Warm exhaust rings across the three matrices |
| Show / Preview | 34 | Tunnel Grid | Perspective gates and converging guide rails |
| Show / Preview | 35 | Apex Weave | Interlaced diagonal ribbons with alternating crossings |

New running effects obey the running brightness cap. The new turn effects keep
both lower red sections solid during BRAKE_TURN and animate the clear top strip.
Show/preview effects retain the existing brake and reverse override rules. All
output still passes through the thermal and LED power limits.

Native geometry and rendering tests plus an exporter for a standalone animated
preview are documented in `test/test_matrix_animations/README.md`. The preview
is generated from the actual C++ effects and pixel mapping, with schematic lens
colors rather than a physical optical simulation.

### WiFi tuning and profiles

- **Apply** uses the current form immediately without writing startup settings.
- **Save** applies the form and remembers it across power cycles.
- **Revert** restores the last saved settings and exits show mode.
- Editing controls alone does not apply or save lighting changes. Diagnostics
  continue refreshing while edits stay in the form.

The Display tab has six named profile slots stored on the controller, available
from any connected phone. **Save Profile** captures the lighting values in the
form, including edits you have not applied. **Load** applies the selected profile
temporarily; use the bottom **Save** button to make it the startup configuration.
Profiles include colors, brightness, animation styles/speeds, turn timing, rest
mode, and show effect/text choices. They exclude WiFi credentials, lens layout,
startup animation, show-mode activation, and preview/software input overrides.
Saving or deleting a profile does not change the active lights. Reset Settings
restores saved defaults without deleting profiles.

Colors provides independent brake, turn/hazard, reverse, and running colors.
Red diffuser sections still filter out green and blue. Animations provides
independent brake, reverse, and running speeds from 50% to 200%; solid styles
remain steady and brake pulse speed is independent of show speed.

Turn timing offers:

- **Simple:** the existing 200-1500 ms blink period with equal on/off halves.
- **Custom:** separate Sweep (50-1500 ms), Full-light hold (0-1500 ms), and Off
  (50-1500 ms) controls. The page shows their total cycle time. Chase styles
  animate during Sweep and fill the turn area during Hold. Simple Flash and
  Hazards stay on for Sweep + Hold. A zero hold skips that phase.

Existing installations default to Simple, preserving their saved blink period.
These controls do not change physical input mapping or debounce. Frame Interval
still controls output smoothness (20 ms = 50 fps).

API: `POST /api/settings` applies supplied fields temporarily by default;
include `"persist": true` to save. `GET /api/settings` includes `settings_pending`.
`POST /api/revert` restores the last successful Save. `GET /api/profiles` lists
six slots; `POST /api/profiles` accepts `action` (`save`, `load`, `delete`) and
`slot` (0-5). Saving also requires a `name` (1-24 UTF-8 bytes) and complete
`settings` matching the lighting fields in `src/lighting_config.h`. Failed
storage writes return an error instead of reporting success.

### Firmware updates over WiFi (OTA)

First install this firmware once by USB using the **esp32-s3-pcb** environment.
It adds both a browser updater and PlatformIO OTA. The existing 8 MB partition
layout already has two 0x330000-byte application slots; no partition migration
or settings/profile erase is needed. Keep the vehicle parked with all physical
light inputs idle and power connected. Light output pauses during the upload.
Updates abort if a physical light input becomes active during transfer.

**From the web app:**

1. Build `esp32-s3-pcb` in PlatformIO.
2. Connect to the controller and open **Network > Firmware Updates** in a normal
   browser. If the captive-portal window cannot select files, open the controller
   IP directly in Chrome/Safari/Edge.
3. Select `.pio/build/esp32-s3-pcb/firmware.bin`, enter the controller's **AP WiFi
   password**, and press **Upload & Restart**. Do not select `firmware.factory.bin`,
   `bootloader.bin`, or `partitions.bin`.
4. Wait for the page to confirm the controller is back online. If WiFi disconnects
   during restart, reconnect to the controller network. Uploading 100% only means
   the file was sent; success is confirmed separately after validation/restart.

The AP password is the one loaded at startup, including when the controller is
on a home network. After changing it, Save and reboot before using the new
password for updates. Both firmware update methods use this password. Firmware
updates preserve saved settings and profiles; temporary Apply changes are lost
on restart.

**From VS Code / PlatformIO:**

Use the main **esp32-s3-pcb > Upload** task for USB uploads.
For WiFi, use the separate `platformio-ota.ini` configuration from a terminal.
It defaults to `192.168.4.1` and prompts for the AP password when interactive
input is available. Alternatively, supply the password to that process:

```powershell
$env:FOXBODY_OTA_PASSWORD = 'your-controller-ap-password'
pio run -c platformio-ota.ini -e esp32-s3-pcb-ota -t upload
```

For a controller on home WiFi, use its displayed IP:

```powershell
pio run -c platformio-ota.ini -e esp32-s3-pcb-ota -t upload --upload-port 192.168.1.123
```

A variable set inside a terminal is available to commands launched from that
terminal; an already-running VS Code task does not automatically inherit it.
The upload helper uses the `espota.py` bundled with the pinned Arduino framework
so its authentication matches the firmware, and passes passwords directly as
arguments without printing them or interpreting shell characters. Allow the
Python uploader through the Windows firewall on your private network if its
return connection is blocked. Neither OTA method needs internet access once the
firmware and development dependencies are available locally.

The updater accepts only complete ESP32-S3 application images carrying this
project's matching PCB/development target marker. The ESP32 updater additionally
validates image integrity before selecting the new boot slot. Interrupted or
rejected transfers do not activate the incomplete image. An application that
passes validation can still have software bugs; automatic rollback after a bad
boot is not configured. USB remains the recovery path if the app cannot start
WiFi. Older builds without the OTA target marker must be installed over USB.

Read-only `GET /api/firmware` reports the build, target, boot ID, inactive-slot
capacity, input state, and last update error. Browser upload is authenticated
`POST /api/firmware` with one multipart field named `firmware` and an exact
`X-Firmware-Size` header; the Basic authentication username is `admin`. These
routes are implemented separately from settings/profile persistence.
