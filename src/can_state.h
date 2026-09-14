#pragma once

#include <cmath>
#include <can_contract/can_protocol.h>

// Firmware adapters for the unchanged shared CCM/water-meth contract.
namespace taillight_can {

inline can_protocol::CanFrame encodeState(uint8_t left, uint8_t right,
                                         uint8_t inputFlags, uint8_t brightness,
                                         float temperatureC, uint8_t derate) {
    can_protocol::CanFrame frame;
    frame.id = can_protocol::ID_TAILLIGHT_STATE;
    frame.dlc = 7;
    frame.data[0] = left;
    frame.data[1] = right;
    frame.data[2] = inputFlags;
    frame.data[3] = brightness;
    // The shared decoder returns int8_t Celsius; avoid wrapping above 127 C.
    const float boundedTemp = std::isfinite(temperatureC)
        ? (temperatureC < -40.0f ? -40.0f : (temperatureC > 127.0f ? 127.0f : temperatureC))
        : -40.0f;
    frame.data[4] = can_protocol::tempToOffset40(static_cast<int>(std::round(boundedTemp)));
    frame.data[5] = static_cast<uint8_t>((static_cast<uint16_t>(derate) * 100u + 127u) / 255u);
    // No status-bit assignments exist in the shared contract.
    frame.data[6] = 0;
    return frame;
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
