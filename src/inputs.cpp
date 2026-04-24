// ---------------------------------------------------------------------------
// inputs.cpp
// ---------------------------------------------------------------------------

#include "inputs.h"

void Inputs::begin() {
    // Left side  (channels 0-3) — brake and reverse get fast debounce
    _channels[0].pin = PIN_LEFT_BRAKE;   _channels[0].debounceMs = DEBOUNCE_FAST_MS; _channels[0].state = false; _channels[0].lastRaw = false; _channels[0].lastChangeMs = 0;
    _channels[1].pin = PIN_LEFT_RUNNING; _channels[1].debounceMs = DEBOUNCE_MS;      _channels[1].state = false; _channels[1].lastRaw = false; _channels[1].lastChangeMs = 0;
    _channels[2].pin = PIN_LEFT_TURN;    _channels[2].debounceMs = DEBOUNCE_MS;      _channels[2].state = false; _channels[2].lastRaw = false; _channels[2].lastChangeMs = 0;
    _channels[3].pin = PIN_LEFT_REVERSE; _channels[3].debounceMs = DEBOUNCE_FAST_MS; _channels[3].state = false; _channels[3].lastRaw = false; _channels[3].lastChangeMs = 0;

    // Right side (channels 4-7) — same pattern
    _channels[4].pin = PIN_RIGHT_BRAKE;   _channels[4].debounceMs = DEBOUNCE_FAST_MS; _channels[4].state = false; _channels[4].lastRaw = false; _channels[4].lastChangeMs = 0;
    _channels[5].pin = PIN_RIGHT_RUNNING; _channels[5].debounceMs = DEBOUNCE_MS;      _channels[5].state = false; _channels[5].lastRaw = false; _channels[5].lastChangeMs = 0;
    _channels[6].pin = PIN_RIGHT_TURN;    _channels[6].debounceMs = DEBOUNCE_MS;      _channels[6].state = false; _channels[6].lastRaw = false; _channels[6].lastChangeMs = 0;
    _channels[7].pin = PIN_RIGHT_REVERSE; _channels[7].debounceMs = DEBOUNCE_FAST_MS; _channels[7].state = false; _channels[7].lastRaw = false; _channels[7].lastChangeMs = 0;

    for (auto& ch : _channels) {
        pinMode(ch.pin, INPUT_PULLUP);
    }
}

// IRAM_ATTR keeps this function in Instruction RAM so it executes with
// zero cache-miss latency.  Called from a 1 ms FreeRTOS task on Core 0.
bool IRAM_ATTR Inputs::update() {
    bool changed = false;

    for (auto& ch : _channels) {
        changed |= _debounce(ch);
    }

    _leftBrake   = _channels[0].state;
    _leftRunning = _channels[1].state;
    _leftTurn    = _channels[2].state;
    _leftReverse = _channels[3].state;

    _rightBrake   = _channels[4].state;
    _rightRunning = _channels[5].state;
    _rightTurn    = _channels[6].state;
    _rightReverse = _channels[7].state;

    // Write packed snapshots as single atomic byte stores so Core 1 always
    // reads a coherent set of flags without a mutex.
    _leftSnapshot  = (_leftBrake   ? 0x01 : 0)
                   | (_leftRunning ? 0x02 : 0)
                   | (_leftTurn    ? 0x04 : 0)
                   | (_leftReverse ? 0x08 : 0);
    _rightSnapshot = (_rightBrake   ? 0x01 : 0)
                   | (_rightRunning ? 0x02 : 0)
                   | (_rightTurn    ? 0x04 : 0)
                   | (_rightReverse ? 0x08 : 0);

    return changed;
}

bool IRAM_ATTR Inputs::_debounce(Channel& ch) {
    bool raw = (digitalRead(ch.pin) == OPT_ACTIVE_LEVEL);
    unsigned long nowMs = millis();  // single call — avoids split across ms boundary

    if (raw != ch.lastRaw) {
        ch.lastRaw      = raw;
        ch.lastChangeMs = nowMs;
    }

    if ((nowMs - ch.lastChangeMs) >= ch.debounceMs && raw != ch.state) {
        ch.state = raw;
        return true;
    }

    return false;
}
