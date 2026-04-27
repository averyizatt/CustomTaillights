// ---------------------------------------------------------------------------
// canbus.cpp
// MCP2515 SPI CAN bus interface.
// ---------------------------------------------------------------------------

#include "canbus.h"

static constexpr uint8_t CMD_SET_BRIGHTNESS = 0x01;
static constexpr uint8_t CMD_ANIM_OVERRIDE  = 0x02;
static constexpr uint8_t CMD_CLEAR_OVERRIDE = 0x03;
static constexpr uint8_t CMD_CUSTOM_ANIM    = 0x04;

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
void CANBus::tick(LightState driverState, LightState passengerState,
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
        _sendState(driverState, passengerState, inputs, thermal);
    }

    // ── RX: drain all pending frames ────────────────────────────────────────
    struct can_frame frame;
    while (_mcp.readMessage(&frame) == MCP2515::ERROR_OK) {
        _processFrame(frame);
    }
}

// ---------------------------------------------------------------------------
void CANBus::_sendState(LightState driverState, LightState passengerState,
                        const Inputs& inputs, const ThermalManager& thermal) {
    struct can_frame frame;
    frame.can_id  = CAN_ID_STATE_BROADCAST;
    frame.can_dlc = 7;

    frame.data[0] = static_cast<uint8_t>(driverState);
    frame.data[1] = static_cast<uint8_t>(passengerState);

    // Driver raw flags
    frame.data[2] = (inputs.driverBrake()      ? 0x01 : 0)
                  | (inputs.driverRunning()    ? 0x02 : 0)
                  | (inputs.driverTurn()       ? 0x04 : 0)
                  | (inputs.driverReverse()    ? 0x08 : 0);

    // Passenger raw flags
    frame.data[3] = (inputs.passengerBrake()   ? 0x01 : 0)
                  | (inputs.passengerRunning() ? 0x02 : 0)
                  | (inputs.passengerTurn()    ? 0x04 : 0)
                  | (inputs.passengerReverse() ? 0x08 : 0);

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
// ---------------------------------------------------------------------------
void CANBus::reportFault(uint8_t code, uint8_t severity,
                         uint8_t data0, uint8_t data1) {
    // Always log to Serial regardless of bus state
    Serial.printf("[FAULT] code=0x%02X sev=%u d0=%u d1=%u\n",
                  code, severity, data0, data1);

    if (!_online) return;  // bus offline — can't transmit

    struct can_frame frame;
    frame.can_id  = CAN_ID_FAULT;
    frame.can_dlc = 4;
    frame.data[0] = code;
    frame.data[1] = severity;
    frame.data[2] = data0;
    frame.data[3] = data1;

    if (_mcp.sendMessage(&frame) != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] fault frame TX error"));
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
                _overrideDriver  = static_cast<LightState>(l);
                _overridePassenger = static_cast<LightState>(r);
                _hasOverride   = true;
                Serial.print(F("[CAN] anim override driver="));
                Serial.print(l);
                Serial.print(F(" passenger="));
                Serial.println(r);
            }
            break;

        case CMD_CLEAR_OVERRIDE:
            _hasOverride = false;
            Serial.println(F("[CAN] override cleared"));
            break;

        case CMD_CUSTOM_ANIM:
            // Byte 1 : CANIM_* animation ID
            // Byte 2-3 : duration ms (big-endian; 0 = animation's built-in default)
            // Byte 4 : param0   Byte 5 : param1
            if (frame.can_dlc >= 6) {
                uint8_t  animId   = frame.data[1];
                uint16_t durMs    = ((uint16_t)frame.data[2] << 8) | frame.data[3];
                uint8_t  param0   = frame.data[4];
                uint8_t  param1   = frame.data[5];

                Serial.printf("[CAN] custom anim id=0x%02X dur=%u p0=%u p1=%u\n",
                              animId, durMs, param0, param1);

                switch (animId) {

                    case CANIM_SCROLL_BRAKE_CHECK:
                        AnimationRegistry::scrollText().set(
                            "BRAKE CHECK",
                            CRGB(220, 220, 220),
                            CRGB(30, 0, 0),
                            (durMs > 0) ? (int)durMs : 45);
                        AnimationRegistry::setCustomSlot(
                            AnimationRegistry::CustomSlot::SCROLL_TEXT);
                        _hasCustomAnim = true;
                        break;

                    case CANIM_FLASH_AMBER:
                        AnimationRegistry::flash().set(
                            (param0 > 0) ? param0 : 3,
                            (param1 > 0) ? (uint16_t)(param1) : 150,
                            CRGB(255, 140, 0));
                        AnimationRegistry::setCustomSlot(
                            AnimationRegistry::CustomSlot::FLASH);
                        _hasCustomAnim = true;
                        break;

                    case CANIM_SCROLL_2CHAR: {
                        // param0 and param1 are two ASCII characters
                        char buf[3] = { (char)param0, (char)param1, '\0' };
                        AnimationRegistry::scrollText().set(
                            buf,
                            CRGB(220, 220, 220),
                            CRGB(30, 0, 0),
                            (durMs > 0) ? (int)durMs : 45);
                        AnimationRegistry::setCustomSlot(
                            AnimationRegistry::CustomSlot::SCROLL_TEXT);
                        _hasCustomAnim = true;
                        break;
                    }

                    default:
                        Serial.printf("[CAN] unknown custom anim id 0x%02X\n", animId);
                        break;
                }
            }
            break;

        default:
            Serial.print(F("[CAN] unknown cmd 0x"));
            Serial.println(frame.data[0], HEX);
            break;
    }
}
