#pragma once

#include <cmath>
#include <can_contract/can_protocol.h>

// Schema 2 adapter shared by production telemetry and host tests.
namespace taillight_can {
inline can_protocol::CanFrame encodeState(uint8_t left, uint8_t right,
                                         uint8_t driverFlags, uint8_t passengerFlags,
                                         uint8_t brightness, float temperatureC,
                                         uint8_t derate) {
    can_protocol::TaillightState state{};
    state.left_state = left;
    state.right_state = right;
    state.driver_input_flags = driverFlags;
    state.passenger_input_flags = passengerFlags;
    state.brightness = brightness;
    const float bounded = std::isfinite(temperatureC)
        ? (temperatureC < 0 ? 0 : (temperatureC > 255 ? 255 : temperatureC)) : 0;
    state.die_temp_c = static_cast<uint8_t>(std::round(bounded));
    state.thermal_derate = derate;
    return can_protocol::packTaillightState(state);
}

class BrightnessOverride {
public:
    void set(uint8_t value) { _value = value; _active = true; }
    uint8_t requested(uint8_t fallback) const { return _active ? _value : fallback; }

private:
    bool _active = false;
    uint8_t _value = 0;
};

}  // namespace taillight_can
