#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// power_ramp.h
// Softens the load step on the 5 V supply. Going from dark or parking lights to
// every LED lit is a step of several amps within one frame; a supply or wiring
// that sags under it can reset the controller exactly when the brake lights or
// the startup animation come on. The current is instead allowed to rise by a
// fixed amount per frame: the lights still come on in the first frame, and
// reach full brightness one or two frames (25-50 ms) later. Falling is never
// limited.
//
// The estimate matches the usual WS2812B figure of 20 mA per colour channel at
// full scale; it is a limit on steps, not a measurement.
// ---------------------------------------------------------------------------
namespace power_ramp {

// channelSum = the sum of every red, green and blue value in the frame.
inline uint32_t estimateMa(uint32_t channelSum, uint8_t brightness) {
    return static_cast<uint32_t>(static_cast<uint64_t>(channelSum) * 20u * brightness / (255u * 255u));
}

struct Limiter {
    uint32_t lastMa = 0;

    // Returns the brightness to use for this frame. budgetMa is the hard cap
    // applied elsewhere (so the remembered level never exceeds what was drawn).
    uint8_t apply(uint32_t channelSum, uint8_t brightness, uint32_t stepMa, uint32_t budgetMa) {
        const uint32_t want = estimateMa(channelSum, brightness);
        const uint32_t allowed = lastMa + stepMa;
        uint8_t out = brightness;
        // Above the hard cap the frame is scaled back to the cap anyway, so that is the level to reach.
        if ((want > budgetMa ? budgetMa : want) > allowed) {
            out = static_cast<uint8_t>(static_cast<uint64_t>(brightness) * allowed / want);
            if (out == 0) out = 1;
        }
        const uint32_t drawn = estimateMa(channelSum, out);
        lastMa = drawn > budgetMa ? budgetMa : drawn;
        return out;
    }
};

} // namespace power_ramp
