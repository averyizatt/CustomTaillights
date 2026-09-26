#pragma once
#include "Arduino.h"
#define FASTLED_VERSION 3010000
// Only the RGB value type is stubbed. The real pixel mapping and new animation
// implementations are compiled unchanged; none use FastLED effect primitives.
struct CRGB {
    uint8_t r = 0, g = 0, b = 0;
    CRGB() = default;
    CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    bool operator==(const CRGB& other) const { return r == other.r && g == other.g && b == other.b; }
    static const CRGB Black;
};
inline const CRGB CRGB::Black{0, 0, 0};
