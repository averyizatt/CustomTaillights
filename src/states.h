#pragma once

// ---------------------------------------------------------------------------
// states.h
// Per-side light state enum, turn blink detector, and resolver.
//
// Brake/running/reverse are steady vehicle-level signals in main.cpp; turn
// and hazard modes are selected only from timing-valid turn input transitions.
// A steady ON turn line is not treated as a blink.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include "config.h"

enum class LightState : uint8_t {
    OFF        = 0,
    RUNNING    = 1,
    BRAKE      = 2,
    TURN       = 3,
    REVERSE    = 4,
    BRAKE_TURN = 5,
    HAZARD     = 6,
    CUSTOM     = 7,
    SHOW       = 8,
};

struct TurnBlinkSnapshot {
    bool active = false;
    bool blinking = false;
    unsigned long lastTransitionMs = 0;
    unsigned long lastRisingMs = 0;
};

class TurnBlinkDetector {
public:
    TurnBlinkSnapshot update(bool turnActive, unsigned long nowMs) {
        if (!_initialised) {
            _initialised = true;
            _lastLevel = turnActive;
            if (turnActive) _lastRisingMs = nowMs;
        } else if (turnActive != _lastLevel) {
            const unsigned long edgeMs = nowMs - _lastTransitionMs;
            const bool edgeInRange = (_lastTransitionMs != 0)
                                  && (edgeMs >= BLINK_MIN_EDGE_MS)
                                  && (edgeMs <= BLINK_MAX_EDGE_MS);

            _validEdgeCount = edgeInRange
                            ? (uint8_t)((_validEdgeCount < 3) ? (_validEdgeCount + 1) : 3)
                            : 1;
            _lastTransitionMs = nowMs;
            _lastLevel = turnActive;
            if (turnActive) _lastRisingMs = nowMs;
        }

        if (_lastTransitionMs == 0 || (nowMs - _lastTransitionMs) > BLINK_EXPIRE_MS) {
            _validEdgeCount = 0;
        }

        TurnBlinkSnapshot snap;
        snap.active = _lastLevel;
        snap.blinking = _validEdgeCount >= 2;
        snap.lastTransitionMs = _lastTransitionMs;
        snap.lastRisingMs = _lastRisingMs;
        return snap;
    }

private:
    bool _initialised = false;
    bool _lastLevel = false;
    uint8_t _validEdgeCount = 0;
    unsigned long _lastTransitionMs = 0;
    unsigned long _lastRisingMs = 0;
};

inline unsigned long elapsedBetween(unsigned long a, unsigned long b) {
    return (a >= b) ? (a - b) : (b - a);
}

inline bool validHazardBlink(const TurnBlinkSnapshot& driver,
                             const TurnBlinkSnapshot& passenger)
{
    if (!driver.blinking || !passenger.blinking) return false;

    const bool edgesSynced = elapsedBetween(driver.lastTransitionMs,
                                            passenger.lastTransitionMs) <= HAZARD_SYNC_MS;
    const bool risesSynced = driver.lastRisingMs != 0 && passenger.lastRisingMs != 0
                          && elapsedBetween(driver.lastRisingMs,
                                            passenger.lastRisingMs) <= HAZARD_SYNC_MS;
    return edgesSynced || risesSynced;
}

// Resolve one side from shared steady inputs plus this side's turn blinking.
// Priority: HAZARD > BRAKE_TURN > TURN > BRAKE > REVERSE > RUNNING > OFF.
inline LightState resolveSideState(bool brake, bool running,
                                   bool turnBlinking, bool reverse,
                                   bool hazardBlinking)
{
    if (hazardBlinking)        return LightState::HAZARD;
    if (brake && turnBlinking) return LightState::BRAKE_TURN;
    if (turnBlinking)          return LightState::TURN;
    if (brake)                 return LightState::BRAKE;
    if (reverse)               return LightState::REVERSE;
    if (running)               return LightState::RUNNING;
    return LightState::OFF;
}
