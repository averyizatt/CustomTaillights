#pragma once

// ---------------------------------------------------------------------------
// canbus.h
// MCP2515 SPI CAN bus interface for the Foxbody taillight controller.
//
// Responsibilities
// ────────────────
//  TX  Broadcast the current left/right LightState every CAN_BROADCAST_INTERVAL_MS.
//
//  RX  Parse command frames (CAN_ID_COMMAND) sent by other ECUs:
//        Cmd 0x01 — set global brightness  (byte 1 = 0-255)
//        Cmd 0x02 — animation override     (byte 1 = left state, byte 2 = right state)
//        Cmd 0x03 — clear animation override
//
// Typical usage (main.cpp)
// ─────────────────────────
//   canBus.begin();
//   // in loop:
//   canBus.tick(leftState, rightState, inputs);
//   if (canBus.hasOverride()) {
//       leftState  = canBus.overrideLeft();
//       rightState = canBus.overrideRight();
//   }
//   if (canBus.brightnessChanged()) {
//       FastLED.setBrightness(canBus.brightness());
//       canBus.clearBrightnessChanged();
//   }
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>
#include "config.h"
#include "states.h"
#include "inputs.h"
#include "thermal.h"

class CANBus {
public:
    // Initialise SPI bus and MCP2515.  Returns true on success.
    bool begin();

    // Call every loop iteration.
    //   leftState / rightState — resolved states from this frame (for TX)
    //   inputs                 — raw input object (used to build raw-flags bytes)
    //   thermal                — included in broadcast payload
    void tick(LightState leftState, LightState rightState,
              const Inputs& inputs, const ThermalManager& thermal);

    // ── Animation override ───────────────────────────────────────────────────
    // True after a Cmd 0x02 is received and before Cmd 0x03 clears it.
    bool       hasOverride()    const { return _hasOverride; }
    LightState overrideLeft()   const { return _overrideLeft; }
    LightState overrideRight()  const { return _overrideRight; }

    // ── Brightness override ──────────────────────────────────────────────────
    // brightnessChanged() is set to true when a Cmd 0x01 arrives.
    // Call clearBrightnessChanged() after you have applied the value.
    bool    brightnessChanged()      const { return _brightnessChanged; }
    uint8_t brightness()             const { return _brightness; }
    void    clearBrightnessChanged()       { _brightnessChanged = false; }

    // True if begin() succeeded and the MCP2515 is online
    bool isOnline() const { return _online; }

private:
    MCP2515 _mcp{PIN_CAN_CS};

    bool _online = false;

    // TX
    unsigned long _lastBroadcastMs = 0;
    void _sendState(LightState leftState, LightState rightState,
                    const Inputs& inputs, const ThermalManager& thermal);

    // RX
    void _processFrame(const struct can_frame& frame);

    // Override state
    bool       _hasOverride   = false;
    LightState _overrideLeft  = LightState::OFF;
    LightState _overrideRight = LightState::OFF;

    // Brightness
    bool    _brightnessChanged = false;
    uint8_t _brightness        = BRIGHTNESS_DEFAULT;

    // Connection management
    bool          _spiStarted    = false;  // SPI.begin() called once; never repeated
    unsigned long _busOffRetryMs = 0;      // millis() target for next recovery attempt
    bool _initMCP();                       // (re)configure MCP2515 without re-opening SPI
};
