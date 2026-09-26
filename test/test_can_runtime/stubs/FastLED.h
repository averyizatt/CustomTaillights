#pragma once
#include <Arduino.h>
#define FASTLED_VERSION 3010003
struct CRGB {
    uint8_t r = 0, g = 0, b = 0;
    CRGB() = default;
    CRGB(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};
