#pragma once
#include <FastLED.h>
#include <stdint.h>

struct LedChannelStatus {
    int pin = -1;
    uint32_t completed = 0;
    uint32_t failures = 0;
    int lastError = 0;
};

// Register all three FastLED buffers; brightness/power limiting remain in FastLED.
void ledTransportBegin(CRGB* driver, CRGB* passenger, CRGB* status);
const LedChannelStatus& ledTransportStatus(unsigned index); // 0 driver, 1 passenger, 2 status
const char* ledTransportName();
