#pragma once

// ---------------------------------------------------------------------------
// thermal.h
// ESP32-S3 on-die temperature monitoring with brightness derating.
//
// Thresholds (all °C, configurable in config.h):
//   TEMP_DERATE_START_C  — below this, brightness is never reduced
//   TEMP_DERATE_END_C    — at this temp, brightness is reduced by TEMP_MAX_DERATE
//   TEMP_SHUTDOWN_C      — above this, LEDs are forced to minimum brightness
//                          (not full-off so brake/turn signals remain visible)
//
// Integration:
//   call thermal.begin()          once in setup()
//   call thermal.tick(nowMs)      every loop iteration
//   call thermal.applyBrightness(targetBrightness)
//     → returns the derated value to actually pass to FastLED.setBrightness()
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "config.h"

class ThermalManager {
public:
    void begin();

    // Call every loop iteration; samples temperature on its own timer.
    void tick(unsigned long nowMs);

    // Apply thermal derating to the requested brightness.
    // Returns a value in [BRIGHTNESS_MIN_SAFETY .. requestedBrightness].
    uint8_t applyBrightness(uint8_t requested) const;

    // Current die temperature in °C (last sample).
    float   tempC()         const { return _tempC;         }

    // 0 = no derating, 255 = maximum derating applied.
    uint8_t derateAmount()  const { return _derateAmount;  }

    // True when temperature is above TEMP_SHUTDOWN_C.
    bool    isShutdown()    const { return _shutdown;      }

private:
    float   _tempC        = 25.0f;
    uint8_t _derateAmount = 0;
    bool    _shutdown     = false;

    unsigned long _lastSampleMs = 0;
};
