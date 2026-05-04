// ---------------------------------------------------------------------------
// status_led.cpp
// Single onboard WS2812B status indicator.
// ---------------------------------------------------------------------------

#include "status_led.h"
#include "config.h"

void StatusLed::begin() {
    _state   = StatusLedState::BOOT;
    _lastMs  = 0;
    _blinkOn = false;
    pixel    = CRGB::Black;
}

void StatusLed::setState(StatusLedState s) {
    if (s == _state) return;
    _state   = s;
    _blinkOn = false;  // reset blink phase on state change for clean transitions
    _lastMs  = 0;
}

void StatusLed::tick(unsigned long nowMs) {
    // All pixel values use full range (0–255); STATUS_LED_BRIGHT is applied
    // via nscale8 at the end of this function so the status LED is never
    // dimmed by the global taillight brightness / thermal derating.

    switch (_state) {

        case StatusLedState::BOOT:
            // Slow blue blink: 400 ms on / 400 ms off
            if ((nowMs - _lastMs) >= 400UL) {
                _lastMs  = nowMs;
                _blinkOn = !_blinkOn;
            }
            pixel = _blinkOn ? CRGB(0, 0, 255) : CRGB::Black;
            break;

        case StatusLedState::OK:
            // Solid green
            pixel = CRGB(0, 255, 0);
            break;

        case StatusLedState::CAN_OFFLINE:
            // Orange blink: 300 ms on / 300 ms off
            if ((nowMs - _lastMs) >= 300UL) {
                _lastMs  = nowMs;
                _blinkOn = !_blinkOn;
            }
            pixel = _blinkOn ? CRGB(255, 80, 0) : CRGB::Black;
            break;

        case StatusLedState::THERMAL_WARN:
            // Solid yellow
            pixel = CRGB(255, 200, 0);
            break;

        case StatusLedState::THERMAL_SHUTDOWN:
            // Solid red
            pixel = CRGB(255, 0, 0);
            break;

        case StatusLedState::FAULT_HISTORY:
            // Fast red blink: 150 ms on / 150 ms off
            if ((nowMs - _lastMs) >= 150UL) {
                _lastMs  = nowMs;
                _blinkOn = !_blinkOn;
            }
            pixel = _blinkOn ? CRGB(255, 0, 0) : CRGB::Black;
            break;
    }
    // Cap brightness so the status dot stays dim relative to the taillights.
    // STATUS_LED_BRIGHT is applied here independently of FastLED's global
    // setBrightness() — the nscale8 fires first so the controller sees an
    // already-dimmed value before any further global scaling is applied.
    pixel.nscale8(STATUS_LED_BRIGHT);
}
