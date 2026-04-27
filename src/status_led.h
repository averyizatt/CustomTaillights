#pragma once

// ---------------------------------------------------------------------------
// status_led.h
// Single onboard WS2812B status indicator.
//
// States (priority highest → lowest when multiple conditions exist):
//
//   THERMAL_SHUTDOWN  solid red         die temp ≥ TEMP_SHUTDOWN_C (85 °C)
//   THERMAL_WARN      solid yellow      brightness derating active (≥ 65 °C)
//   CAN_OFFLINE       orange blink      MCP2515 offline or bus-off
//   FAULT_HISTORY     fast red blink    prior WDT/panic resets in NVS
//   OK                solid green       all systems normal
//   BOOT              slow blue blink   startup / crank holdoff
//
// Usage (main.cpp):
//   1. Declare: StatusLed statusLed;
//   2. Register with FastLED BEFORE calling begin():
//        auto& ctrl = FastLED.addLeds<WS2812B, PIN_STATUS_LED, GRB>
//                               (&statusLed.pixel, 1);
//        ctrl.setScale(STATUS_LED_BRIGHT);
//   3. statusLed.begin();
//   4. In loop(): statusLed.setState(desired); (then FastLED.show() handles it)
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <FastLED.h>

enum class StatusLedState : uint8_t {
    BOOT,              // slow blue blink  — startup / holdoff
    OK,                // solid green      — all systems normal
    CAN_OFFLINE,       // orange blink     — MCP2515 offline / bus-off
    THERMAL_WARN,      // solid yellow     — brightness derating active
    THERMAL_SHUTDOWN,  // solid red        — thermal safety limit hit
    FAULT_HISTORY,     // fast red blink   — prior crash(es) in NVS log
};

class StatusLed {
public:
    // Call once in setup() after FastLED.addLeds has been called for this pixel.
    void begin();

    // Set a new state.  No-op if already in that state (preserves blink phase).
    void setState(StatusLedState s);

    StatusLedState state() const { return _state; }

    // Call every frame before FastLED.show().  Updates `pixel`; the next
    // FastLED.show() call flushes it to the hardware.
    void tick(unsigned long nowMs);

    // The raw pixel — passed directly to FastLED.addLeds in main.cpp.
    CRGB pixel = CRGB::Black;

private:
    StatusLedState _state   = StatusLedState::BOOT;
    unsigned long  _lastMs  = 0;
    bool           _blinkOn = false;
};
