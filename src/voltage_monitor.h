#pragma once
#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------------------------
// VoltageMonitor — Undervoltage Lockout (UVLO)
//
// Polls a voltage-divider ADC input to measure the 12 V car rail.
// LEDs are suppressed until the rail has been above VMON_ON_THRESH_V
// continuously for VMON_STABLE_MS milliseconds.  A hysteresis threshold
// (VMON_OFF_THRESH_V) prevents rapid toggling around the enable point.
//
// State machine:
//
//   LOCKED  ──(V >= ON  for STABLE_MS)──►  READY
//   READY   ──(V <  OFF)               ──►  LOCKED
//
// The WDT, inputs, and CAN bus continue to run in the LOCKED state —
// only LED output is suppressed by the caller checking isReady().
// ---------------------------------------------------------------------------
class VoltageMonitor {
public:
    void  begin();
    void  tick(unsigned long nowMs);

    // True once the rail has been stable above the ON threshold long enough.
    bool  isReady()   const { return _ready; }

    // Most recent measured rail voltage in volts.
    float voltageV()  const { return _voltageV; }

private:
    float        _voltageV    = 0.0f;
    bool         _ready       = false;
    unsigned long _stableFrom = 0;    // millis() when voltage first crossed ON threshold
    unsigned long _lastSample = 0;

    float _readVoltage() const;
};
