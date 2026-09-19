#include "led_output.h"

#include <Arduino.h>
#include <FastLED.h>

#include "config.h"

#if defined(ESP32)
#include <esp_idf_version.h>
#if ESP_IDF_VERSION_MAJOR >= 5 && !FASTLED_RMT5
#error "This Arduino board links the new RMT driver; enable FASTLED_RMT5 to avoid a boot-time driver conflict."
#endif
#endif

namespace {
unsigned long g_lastShowMs = 0;
bool          g_hasShown   = false;
}

bool ledOutputReady(unsigned long intervalMs) {
    if (intervalMs < LED_SHOW_MIN_INTERVAL_MS) intervalMs = LED_SHOW_MIN_INTERVAL_MS;
    return !g_hasShown || (millis() - g_lastShowMs) >= intervalMs;
}

void ledOutputShow(bool force) {
    if (!force && !ledOutputReady(LED_SHOW_MIN_INTERVAL_MS)) return;
    const unsigned long now = millis();
    FastLED.show();
    g_lastShowMs = now;
    g_hasShown   = true;
}

void ledOutputClear(bool forceShow, bool force) {
    FastLED.clear(false);
    if (forceShow) {
        ledOutputShow(force);
    }
}
