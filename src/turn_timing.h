#pragma once
#include "settings.h"

struct TurnTiming {
    unsigned long sweep, hold, off;
    unsigned long on() const { return sweep + hold; }
    unsigned long period() const { return on() + off; }
};
inline unsigned long turnClamp(unsigned long value, unsigned long low, unsigned long high) {
    return value < low ? low : (value > high ? high : value);
}
inline TurnTiming turnTiming(const Settings& s) {
    if (!s.turn_custom) {
        const unsigned long period = turnClamp(s.turn_blink_ms, 200, 1500);
        return {period / 2, 0, period - period / 2};
    }
    return {turnClamp(s.turn_sweep_ms, 50, 1500), turnClamp(s.turn_hold_ms, 0, 1500),
            turnClamp(s.turn_off_ms, 50, 1500)};
}
