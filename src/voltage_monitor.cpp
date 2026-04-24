#include "voltage_monitor.h"

// Divider scale factor: Vin = Vadc * (R1 + R2) / R2
static constexpr float VMON_DIVIDER_SCALE =
    (VMON_R1_KOHM + VMON_R2_KOHM) / VMON_R2_KOHM;

// ---------------------------------------------------------------------------
void VoltageMonitor::begin() {
    // Configure pin as ADC input (no pull resistors)
    pinMode(PIN_VMON, INPUT);

    // Set 11 dB attenuation on this channel to allow up to ~3.1 V input
    analogSetPinAttenuation(PIN_VMON, ADC_11db);

    // Take an initial reading so isReady() starts with a real value
    _voltageV  = _readVoltage();
    _lastSample = millis();

    // If the rail is already healthy at boot (e.g. bench power, not cranking),
    // start the stable timer immediately rather than waiting an extra cycle.
    if (_voltageV >= VMON_ON_THRESH_V) {
        _stableFrom = _lastSample;
    }
}

// ---------------------------------------------------------------------------
void VoltageMonitor::tick(unsigned long nowMs) {
    if (nowMs - _lastSample < VMON_SAMPLE_MS) return;
    _lastSample = nowMs;

    _voltageV = _readVoltage();

    if (!_ready) {
        // LOCKED state — waiting for voltage to be stable above ON threshold
        if (_voltageV >= VMON_ON_THRESH_V) {
            if (_stableFrom == 0) {
                _stableFrom = nowMs;  // just crossed threshold — start timer
            } else if (nowMs - _stableFrom >= VMON_STABLE_MS) {
                _ready = true;
                Serial.printf("[uvlo] READY — rail %.2f V (stable %lu ms)\n",
                              _voltageV, (unsigned long)VMON_STABLE_MS);
            }
        } else {
            // Fell below threshold — reset stable timer
            _stableFrom = 0;
        }
    } else {
        // READY state — lock out again if rail drops below hysteresis threshold
        if (_voltageV < VMON_OFF_THRESH_V) {
            _ready      = false;
            _stableFrom = 0;
            Serial.printf("[uvlo] LOCKED — rail dropped to %.2f V\n", _voltageV);
        }
    }
}

// ---------------------------------------------------------------------------
float VoltageMonitor::_readVoltage() const {
    // Average 4 samples to reduce ADC noise
    uint32_t sum = 0;
    for (int i = 0; i < 4; i++) sum += analogRead(PIN_VMON);
    float vadc = (sum / 4.0f) / 4095.0f * VMON_ADC_VREF;
    return vadc * VMON_DIVIDER_SCALE;
}
