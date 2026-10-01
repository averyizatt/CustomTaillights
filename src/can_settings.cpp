// Lighting settings over CAN (can_protocol.h extension 3). See can_settings.h.
#include "can_settings.h"
#include "profiles.h"
#include <can_contract/can_protocol.h>
#include <cstdio>
#include <cstring>

namespace can_settings {
namespace {
namespace key = can_protocol::taillight_setting;

uint8_t g_profileSlots = 0;
bool g_profileSlotsKnown = false;

// Byte or word field for each key. Ranges come from the shared contract.
struct Field { uint8_t* u8; uint16_t* u16; };
Field field(uint8_t k) {
    Settings& s = g_settings;
    switch (k) {
        case key::BRIGHTNESS: return {&s.brightness, nullptr};
        case key::BRIGHTNESS_DIM: return {&s.brightness_dim, nullptr};
        case key::TURN_BLINK_MS: return {nullptr, &s.turn_blink_ms};
        case key::TURN_CUSTOM: return {&s.turn_custom, nullptr};
        case key::TURN_SWEEP_MS: return {nullptr, &s.turn_sweep_ms};
        case key::TURN_HOLD_MS: return {nullptr, &s.turn_hold_ms};
        case key::TURN_OFF_MS: return {nullptr, &s.turn_off_ms};
        case key::BRAKE_SPEED: return {&s.brake_speed, nullptr};
        case key::REVERSE_SPEED: return {&s.reverse_speed, nullptr};
        case key::RUN_SPEED: return {&s.run_speed, nullptr};
        case key::FRAME_MS: return {&s.frame_ms, nullptr};
        case key::BRAKE_ANIM: return {&s.brake_anim, nullptr};
        case key::TURN_ANIM: return {&s.turn_anim, nullptr};
        case key::REVERSE_ANIM: return {&s.reverse_anim, nullptr};
        case key::RUN_ANIM: return {&s.run_anim, nullptr};
        case key::LENS_PRESET: return {&s.lens_preset, nullptr};
        case key::STARTUP_ANIM: return {&s.startup_anim, nullptr};
        case key::REST_MODE: return {&s.rest_mode, nullptr};
        case key::SHOW_SPEED: return {&s.show_speed, nullptr};
        case key::SHOW_ANIM: return {&s.show_anim, nullptr};
        case key::SHOW_MODE: return {&s.show_mode, nullptr};
        default: return {nullptr, nullptr};
    }
}

struct Rgb { uint8_t* r; uint8_t* g; uint8_t* b; };
Rgb colorField(uint8_t which) {
    Settings& s = g_settings;
    switch (which) {
        case can_protocol::taillight_color::BRAKE: return {&s.brake_r, &s.brake_g, &s.brake_b};
        case can_protocol::taillight_color::TURN: return {&s.turn_r, &s.turn_g, &s.turn_b};
        case can_protocol::taillight_color::REVERSE: return {&s.reverse_r, &s.reverse_g, &s.reverse_b};
        case can_protocol::taillight_color::RUNNING: return {&s.run_r, &s.run_g, &s.run_b};
        default: return {nullptr, nullptr, nullptr};
    }
}
}  // namespace

uint16_t get(uint8_t k) {
    const Field f = field(k);
    return f.u8 ? *f.u8 : f.u16 ? *f.u16 : 0;
}

bool set(uint8_t k, uint16_t value, uint16_t& applied, bool& clamped) {
    can_protocol::TaillightSettingRange range{};
    const Field f = field(k);
    if (!can_protocol::taillightSettingRange(k, range) || (!f.u8 && !f.u16)) return false;
    applied = value < range.low ? range.low : value > range.high ? range.high : value;
    clamped = applied != value;
    if (f.u8) *f.u8 = static_cast<uint8_t>(applied);
    else *f.u16 = applied;
    return true;
}

bool setColor(uint8_t which, uint8_t red, uint8_t green, uint8_t blue) {
    const Rgb rgb = colorField(which);
    if (!rgb.r) return false;
    *rgb.r = red; *rgb.g = green; *rgb.b = blue;
    return true;
}

bool color(uint8_t which, uint8_t& red, uint8_t& green, uint8_t& blue) {
    const Rgb rgb = colorField(which);
    if (!rgb.r) return false;
    red = *rgb.r; green = *rgb.g; blue = *rgb.b;
    return true;
}

bool setTextChunk(uint8_t offset, const uint8_t* chars, uint8_t length) {
    char* text = g_settings.show_text;
    if (offset > can_protocol::TAILLIGHT_SHOW_TEXT_MAX) return false;
    if (offset == 0) std::memset(text, 0, sizeof(g_settings.show_text));
    uint8_t i = 0;
    for (; i < length && chars[i] && offset + i < can_protocol::TAILLIGHT_SHOW_TEXT_MAX; ++i) {
        const char c = static_cast<char>(chars[i]);
        text[offset + i] = (c >= 0x20 && c <= 0x7E) ? c : '?';
    }
    if (i < 6) text[offset + i] = '\0';  // Short or NUL-terminated chunk ends the text.
    return true;
}

uint8_t profileSlots() {
    if (!g_profileSlotsKnown) refreshProfileSlots();
    return g_profileSlots;
}

void refreshProfileSlots() {
    char name[PROFILE_NAME_SIZE];
    Settings scratch = g_settings;
    g_profileSlots = 0;
    for (int slot = 0; slot < PROFILE_COUNT; ++slot) {
        if (profile_read(slot, name, scratch)) g_profileSlots |= static_cast<uint8_t>(1U << slot);
    }
    g_profileSlotsKnown = true;
}

uint8_t action(uint8_t kind, uint8_t argument) {
    namespace act = can_protocol::taillight_action;
    namespace status = can_protocol::config_ack_status;
    const bool slotAction = kind >= act::PROFILE_LOAD && kind <= act::PROFILE_DELETE;
    if (slotAction && argument >= can_protocol::TAILLIGHT_PROFILE_COUNT) return status::VALUE_CLAMPED;
    switch (kind) {
        case act::SAVE:
            return settings_save() ? status::OK : can_protocol::TAILLIGHT_ACK_SAVE_FAILED;
        case act::REVERT:
            settings_revert();
            return status::OK;
        case act::FACTORY_DEFAULTS: {
            // Defaults for lighting only; keep network credentials as they are.
            const Settings previous = g_settings;
            settings_reset();
            g_settings.wifi_mode = previous.wifi_mode;
            std::memcpy(g_settings.ap_ssid, previous.ap_ssid, sizeof(previous.ap_ssid));
            std::memcpy(g_settings.ap_pass, previous.ap_pass, sizeof(previous.ap_pass));
            std::memcpy(g_settings.sta_ssid, previous.sta_ssid, sizeof(previous.sta_ssid));
            std::memcpy(g_settings.sta_pass, previous.sta_pass, sizeof(previous.sta_pass));
            return status::OK;
        }
        case act::REPORT:
            return status::OK;
        case act::PROFILE_LOAD: {
            char name[PROFILE_NAME_SIZE];
            Settings loaded = g_settings;
            if (!profile_read(argument, name, loaded)) return can_protocol::TAILLIGHT_ACK_NOT_FOUND;
            loaded.show_mode = g_settings.show_mode;  // A profile never starts or stops a show.
            g_settings = loaded;
            return status::OK;
        }
        case act::PROFILE_SAVE: {
            char name[PROFILE_NAME_SIZE];
            Settings existing = g_settings;
            if (!profile_read(argument, name, existing)) std::snprintf(name, sizeof(name), "Profile %u", argument + 1U);
            const bool ok = profile_write(argument, name, g_settings);
            refreshProfileSlots();
            return ok ? status::OK : can_protocol::TAILLIGHT_ACK_SAVE_FAILED;
        }
        case act::PROFILE_DELETE: {
            const bool ok = profile_delete(argument);
            refreshProfileSlots();
            return ok ? status::OK : can_protocol::TAILLIGHT_ACK_SAVE_FAILED;
        }
        default:
            return status::UNSUPPORTED_COMMAND;
    }
}

}  // namespace can_settings
