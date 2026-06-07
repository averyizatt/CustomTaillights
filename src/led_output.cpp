#include "led_output.h"

#include <Arduino.h>
#include <FastLED.h>

#include "config.h"

namespace {
unsigned long g_lastShowMs = 0;
bool          g_hasShown   = false;
}

void ledOutputShow(bool force) {
    const unsigned long now = millis();
    if (!force && g_hasShown && (now - g_lastShowMs) < LED_SHOW_MIN_INTERVAL_MS) {
        return;
    }

    FastLED.show();
    g_lastShowMs = millis();
    g_hasShown   = true;
}

void ledOutputClear(bool forceShow, bool force) {
    FastLED.clear(false);
    if (forceShow) {
        ledOutputShow(force);
    }
}
