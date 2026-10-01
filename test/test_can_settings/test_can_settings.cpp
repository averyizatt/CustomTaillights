// Settings over CAN (can_protocol.h extension 3) with the real CAN service,
// settings, profiles and JSON code against the fake MCP2515 and Preferences.
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "canbus.h"
#include "can_settings.h"
#include "lighting_config.h"
#include "profiles.h"
#include "settings.h"
#include <Preferences.h>

unsigned long testMillis = 0;
AnimScrollText AnimationRegistry::_scrollText;
AnimFlash AnimationRegistry::_flash;
AnimationRegistry::CustomSlot AnimationRegistry::_customSlot = AnimationRegistry::CustomSlot::NONE;
void AnimScrollText::set(const char*, CRGB, CRGB, int) {}
void AnimScrollText::begin(TailLight&, LightState) {}
void AnimScrollText::update(TailLight&, LightState, unsigned long) {}
void AnimScrollText::end(TailLight&) {}
void AnimFlash::set(uint8_t, uint16_t, CRGB) {}
void AnimFlash::begin(TailLight&, LightState) {}
void AnimFlash::update(TailLight&, LightState, unsigned long) {}
void AnimFlash::end(TailLight&) {}

namespace cp = can_protocol;

struct Bench {
    CANBus bus;
    Bench() {
        testMillis = 0;
        std::fill(std::begin(registers), std::end(registers), 0);
        MCP2515::sent.clear(); MCP2515::incoming.clear();
        Preferences::store.clear();
        settings_reset();
        settings_load();
        can_settings::refreshProfileSlots();
        assert(bus.begin());
    }
    // Advance time; every transmission completes successfully (peer ACK).
    void run(unsigned long ms) {
        Inputs inputs; ThermalManager thermal;
        const unsigned long end = testMillis + ms;
        for (; testMillis < end; testMillis += 2) {
            bus.tick(LightState::RUNNING, LightState::RUNNING, inputs, thermal, 128);
            if (registers[0x30] & 0x08) registers[0x30] = 0;
        }
    }
    void send(std::vector<uint8_t> bytes) {
        can_frame frame; frame.can_id = CAN_ID_COMMAND; frame.can_dlc = static_cast<uint8_t>(bytes.size());
        std::copy(bytes.begin(), bytes.end(), frame.data);
        MCP2515::incoming.push_back(frame);
    }
    std::vector<can_frame> reports(uint8_t kind) {
        std::vector<can_frame> out;
        for (const auto& f : MCP2515::sent) if (f.can_id == cp::ID_TAILLIGHT_STATUS && f.data[0] == kind) out.push_back(f);
        return out;
    }
    can_frame lastAck() { auto acks = reports(cp::taillight_report::ACK); assert(!acks.empty()); return acks.back(); }
};

void test_every_existing_command_is_acknowledged() {
    Bench b;
    const std::vector<std::pair<std::vector<uint8_t>, uint8_t>> cases = {
        {{0x01, 73}, cp::config_ack_status::OK},
        {{0x01}, cp::config_ack_status::INVALID_LENGTH},
        {{0x02, 2, 9}, cp::config_ack_status::VALUE_CLAMPED},
        {{0x03}, cp::config_ack_status::OK},
        {{0x04, 0x02, 0, 0, 3, 150}, cp::config_ack_status::OK},
        {{0x04, 0x7F, 0, 0, 0, 0}, cp::config_ack_status::UNSUPPORTED_COMMAND},
        {{0x05, 2, 40}, cp::config_ack_status::UNSUPPORTED_COMMAND},
        {{0x7F}, cp::config_ack_status::UNSUPPORTED_COMMAND}};
    for (const auto& [bytes, status] : cases) {
        b.send(bytes); b.run(20);
        const can_frame ack = b.lastAck();
        assert(ack.data[1] == bytes[0] && ack.data[2] == status && ack.data[7] == cp::CAN_PROTOCOL_SCHEMA_VERSION);
    }
}

void test_settings_are_clamped_versioned_and_reported() {
    Bench b;
    b.run(600);
    const uint8_t start = b.reports(cp::taillight_report::STATUS).back().data[1];
    b.send({0x06, cp::taillight_setting::TURN_ANIM, 0, 5}); b.run(20);
    assert(g_settings.turn_anim == 5 && b.lastAck().data[2] == cp::config_ack_status::OK);
    b.send({0x06, cp::taillight_setting::FRAME_MS, 0, 5}); b.run(20);
    assert(g_settings.frame_ms == 10 && b.lastAck().data[2] == cp::config_ack_status::VALUE_CLAMPED && b.lastAck().data[5] == 10);
    b.send({0x06, 40, 0, 1}); b.run(20);
    assert(b.lastAck().data[2] == cp::config_ack_status::UNSUPPORTED_COMMAND);
    b.send({0x06, cp::taillight_setting::TURN_ANIM, 0}); b.run(20);
    assert(b.lastAck().data[2] == cp::config_ack_status::INVALID_LENGTH);
    b.send({0x07, cp::taillight_color::TURN, 255, 100, 7}); b.run(20);
    assert(g_settings.turn_r == 255 && g_settings.turn_g == 100 && g_settings.turn_b == 7);
    b.send({0x08, 0, 'F', 'O', 'X', 'B', 'O', 'D'}); b.send({0x08, 6, 'Y', ' ', '5', '.', '0'}); b.run(40);
    assert(std::string(g_settings.show_text) == "FOXBODY 5.0");
    b.run(600);
    const can_frame status = b.reports(cp::taillight_report::STATUS).back();
    assert(static_cast<uint8_t>(status.data[1] - start) == 5);  // Three settings, one color, two text chunks... minus the rejected ones.
    assert(status.data[2] & cp::taillight_status_flag::UNSAVED);
    // Full report: every key, every color and the text, drained without starving 0x100.
    MCP2515::sent.clear();
    b.send({0x09, cp::taillight_action::REPORT, 0}); b.run(1000);
    const auto settings = b.reports(cp::taillight_report::SETTING), colors = b.reports(cp::taillight_report::COLOR);
    assert(settings.size() == cp::taillight_setting::COUNT && colors.size() == cp::taillight_color::COUNT);
    for (const auto& f : settings) assert(cp::decodeU16BE(f.data[2], f.data[3]) == can_settings::get(f.data[1]));
    std::string text;
    for (const auto& f : b.reports(cp::taillight_report::TEXT)) {
        for (int i = 2; i < 8 && f.data[i]; ++i) text += static_cast<char>(f.data[i]);
    }
    assert(text == "FOXBODY 5.0");
    int states = 0;
    for (const auto& f : MCP2515::sent) states += f.can_id == cp::ID_TAILLIGHT_STATE;
    assert(states >= 9 && states <= 11);  // 100 ms broadcast kept its cadence.
}

void test_save_revert_defaults_and_profiles() {
    Bench b;
    b.send({0x06, cp::taillight_setting::BRAKE_ANIM, 0, 3}); b.run(20);
    b.send({0x09, cp::taillight_action::SAVE}); b.run(20);
    assert(b.lastAck().data[2] == cp::config_ack_status::OK && !settings_pending());
    b.send({0x06, cp::taillight_setting::BRAKE_ANIM, 0, 6}); b.run(20);
    b.send({0x09, cp::taillight_action::REVERT, 0}); b.run(20);
    assert(g_settings.brake_anim == 3);
    b.send({0x09, cp::taillight_action::PROFILE_SAVE, 2}); b.run(20);
    assert(can_settings::profileSlots() == 1 << 2);
    b.send({0x09, cp::taillight_action::FACTORY_DEFAULTS, 0}); b.run(20);
    assert(g_settings.brake_anim == 0 && settings_pending());
    b.send({0x09, cp::taillight_action::PROFILE_LOAD, 2}); b.run(20);
    assert(g_settings.brake_anim == 3 && b.lastAck().data[2] == cp::config_ack_status::OK);
    b.send({0x09, cp::taillight_action::PROFILE_LOAD, 4}); b.run(20);
    assert(b.lastAck().data[2] == cp::TAILLIGHT_ACK_NOT_FOUND);
    b.send({0x09, cp::taillight_action::PROFILE_DELETE, 2}); b.run(20);
    assert(can_settings::profileSlots() == 0);
    b.send({0x09, cp::taillight_action::PROFILE_SAVE, 9}); b.run(20);
    assert(b.lastAck().data[2] == cp::config_ack_status::VALUE_CLAMPED);
    b.run(600);
    assert(b.reports(cp::taillight_report::STATUS).back().data[4] == 0);
}

void test_show_mode_setting_and_status_flags() {
    Bench b;
    b.send({0x06, cp::taillight_setting::SHOW_ANIM, 0, 34}); b.send({0x06, cp::taillight_setting::SHOW_MODE, 0, 1}); b.run(600);
    const can_frame status = b.reports(cp::taillight_report::STATUS).back();
    assert(g_settings.show_anim == 34 && (status.data[2] & cp::taillight_status_flag::SHOW) && status.data[3] == 34);
    b.send({0x03}); b.run(600);
    assert(!g_settings.show_mode && !(b.reports(cp::taillight_report::STATUS).back().data[2] & cp::taillight_status_flag::SHOW));
    b.bus.setAnimationStart(testMillis - 3200); b.run(600);
    const can_frame later = b.reports(cp::taillight_report::STATUS).back();
    assert(cp::decodeU16BE(later.data[5], later.data[6]) >= 200);  // Phase in 16 ms steps.
}

void test_contract_ranges_match_firmware_limits() {
    // The web/profile validator (LIGHTING_FIELDS) must accept exactly the contract range.
    const std::pair<const char*, uint8_t> keys[] = {
        {"brightness", 1}, {"brightness_dim", 2}, {"turn_blink_ms", 3}, {"turn_custom", 4}, {"turn_sweep_ms", 5},
        {"turn_hold_ms", 6}, {"turn_off_ms", 7}, {"brake_speed", 8}, {"reverse_speed", 9}, {"run_speed", 10},
        {"frame_ms", 11}, {"brake_anim", 12}, {"turn_anim", 13}, {"reverse_anim", 14}, {"run_anim", 15},
        {"rest_mode", 18}, {"show_speed", 19}, {"show_anim", 20}};
    settings_reset();
    auto accepts = [](const char* name, long value) {
        JsonDocument doc;
        lightingToJson(doc.to<JsonObject>(), g_settings);
        doc[name] = value;
        Settings scratch = g_settings;
        return lightingFromJson(doc.as<JsonObjectConst>(), scratch);
    };
    for (const auto& [name, key] : keys) {
        cp::TaillightSettingRange range{};
        assert(cp::taillightSettingRange(key, range));
        assert(accepts(name, range.low) && accepts(name, range.high));
        assert(!accepts(name, long(range.low) - 1) && !accepts(name, long(range.high) + 1));
    }
}

int main() {
    test_every_existing_command_is_acknowledged();
    test_settings_are_clamped_versioned_and_reported();
    test_save_revert_defaults_and_profiles();
    test_show_mode_setting_and_status_flags();
    test_contract_ranges_match_firmware_limits();
    std::cout << "CAN settings tests passed\n";
}
