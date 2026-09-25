// ---------------------------------------------------------------------------
// canbus.cpp
// MCP2515 SPI CAN bus interface.
// ---------------------------------------------------------------------------

#include "canbus.h"
#include "can_control.h"

static constexpr uint8_t CMD_SET_BRIGHTNESS = can_protocol::taillight_command::SET_BRIGHTNESS;
static constexpr uint8_t CMD_ANIM_OVERRIDE  = can_protocol::taillight_command::SET_OVERRIDE;
static constexpr uint8_t CMD_CLEAR_OVERRIDE = can_protocol::taillight_command::CLEAR_OVERRIDE;
static constexpr uint8_t CMD_CUSTOM_ANIM    = can_protocol::taillight_command::TRIGGER_CUSTOM_ANIMATION;
static constexpr unsigned long CAN_RETRY_MS = 1000;
static constexpr uint8_t CAN_TX_FAILURE_LIMIT = 8;

static bool deadlineReached(unsigned long nowMs, unsigned long deadlineMs) {
    return deadlineMs != 0 && static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

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
        _busOffRetryMs = millis() + CAN_RETRY_MS;
        return false;
    }

    // Accept only CAN_ID_COMMAND frames; mask & filter on bits 10:0.
    _mcp.setFilterMask(MCP2515::MASK0, false, 0x7FF);
    _mcp.setFilter(MCP2515::RXF0,      false, CAN_ID_COMMAND);
    _mcp.setFilter(MCP2515::RXF1,      false, CAN_ID_COMMAND);
    _mcp.setFilterMask(MCP2515::MASK1, false, 0x7FF);
    _mcp.setFilter(MCP2515::RXF2,      false, CAN_ID_COMMAND);
    _mcp.setFilter(MCP2515::RXF3,      false, CAN_ID_COMMAND);
    _mcp.setFilter(MCP2515::RXF4,      false, CAN_ID_COMMAND);
    _mcp.setFilter(MCP2515::RXF5,      false, CAN_ID_COMMAND);

    if (_mcp.setNormalMode() != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] setNormalMode failed"));
        _online = false;
        _busOffRetryMs = millis() + CAN_RETRY_MS;
        return false;
    }

    _online        = true;
    _busOffRetryMs = 0;
    _consecutiveTxFailures = 0;
    Serial.println(F("[CAN] MCP2515 online — 500 kbit/s"));
    return true;
}

// ---------------------------------------------------------------------------
void CANBus::tick(LightState driverState, LightState passengerState,
                  const Inputs& inputs, const ThermalManager& thermal) {
    if (_demoMode && millis() - _lastDemoStepMs >= 5000UL) {
        _lastDemoStepMs = millis();
        g_settings.show_anim = (g_settings.show_anim + 1U) % can_protocol::TAILLIGHT_SHOW_COUNT;
    }
    if (_hasCustomAnim && deadlineReached(millis(), _customAnimUntilMs)) {
        clearCustomAnim();
        AnimationRegistry::setCustomSlot(AnimationRegistry::CustomSlot::NONE);
    }
    // ── Bus-off detection and recovery ───────────────────────────────────────────
    // EFLG bit5 = TXBO (transmit bus-off).  This happens when the TX error
    // counter reaches 256 — usually a wiring fault or missing termination.
    // We schedule a reinit 250 ms later to allow the bus to settle.
    if (_online) {
        uint8_t eflg = _mcp.getErrorFlags();
        if (eflg & 0x20) {   // TXBO — bus-off
            _online        = false;
            _busOffRetryMs = millis() + CAN_RETRY_MS;
            Serial.println(F("[CAN] bus-off detected — retry scheduled"));
        }
        if (eflg & 0xC0) {   // RX0OVR / RX1OVR — receive buffer overflow
            _mcp.clearRXnOVRFlags();
            Serial.println(F("[CAN] RX overflow cleared"));
        }
    }
    if (!_online) {
        if (deadlineReached(millis(), _busOffRetryMs)) {
            Serial.println(F("[CAN] attempting controller recovery ..."));
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
    uint8_t framesRead = 0;
    while (framesRead < 8 && _mcp.readMessage(&frame) == MCP2515::ERROR_OK) {
        _processFrame(frame);
        ++framesRead;
    }
}

// ---------------------------------------------------------------------------
void CANBus::_sendState(LightState driverState, LightState passengerState,
                        const Inputs& inputs, const ThermalManager& thermal) {
    can_protocol::TaillightState state{};
    state.left_state = static_cast<uint8_t>(driverState);
    state.right_state = static_cast<uint8_t>(passengerState);

    // Driver raw flags
    state.driver_input_flags = (inputs.driverBrake()      ? 0x01 : 0)
                  | (inputs.driverRunning()    ? 0x02 : 0)
                  | (inputs.driverTurn()       ? 0x04 : 0)
                  | (inputs.driverReverse()    ? 0x08 : 0);

    // Passenger raw flags
    state.passenger_input_flags = (inputs.passengerBrake()   ? 0x01 : 0)
                  | (inputs.passengerRunning() ? 0x02 : 0)
                  | (inputs.passengerTurn()    ? 0x04 : 0)
                  | (inputs.passengerReverse() ? 0x08 : 0);

    state.brightness = FastLED.getBrightness();
    // Byte 5: die temperature in °C, clamped to 0-255
    int tempRounded = static_cast<int>(thermal.tempC() + 0.5f);
    state.die_temp_c = can_protocol::clampU8(tempRounded);
    // Byte 6: thermal derate amount (0=none, 255=maximum)
    state.thermal_derate = thermal.derateAmount();
    const auto packed = can_protocol::packTaillightState(state);
    struct can_frame frame{};
    frame.can_id = packed.id;
    frame.can_dlc = packed.dlc;
    for (uint8_t i = 0; i < packed.dlc; ++i) frame.data[i] = packed.data[i];

    if (_mcp.sendMessage(&frame) != MCP2515::ERROR_OK) {
        if (_consecutiveTxFailures < 255) ++_consecutiveTxFailures;
        Serial.println(F("[CAN] TX error"));
        if (_consecutiveTxFailures >= CAN_TX_FAILURE_LIMIT) {
            _online = false;
            _busOffRetryMs = millis() + CAN_RETRY_MS;
            Serial.println(F("[CAN] repeated TX failures — controller recovery scheduled"));
        }
    } else {
        _consecutiveTxFailures = 0;
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
    if (frame.can_id != CAN_ID_COMMAND || frame.can_dlc < 1 || frame.can_dlc > 8) return;

    switch (frame.data[0]) {

        case can_protocol::taillight_command::SET_MODE: {
            can_protocol::CanFrame command{};
            command.id = frame.can_id;
            command.dlc = frame.can_dlc;
            for (uint8_t i = 0; i < frame.can_dlc && i < 8; ++i) command.data[i] = frame.data[i];
            if (!applyCanMode(g_settings, command)) break;
            _hasOverride = false;
            _hasCustomAnim = false;
            AnimationRegistry::setCustomSlot(AnimationRegistry::CustomSlot::NONE);
            _demoMode = frame.data[1] == can_protocol::taillight_mode::DEMO;
            _lastDemoStepMs = millis();
            break;
        }

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
            _hasCustomAnim = false;
            _demoMode = false;
            g_settings.show_mode = 0;
            AnimationRegistry::setCustomSlot(AnimationRegistry::CustomSlot::NONE);
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
                // Duration limits runtime; scrolling uses a fixed column speed.
                _customAnimUntilMs = durMs ? millis() + durMs : 0;

                Serial.printf("[CAN] custom anim id=0x%02X dur=%u p0=%u p1=%u\n",
                              animId, durMs, param0, param1);

                switch (animId) {

                    case CANIM_SCROLL_BRAKE_CHECK:
                        AnimationRegistry::scrollText().set(
                            "BRAKE CHECK",
                            CRGB(220, 220, 220),
                            CRGB(30, 0, 0),
                            45);
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
                            45);
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
