// ---------------------------------------------------------------------------
// canbus.cpp
// MCP2515 SPI CAN bus interface.
// ---------------------------------------------------------------------------

#include "canbus.h"

// ---------------------------------------------------------------------------
// State broadcast payload layout (CAN_ID_STATE_BROADCAST, 0x100)
// ─────────────────────────────────────────────────────────────────
//  Byte 0 : left  LightState  (enum value 0-6)
//  Byte 1 : right LightState  (enum value 0-6)
//  Byte 2 : left  raw flags   bit0=brake  bit1=running  bit2=turn  bit3=reverse
//  Byte 3 : right raw flags   bit0=brake  bit1=running  bit2=turn  bit3=reverse
//  Byte 4 : current brightness (0-255)
//  Byte 5 : die temperature °C (0-255, clamped)
//  Byte 6 : thermal derate amount (0=none, 255=max)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Command frame layout (CAN_ID_COMMAND, 0x101)
// ─────────────────────────────────────────────
//  Byte 0 : command
//    0x01  Set brightness   — byte 1 = brightness (0-255)
//    0x02  Animation override — byte 1 = left LightState, byte 2 = right LightState
//    0x03  Clear override
// ---------------------------------------------------------------------------

static constexpr uint8_t CMD_SET_BRIGHTNESS = 0x01;
static constexpr uint8_t CMD_ANIM_OVERRIDE  = 0x02;
static constexpr uint8_t CMD_CLEAR_OVERRIDE = 0x03;

// ---------------------------------------------------------------------------
bool CANBus::begin() {
    // Open SPI bus once.  Calling SPI.begin() again on a running bus is
    // harmless in Arduino but unnecessary and potentially disruptive.
    if (!_spiStarted) {
        SPI.begin(PIN_CAN_SCK, PIN_CAN_MISO, PIN_CAN_MOSI, PIN_CAN_CS);
        _spiStarted = true;
    }
    return _initMCP();
}

// ---------------------------------------------------------------------------
bool CANBus::_initMCP() {
    _mcp.reset();

    if (_mcp.setBitrate(CAN_500KBPS, MCP_8MHZ) != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] setBitrate failed — check module / clock"));
        _online = false;
        return false;
    }

    // Accept only CAN_ID_COMMAND frames; mask & filter on bits 10:0.
    _mcp.setFilterMask(MCP2515::MASK0, false, 0x7FF);
    _mcp.setFilter(MCP2515::RXF0,      false, CAN_ID_COMMAND);

    if (_mcp.setNormalMode() != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] setNormalMode failed"));
        _online = false;
        return false;
    }

    _online        = true;
    _busOffRetryMs = 0;
    Serial.println(F("[CAN] MCP2515 online — 500 kbit/s"));
    return true;
}

// ---------------------------------------------------------------------------
void CANBus::tick(LightState leftState, LightState rightState,
                  const Inputs& inputs, const ThermalManager& thermal) {
    // ── Bus-off detection and recovery ───────────────────────────────────────────
    // EFLG bit5 = TXBO (transmit bus-off).  This happens when the TX error
    // counter reaches 256 — usually a wiring fault or missing termination.
    // We schedule a reinit 250 ms later to allow the bus to settle.
    if (_online) {
        uint8_t eflg = _mcp.getErrorFlags();
        if (eflg & 0x20) {   // TXBO — bus-off
            _online        = false;
            _busOffRetryMs = millis() + 250;
            Serial.println(F("[CAN] bus-off detected — retry in 250 ms"));
        }
        if (eflg & 0xC0) {   // RX0OVR / RX1OVR — receive buffer overflow
            _mcp.clearRXnOVRFlags();
            Serial.println(F("[CAN] RX overflow cleared"));
        }
    }
    if (!_online) {
        if (_busOffRetryMs != 0 && millis() >= _busOffRetryMs) {
            Serial.println(F("[CAN] attempting bus-off recovery ..."));
            _initMCP();
        }
        return;  // skip TX/RX until bus is confirmed back online
    }

    // ── TX: periodic state broadcast ────────────────────────────────────────
    unsigned long nowMs = millis();
    if (nowMs - _lastBroadcastMs >= CAN_BROADCAST_INTERVAL_MS) {
        _lastBroadcastMs = nowMs;
        _sendState(leftState, rightState, inputs, thermal);
    }

    // ── RX: drain all pending frames ────────────────────────────────────────
    struct can_frame frame;
    while (_mcp.readMessage(&frame) == MCP2515::ERROR_OK) {
        _processFrame(frame);
    }
}

// ---------------------------------------------------------------------------
void CANBus::_sendState(LightState leftState, LightState rightState,
                        const Inputs& inputs, const ThermalManager& thermal) {
    struct can_frame frame;
    frame.can_id  = CAN_ID_STATE_BROADCAST;
    frame.can_dlc = 7;

    frame.data[0] = static_cast<uint8_t>(leftState);
    frame.data[1] = static_cast<uint8_t>(rightState);

    // Left raw flags
    frame.data[2] = (inputs.leftBrake()   ? 0x01 : 0)
                  | (inputs.leftRunning() ? 0x02 : 0)
                  | (inputs.leftTurn()    ? 0x04 : 0)
                  | (inputs.leftReverse() ? 0x08 : 0);

    // Right raw flags
    frame.data[3] = (inputs.rightBrake()   ? 0x01 : 0)
                  | (inputs.rightRunning() ? 0x02 : 0)
                  | (inputs.rightTurn()    ? 0x04 : 0)
                  | (inputs.rightReverse() ? 0x08 : 0);

    frame.data[4] = _brightness;
    // Byte 5: die temperature in °C, clamped to 0-255
    int tempRounded = static_cast<int>(thermal.tempC() + 0.5f);
    frame.data[5]   = static_cast<uint8_t>(tempRounded < 0 ? 0 : (tempRounded > 255 ? 255 : tempRounded));
    // Byte 6: thermal derate amount (0=none, 255=maximum)
    frame.data[6]   = thermal.derateAmount();

    if (_mcp.sendMessage(&frame) != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] TX error"));
    }
}

// ---------------------------------------------------------------------------
void CANBus::_processFrame(const struct can_frame& frame) {
    if (frame.can_id != CAN_ID_COMMAND || frame.can_dlc < 1) return;

    switch (frame.data[0]) {

        case CMD_SET_BRIGHTNESS:
            if (frame.can_dlc >= 2) {
                _brightness        = frame.data[1];
                _brightnessChanged = true;
                Serial.print(F("[CAN] brightness -> "));
                Serial.println(_brightness);
            }
            break;

        case CMD_ANIM_OVERRIDE:
            if (frame.can_dlc >= 3) {
                // Clamp incoming values to valid LightState range (0-6)
                uint8_t l = frame.data[1];
                uint8_t r = frame.data[2];
                if (l > 6) l = 0;
                if (r > 6) r = 0;
                _overrideLeft  = static_cast<LightState>(l);
                _overrideRight = static_cast<LightState>(r);
                _hasOverride   = true;
                Serial.print(F("[CAN] anim override left="));
                Serial.print(l);
                Serial.print(F(" right="));
                Serial.println(r);
            }
            break;

        case CMD_CLEAR_OVERRIDE:
            _hasOverride = false;
            Serial.println(F("[CAN] override cleared"));
            break;

        default:
            Serial.print(F("[CAN] unknown cmd 0x"));
            Serial.println(frame.data[0], HEX);
            break;
    }
}
