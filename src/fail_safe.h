#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// fail_safe.h
// Second opinions for the lights that must never fail quietly. Each one is
// independent of the part it backs up, and does nothing while that part works.
// ---------------------------------------------------------------------------
namespace fail_safe {

// A raw input level that has stayed active for holdMs. The input task debounces
// in 5 ms; this slower reading on the render core only matters if that task has
// stopped reporting, and then keeps brake and reverse working.
struct HeldLevel {
    unsigned long sinceMs = 0;
    bool active = false;

    bool update(bool raw, unsigned long nowMs, unsigned long holdMs) {
        if (!raw) { active = false; return false; }
        if (!active) { active = true; sinceMs = nowMs; }
        return nowMs - sinceMs >= holdMs;
    }
};

// A lamp that should be showing the brake light but has had no pixel lit for
// limitMs: whatever the animation, colour setting or a command did, paint it.
// The limit is longer than the dark part of any brake flash effect.
struct DarkLamp {
    unsigned long sinceMs = 0;
    bool dark = false;

    bool update(bool shouldBeLit, uint16_t litPixels, unsigned long nowMs, unsigned long limitMs) {
        if (!shouldBeLit || litPixels > 0) { dark = false; return false; }
        if (!dark) { dark = true; sinceMs = nowMs; }
        return nowMs - sinceMs >= limitMs;
    }
};

// Brightness never goes below the floor while a brake, reverse or turn light is
// asked for, whatever a setting or a CAN brightness command says.
inline uint8_t floorBrightness(uint8_t brightness, bool safetyLight, uint8_t floor) {
    return (safetyLight && brightness < floor) ? floor : brightness;
}

} // namespace fail_safe
