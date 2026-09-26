// MCP2515 CAN service. Runtime SPI work is bounded; never reset or wait for TX
// completion from the lighting loop.
#include "canbus.h"
#include "can_control.h"

static constexpr uint8_t CMD_SET_BRIGHTNESS = 0x01;
static constexpr uint8_t CMD_ANIM_OVERRIDE  = 0x02;
static constexpr uint8_t CMD_CLEAR_OVERRIDE = 0x03;
static constexpr uint8_t CMD_CUSTOM_ANIM    = 0x04;
static constexpr unsigned long CAN_POLL_MS = 2;
static constexpr unsigned long CAN_TX_TIMEOUT_MS = 20;
static constexpr unsigned long CAN_RETRY_MAX_MS = 10000;
static constexpr uint8_t REG_CANCTRL = 0x0F;
static constexpr uint8_t REG_TXB0CTRL = 0x30;
static constexpr uint8_t TXREQ = 0x08;
static constexpr uint8_t TX_ERRORS = 0x70; // ABTF, MLOA, TXERR

// The library hides register access. These two bounded transactions let us
// enable one-shot TX and inspect/abort it without polling loops or delays.
uint8_t CANBus::_readRegister(uint8_t reg) {
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CAN_CS, LOW);
    SPI.transfer(0x03); // READ
    SPI.transfer(reg);
    const uint8_t value = SPI.transfer(0);
    digitalWrite(PIN_CAN_CS, HIGH);
    SPI.endTransaction();
    return value;
}

void CANBus::_modifyRegister(uint8_t reg, uint8_t mask, uint8_t value) {
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_CAN_CS, LOW);
    SPI.transfer(0x05); // BIT MODIFY
    SPI.transfer(reg);
    SPI.transfer(mask);
    SPI.transfer(value);
    digitalWrite(PIN_CAN_CS, HIGH);
    SPI.endTransaction();
}

bool CANBus::begin() {
    if (!_spiStarted) {
        SPI.begin(PIN_CAN_SCK, PIN_CAN_MISO, PIN_CAN_MOSI, PIN_CAN_CS);
        _spiStarted = true;
    }
    return _initMCP();
}

bool CANBus::_initMCP() {
    _online = false;
    _txPending = false;
    _busOff = false;
    _txRetryDelayMs = 0;
    if (_mcp.reset() != MCP2515::ERROR_OK
        || _mcp.setBitrate(CAN_500KBPS, MCP_8MHZ) != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] init failed; CAN disabled until restart"));
        return false;
    }
    if (_mcp.setFilterMask(MCP2515::MASK0, false, 0x7FF) != MCP2515::ERROR_OK
        || _mcp.setFilterMask(MCP2515::MASK1, false, 0x7FF) != MCP2515::ERROR_OK
        || _mcp.setFilter(MCP2515::RXF0, false, CAN_ID_COMMAND) != MCP2515::ERROR_OK
        || _mcp.setFilter(MCP2515::RXF1, false, CAN_ID_COMMAND) != MCP2515::ERROR_OK
        || _mcp.setFilter(MCP2515::RXF2, false, CAN_ID_COMMAND) != MCP2515::ERROR_OK
        || _mcp.setFilter(MCP2515::RXF3, false, CAN_ID_COMMAND) != MCP2515::ERROR_OK
        || _mcp.setFilter(MCP2515::RXF4, false, CAN_ID_COMMAND) != MCP2515::ERROR_OK
        || _mcp.setFilter(MCP2515::RXF5, false, CAN_ID_COMMAND) != MCP2515::ERROR_OK
        || _mcp.setNormalMode() != MCP2515::ERROR_OK) {
        Serial.println(F("[CAN] configuration failed; CAN disabled until restart"));
        return false;
    }

    // CANCTRL.OSM prevents endless automatic retransmission without an ACK.
    // autowp 1.3.1's setNormalOneShotMode() compares CANSTAT.OPMOD with OSM
    // and falsely times out, so enable and verify OSM explicitly instead.
    _modifyRegister(REG_CANCTRL, 0x08, 0x08);
    if ((_readRegister(REG_CANCTRL) & 0xE8) != 0x08) {
        Serial.println(F("[CAN] one-shot mode readback failed; CAN disabled"));
        return false;
    }
    _online = true;
    Serial.println(F("[CAN] MCP2515 ready - 500 kbit/s, one-shot TX"));
    return true;
}

void CANBus::_txFailed(unsigned long nowMs) {
    if (_txRetryDelayMs == 0) {
        Serial.println(F("[CAN] TX unavailable; backing off (lighting continues)"));
    }
    _txRetryDelayMs = _txRetryDelayMs == 0 ? 1000UL
        : min(_txRetryDelayMs * 2, CAN_RETRY_MAX_MS);
    _lastTxFailureMs = nowMs;
    _txPending = false;
}

void CANBus::_checkTx(unsigned long nowMs) {
    if (!_txPending) return;
    const uint8_t ctrl = _readRegister(REG_TXB0CTRL);
    if (ctrl & TXREQ) {
        if (nowMs - _txStartedMs < CAN_TX_TIMEOUT_MS) return;
        _modifyRegister(REG_TXB0CTRL, TXREQ, 0); // abort stuck request
        _txFailed(nowMs);
        return;
    }
    _txPending = false;
    if (ctrl & TX_ERRORS) {
        _txFailed(nowMs);
    } else {
        // sendMessage() returning OK only means queued. Success is counted
        // here, after TXREQ clears and the controller reports no TX errors.
        if (_txRetryDelayMs) Serial.println(F("[CAN] TX recovered"));
        _txRetryDelayMs = 0;
    }
}

void CANBus::_sendFrame(const struct can_frame& frame) {
    const unsigned long nowMs = millis();
    if (!_online || _busOff || _txPending
        || (_txRetryDelayMs && nowMs - _lastTxFailureMs < _txRetryDelayMs)) return;
    // Use a single tracked TX slot. Never fill all three buffers with stale
    // telemetry, and never overwrite a request still being aborted.
    if (_readRegister(REG_TXB0CTRL) & TXREQ) {
        _modifyRegister(REG_TXB0CTRL, TXREQ, 0);
        _txFailed(nowMs);
        return;
    }
    // Setting TXREQ clears the read-only ABTF/MLOA/TXERR flags automatically.
    if (_mcp.sendMessage(MCP2515::TXB0, &frame) != MCP2515::ERROR_OK) {
        _modifyRegister(REG_TXB0CTRL, TXREQ, 0);
        _txFailed(nowMs);
        return;
    }
    _txPending = true;
    _txStartedMs = nowMs;
}

void CANBus::tick(LightState driverState, LightState passengerState,
                  const Inputs& inputs, const ThermalManager& thermal,
                  uint8_t appliedBrightness) {
    const unsigned long nowMs = millis();
    if (_demoMode && nowMs - _lastDemoStepMs >= 5000UL) {
        _lastDemoStepMs = nowMs;
        g_settings.show_anim = (g_settings.show_anim + 1U) % can_protocol::TAILLIGHT_SHOW_COUNT;
    }
    if (_hasCustomAnim && _customAnimUntilMs != 0 &&
        static_cast<int32_t>(nowMs - _customAnimUntilMs) >= 0) {
        clearCustomAnim();
        AnimationRegistry::setCustomSlot(AnimationRegistry::CustomSlot::NONE);
    }
    // Failed startup stays quiet. Retrying reset()/setMode() here used to
    // block the render loop every second when the MCP2515 was unavailable.
    if (!_online || nowMs - _lastPollMs < CAN_POLL_MS) return;
    _lastPollMs = nowMs;

    const uint8_t eflg = _mcp.getErrorFlags();
    const bool busOff = (eflg & 0x20) != 0;
    if (busOff && !_busOff) {
        _modifyRegister(REG_TXB0CTRL, TXREQ, 0);
        _txFailed(nowMs);
    }
    // The MCP2515 recovers bus-off automatically after sufficient idle bits.
    // Leave it in normal mode and poll; do not reset it from the render loop.
    _busOff = busOff;
    if (eflg & 0xC0) _mcp.clearRXnOVRFlags();
    _checkTx(nowMs);

    // RX stays available during TX backoff. Bound processing to two frames.
    struct can_frame frame;
    for (uint8_t i = 0; i < 2 && _mcp.readMessage(&frame) == MCP2515::ERROR_OK; ++i) {
        _processFrame(frame);
    }
    if (nowMs - _lastBroadcastMs >= CAN_BROADCAST_INTERVAL_MS) {
        _lastBroadcastMs = nowMs;
        _sendState(driverState, passengerState, inputs, thermal, appliedBrightness);
    }
}

void CANBus::_sendState(LightState driverState, LightState passengerState,
                        const Inputs& inputs, const ThermalManager& thermal,
                        uint8_t appliedBrightness) {
    const auto packed = taillight_can::encodeState(
        static_cast<uint8_t>(driverState), static_cast<uint8_t>(passengerState),
        inputs.driverSnapshot() & 0x0F, inputs.passengerSnapshot() & 0x0F,
        appliedBrightness, thermal.tempC(), thermal.derateAmount());
    struct can_frame frame{};
    frame.can_id = packed.id;
    frame.can_dlc = packed.dlc;
    for (uint8_t i = 0; i < packed.dlc; ++i) frame.data[i] = packed.data[i];

    _sendFrame(frame);
}

void CANBus::reportFault(uint8_t code, uint8_t severity, uint8_t data0, uint8_t data1) {
    Serial.printf("[FAULT] code=0x%02X sev=%u d0=%u d1=%u\n", code, severity, data0, data1);
    struct can_frame frame = {};
    frame.can_id = CAN_ID_FAULT;
    frame.can_dlc = 4;
    frame.data[0] = code;
    frame.data[1] = severity;
    frame.data[2] = data0;
    frame.data[3] = data1;
    _sendFrame(frame);
}

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
                _brightness.set(frame.data[1]);
                Serial.print(F("[CAN] brightness -> "));
                Serial.println(frame.data[1]);
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
