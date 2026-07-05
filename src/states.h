#pragma once

// ---------------------------------------------------------------------------
// states.h
// Per-side light state enum, turn blink detector, and resolver.
//
// Steady inputs and blinking inputs are deliberately separate. Hazard and
// turn modes are selected only after the debounced turn input has produced
// timing-valid transitions; a steady ON turn line is not treated as a blink.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include "config.h"

// States that apply to one individual side.
enum class LightState : uint8_t {
    OFF        = 0,  // nothing active
    RUNNING    = 1,  // dim red, parking / running light
    BRAKE      = 2,  // bright red, brake
    TURN       = 3,  // amber blink, this side's turn signal
    REVERSE    = 4,  // white, reverse
    BRAKE_TURN = 5,  // brake + turn active on the same side
    HAZARD     = 6,  // both turn inputs blinking together
    CUSTOM     = 7,  // CAN-commanded custom animation
    SHOW       = 8,  // standalone show-mode animation
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

// Derive the LightState for one side from debounced steady inputs plus the
// blink detector output. Each render frame starts clean from inputs, then
// applies this priority:
//   HAZARD > BRAKE_TURN > TURN > BRAKE > REVERSE > RUNNING > OFF
inline LightState resolveSideState(bool brake, bool running,
                                   bool turnBlinking, bool reverse,
                                   bool hazardBlinking)
{
    if (hazardBlinking)         return LightState::HAZARD;
    if (brake && turnBlinking)  return LightState::BRAKE_TURN;
    if (turnBlinking)           return LightState::TURN;
    if (brake)                  return LightState::BRAKE;
    if (reverse)                return LightState::REVERSE;
    if (running)                return LightState::RUNNING;
    return LightState::OFF;
}
