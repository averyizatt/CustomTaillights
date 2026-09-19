#pragma once
#include "states.h"

inline bool physicalPreviewBlocked(uint8_t driver, uint8_t passenger) {
    return ((driver | passenger) & 0x09) != 0; // physical brake or reverse
}
inline bool requestedTurn(bool physicalBlinking, bool softwareEnabled, uint8_t softwareMask) {
    return physicalBlinking || (softwareEnabled && (softwareMask & 0x04));
}
struct LightingRuntime {
    LightState driver = LightState::OFF;
    LightState passenger = LightState::OFF;
    uint32_t frames = 0;
    uint32_t lastFrameMs = 0;
    uint16_t driverLit = 0;
    uint16_t passengerLit = 0;
};
extern LightingRuntime g_lighting;
