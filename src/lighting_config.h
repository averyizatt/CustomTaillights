#pragma once
#include "settings.h"
#include "config.h"
#include <ArduinoJson.h>
#include <cstring>

// Explicit list excludes WiFi credentials, hardware layout, and runtime overrides.
#define LIGHTING_FIELDS(X) \
    X(brightness, 10, 255) \
    X(brightness_dim, 5, RUNNING_BRIGHTNESS_MAX_PERCENT) \
    X(turn_blink_ms, 200, 1500) \
    X(turn_custom, 0, 1) \
    X(turn_sweep_ms, 50, 1500) \
    X(turn_hold_ms, 0, 1500) \
    X(turn_off_ms, 50, 1500) \
    X(frame_ms, 10, 100) \
    X(brake_speed, 50, 200) \
    X(reverse_speed, 50, 200) \
    X(run_speed, 50, 200) \
    X(show_speed, 50, 200) \
    X(brake_r, 0, 255) \
    X(brake_g, 0, 255) \
    X(brake_b, 0, 255) \
    X(turn_r, 0, 255) \
    X(turn_g, 0, 255) \
    X(turn_b, 0, 255) \
    X(reverse_r, 0, 255) \
    X(reverse_g, 0, 255) \
    X(reverse_b, 0, 255) \
    X(run_r, 0, 255) \
    X(run_g, 0, 255) \
    X(run_b, 0, 255) \
    X(brake_anim, 0, BRAKE_ANIM_MAX) \
    X(turn_anim, 0, TURN_ANIM_MAX) \
    X(reverse_anim, 0, REVERSE_ANIM_MAX) \
    X(run_anim, 0, RUN_ANIM_MAX) \
    X(show_anim, 0, SHOW_ANIM_MAX) \
    X(rest_mode, 0, 1)

inline void lightingToJson(JsonObject out, const Settings& s) {
#define WRITE_LIGHTING(key, low, high) out[#key] = s.key;
    LIGHTING_FIELDS(WRITE_LIGHTING)
#undef WRITE_LIGHTING
    out["show_text"] = s.show_text;
}
inline bool lightingFromJson(JsonObjectConst in, Settings& s) {
    if (in.isNull()) return false;
    // Validate the entire document before changing the destination.
#define CHECK_LIGHTING(key, low, high) if (!in[#key].is<int>() || in[#key].as<int>() < low || in[#key].as<int>() > high) return false;
    LIGHTING_FIELDS(CHECK_LIGHTING)
#undef CHECK_LIGHTING
    const char* text = in["show_text"].as<const char*>();
    if (!text || strlen(text) >= sizeof(s.show_text)) return false;
#define READ_LIGHTING(key, low, high) s.key = in[#key].as<int>();
    LIGHTING_FIELDS(READ_LIGHTING)
#undef READ_LIGHTING
    strcpy(s.show_text, text);
    return true;
}
#undef LIGHTING_FIELDS
