// ---------------------------------------------------------------------------
// thermal.cpp
// ESP32-S3 on-die temperature monitoring with brightness derating.
// ---------------------------------------------------------------------------

#include "thermal.h"
#include <esp_system.h>   // temperatureRead()

void ThermalManager::begin() {
    _lastSampleMs = 0;  // force an immediate sample on first tick()
    _tempC        = static_cast<float>(temperatureRead());
    _derateAmount = 0;
    _shutdown     = false;


    Serial.print(F("[thermal] initial die temp: "));
    Serial.print(_tempC, 1);
    Serial.println(F(" °C"));
}

void ThermalManager::tick(unsigned long nowMs) {
    // Sample temperature every TEMP_SAMPLE_INTERVAL_MS (no need to be faster).
    if ((nowMs - _lastSampleMs) < TEMP_SAMPLE_INTERVAL_MS) return;
    _lastSampleMs = nowMs;

    _tempC = static_cast<float>(temperatureRead());


    // ── Shutdown zone (> TEMP_SHUTDOWN_C) ────────────────────────────────────
    if (_tempC >= static_cast<float>(TEMP_SHUTDOWN_C)) {
        if (!_shutdown) {
            _shutdown = true;
            Serial.print(F("[thermal] SHUTDOWN threshold reached: "));
            Serial.print(_tempC, 1);
            Serial.println(F(" °C — LEDs reduced to safety minimum"));
        }
        _derateAmount = 255;
        return;
    }

    // ── Derate zone (TEMP_DERATE_START_C .. TEMP_DERATE_END_C) ───────────────
    if (_tempC >= static_cast<float>(TEMP_DERATE_START_C)) {
        if (_shutdown) {
            _shutdown = false;
            Serial.println(F("[thermal] temperature returned below shutdown threshold"));
        }
        // Linear interpolation: 0 at DERATE_START, 255 at DERATE_END.
        float span  = static_cast<float>(TEMP_DERATE_END_C - TEMP_DERATE_START_C);
        float above = _tempC - static_cast<float>(TEMP_DERATE_START_C);
        float ratio = above / span;
        if (ratio > 1.0f) ratio = 1.0f;
        _derateAmount = static_cast<uint8_t>(ratio * 255.0f);
        return;
    }

    // ── Normal zone ───────────────────────────────────────────────────────────
    if (_shutdown) {
        _shutdown = false;
        Serial.println(F("[thermal] temperature normal"));
    }
    _derateAmount = 0;
}

uint8_t ThermalManager::applyBrightness(uint8_t requested) const {
    // Shutdown: clamp to BRIGHTNESS_MIN_SAFETY so safety-critical lights
    // (brake, turn) remain visible even if the MCU is overheating.
    if (_shutdown) return BRIGHTNESS_MIN_SAFETY;

    if (_derateAmount == 0) return requested;

    // Scale: result = requested * (1 - derateAmount/255)
    // But never go below BRIGHTNESS_MIN_SAFETY.
    uint16_t derated = static_cast<uint16_t>(requested)
                     * static_cast<uint16_t>(255 - _derateAmount)
                     / 255u;
    uint8_t result = static_cast<uint8_t>(derated);
    if (result < BRIGHTNESS_MIN_SAFETY) result = BRIGHTNESS_MIN_SAFETY;
    return result;
}
