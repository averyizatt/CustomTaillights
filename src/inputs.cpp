// ---------------------------------------------------------------------------
// inputs.cpp
// ---------------------------------------------------------------------------

#include "inputs.h"

void Inputs::begin() {
    // Driver side (channels 0-3) — brake and reverse get fast debounce
    _channels[0].pin = PIN_DRIVER_BRAKE;   _channels[0].debounceMs = DEBOUNCE_FAST_MS; _channels[0].state = false; _channels[0].lastRaw = false; _channels[0].lastChangeMs = 0;
    _channels[1].pin = PIN_DRIVER_RUNNING; _channels[1].debounceMs = DEBOUNCE_RUNNING_MS; _channels[1].state = false; _channels[1].lastRaw = false; _channels[1].lastChangeMs = 0;
    _channels[2].pin = PIN_DRIVER_TURN;    _channels[2].debounceMs = DEBOUNCE_MS;      _channels[2].state = false; _channels[2].lastRaw = false; _channels[2].lastChangeMs = 0;
    _channels[3].pin = PIN_DRIVER_REVERSE; _channels[3].debounceMs = DEBOUNCE_FAST_MS; _channels[3].state = false; _channels[3].lastRaw = false; _channels[3].lastChangeMs = 0;

    // Passenger side (channels 4-7) — same pattern
    _channels[4].pin = PIN_PASSENGER_BRAKE;   _channels[4].debounceMs = DEBOUNCE_FAST_MS; _channels[4].state = false; _channels[4].lastRaw = false; _channels[4].lastChangeMs = 0;
    _channels[5].pin = PIN_PASSENGER_RUNNING; _channels[5].debounceMs = DEBOUNCE_RUNNING_MS; _channels[5].state = false; _channels[5].lastRaw = false; _channels[5].lastChangeMs = 0;
    _channels[6].pin = PIN_PASSENGER_TURN;    _channels[6].debounceMs = DEBOUNCE_MS;      _channels[6].state = false; _channels[6].lastRaw = false; _channels[6].lastChangeMs = 0;
    _channels[7].pin = PIN_PASSENGER_REVERSE; _channels[7].debounceMs = DEBOUNCE_FAST_MS; _channels[7].state = false; _channels[7].lastRaw = false; _channels[7].lastChangeMs = 0;

    const uint8_t inputMode = OPT_ACTIVE_LEVEL == LOW ? INPUT_PULLUP : INPUT_PULLDOWN;
    for (auto& ch : _channels) {
        pinMode(ch.pin, inputMode);
    }
#if defined(CUSTOM_TAILLIGHTS_PCB)
    // Read the spare connector for diagnostics only; it selects no light state.
    pinMode(PIN_OPTO_AUX, inputMode);
#endif
}

uint8_t Inputs::rawPcbLevels() const {
    uint8_t levels = 0;
#if defined(CUSTOM_TAILLIGHTS_PCB)
    for (unsigned i = 0; i < 6; ++i) {
        if (digitalRead(PCB_OPTO_PINS[i]) == HIGH) levels |= (1u << i);
    }
#endif
    return levels;
}

// IRAM_ATTR keeps this function in Instruction RAM so it executes with
// zero cache-miss latency.  Called from a 1 ms FreeRTOS task on Core 0.
bool IRAM_ATTR Inputs::update() {
    bool changed = false;

    for (auto& ch : _channels) {
        changed |= _debounce(ch);
    }

    _driverBrake   = _channels[0].state;
    _driverRunning = _channels[1].state;
    _driverTurn    = _channels[2].state;
    _driverReverse = _channels[3].state;

    _passengerBrake   = _channels[4].state;
    _passengerRunning = _channels[5].state;
    _passengerTurn    = _channels[6].state;
    _passengerReverse = _channels[7].state;

    // Write packed snapshots as single atomic byte stores so Core 1 always
    // reads a coherent set of flags without a mutex.
    _driverSnapshot  = (_driverBrake   ? 0x01 : 0)
                   | (_driverRunning ? 0x02 : 0)
                   | (_driverTurn    ? 0x04 : 0)
                   | (_driverReverse ? 0x08 : 0);
    _passengerSnapshot = (_passengerBrake   ? 0x01 : 0)
                   | (_passengerRunning ? 0x02 : 0)
                   | (_passengerTurn    ? 0x04 : 0)
                   | (_passengerReverse ? 0x08 : 0);

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
