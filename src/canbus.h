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
#include <Arduino.h>
#include <SPI.h>
#include <mcp2515.h>
#include "config.h"
#include "states.h"
#include "inputs.h"
#include "thermal.h"
#include "faults.h"
#include "can_protocol.h"
#include "can_state.h"
#include "animations.h"

class CANBus {
public:
    // Initialise SPI bus and MCP2515.  Returns true on success.
    bool begin();

    // Call every loop iteration.
    //   driverState / passengerState — resolved states from this frame (for TX)
    //   inputs                 — raw input object (used to build raw-flags bytes)
    //   thermal                — included in broadcast payload
    void tick(LightState driverState, LightState passengerState,
              const Inputs& inputs, const ThermalManager& thermal,
              uint8_t appliedBrightness);

    // ── Animation override ───────────────────────────────────────────────────
    // True after a Cmd 0x02 is received and before Cmd 0x03 clears it.
    bool       hasOverride()    const { return _hasOverride; }
    LightState overrideDriver()    const { return _overrideDriver;    }  // driver side   (US left)
    LightState overridePassenger() const { return _overridePassenger; }  // passenger side (US right)

    // ── Brightness override ──────────────────────────────────────────────────
    // Last CAN command remains active until replaced or the controller restarts.
    // Apply thermal limiting to this requested value every loop.
    uint8_t requestedBrightness(uint8_t fallback) const {
        return _brightness.requested(fallback);
    }

    // True if begin() succeeded and the MCP2515 is online
    bool isOnline() const { return _online; }

    // ── Custom animation (Cmd 0x04) ───────────────────────────────────────────
    // True after a Cmd 0x04 is received and while the animation is running.
    // main.cpp checks this to push LightState::CUSTOM into both sides.
    // Call clearCustomAnim() when both sides' isDone() flags are set.
    bool hasCustomAnim()     const { return _hasCustomAnim; }
    void clearCustomAnim()         { _hasCustomAnim = false; }

    // ── Fault reporting ──────────────────────────────────────────────────────
    // Broadcast a CAN_ID_FAULT frame (0x102) with a FAULT_* code.
    // Silently dropped if the bus is offline — callers do not need to check.
    //   code     : FAULT_* constant from faults.h
    //   severity : FAULT_SEV_INFO / WARNING / CRITICAL
    //   data0/1  : fault-specific payload bytes (0 if unused)
    void reportFault(uint8_t code, uint8_t severity,
                     uint8_t data0 = 0, uint8_t data1 = 0);

private:
    MCP2515 _mcp{PIN_CAN_CS};

    bool _online = false;

    // TX
    unsigned long _lastBroadcastMs = 0;
    void _sendState(LightState driverState, LightState passengerState,
                    const Inputs& inputs, const ThermalManager& thermal,
              uint8_t appliedBrightness);

    // RX
    void _processFrame(const struct can_frame& frame);

    // Override state
    bool       _hasOverride   = false;
    LightState _overrideDriver  = LightState::OFF;
    LightState _overridePassenger = LightState::OFF;

    // Custom animation state
    bool _hasCustomAnim = false;

    // Brightness
    taillight_can::BrightnessOverride _brightness;

    // Connection management
    bool          _spiStarted    = false;  // SPI.begin() called once; never repeated
    unsigned long _busOffRetryMs = 0;      // millis() target for next recovery attempt
    uint8_t       _consecutiveTxFailures = 0;
    bool _initMCP();                       // (re)configure MCP2515 without re-opening SPI
};
