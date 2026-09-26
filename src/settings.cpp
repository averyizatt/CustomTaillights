// ---------------------------------------------------------------------------
// settings.cpp
// Persistent settings storage using the ESP32 NVS Preferences library.
// All settings are stored under the "tailsettings" namespace, separate from
// the fault-counter namespace ("tailfaults") used in main.cpp.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "settings.h"
#include "config.h"

#include <Preferences.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Compiled-in factory defaults.
// ---------------------------------------------------------------------------
static const Settings kDefaults = {
    /* brightness     */ BRIGHTNESS_DEFAULT,
    /* brightness_dim */ BRIGHTNESS_DIM,
    /* turn_blink_ms  */ 600,
    /* custom turn    */ 0, 300, 0, 300,
    /* mode speeds    */ 100, 100, 100,
    /* frame_ms       */ 20,

    /* brake_r/g/b    */ 255, 0,   0,
    /* turn_r/g/b     */ 255, 100, 0,
    /* reverse_r/g/b  */ 255, 255, 255,
    /* run_r/g/b      */  30,   0,   0,  // dim red parking glow

    /* brake_anim     */ 0,
    /* turn_anim      */ 0,
    /* reverse_anim   */ 0,
    /* run_anim       */ 0,
    /* lens_preset    */ 0,

    /* startup_anim   */ 1,  // play startup animation by default

    /* rest_mode      */ 0,   // rest mode off by default

    /* show_mode      */ 0,   // show mode off by default
    /* show_anim      */ 0,   // rainbow
    /* show_speed     */ 100, // normal speed
    /* show_text      */ "FOXBODY MUSTANG",

    /* wifi_mode      */ 0,         // AP by default — always reachable
    /* ap_ssid        */ "Foxbody-Taillights",
    /* ap_pass        */ "foxbody1",  // WPA2 min 8 chars
    /* sta_ssid       */ "",
    /* sta_pass       */ "",
};

// Global settings instance.
Settings g_settings;
static Settings savedSettings = kDefaults;

// ---------------------------------------------------------------------------
void settings_reset() {
    g_settings = kDefaults;
}

// ---------------------------------------------------------------------------
void settings_load() {
    // Start from defaults so any key missing from NVS uses the right value.
    settings_reset();

    Preferences prefs;
    if (!prefs.begin("tailsettings", /*readOnly=*/true)) {
        savedSettings = g_settings;
        // NVS namespace not yet created — first boot, keep defaults.
        return;
    }

    g_settings.brightness     = prefs.getUChar ("brightness",     kDefaults.brightness);
    g_settings.brightness_dim = prefs.getUChar ("brightness_dim", kDefaults.brightness_dim);
    g_settings.turn_blink_ms  = prefs.getUShort("turn_blink_ms",  kDefaults.turn_blink_ms);
    g_settings.turn_custom = prefs.getUChar("turn_custom", kDefaults.turn_custom);
    g_settings.turn_sweep_ms = prefs.getUShort("turn_sweep_ms", kDefaults.turn_sweep_ms);
    g_settings.turn_hold_ms = prefs.getUShort("turn_hold_ms", kDefaults.turn_hold_ms);
    g_settings.turn_off_ms = prefs.getUShort("turn_off_ms", kDefaults.turn_off_ms);
    g_settings.brake_speed = prefs.getUChar("brake_speed", kDefaults.brake_speed);
    g_settings.reverse_speed = prefs.getUChar("reverse_speed", kDefaults.reverse_speed);
    g_settings.run_speed = prefs.getUChar("run_speed", kDefaults.run_speed);
    g_settings.frame_ms       = prefs.getUChar ("frame_ms",       kDefaults.frame_ms);

    g_settings.brake_r   = prefs.getUChar("brake_r",   kDefaults.brake_r);
    g_settings.brake_g   = prefs.getUChar("brake_g",   kDefaults.brake_g);
    g_settings.brake_b   = prefs.getUChar("brake_b",   kDefaults.brake_b);

    g_settings.turn_r    = prefs.getUChar("turn_r",    kDefaults.turn_r);
    g_settings.turn_g    = prefs.getUChar("turn_g",    kDefaults.turn_g);
    g_settings.turn_b    = prefs.getUChar("turn_b",    kDefaults.turn_b);

    g_settings.reverse_r = prefs.getUChar("reverse_r", kDefaults.reverse_r);
    g_settings.reverse_g = prefs.getUChar("reverse_g", kDefaults.reverse_g);
    g_settings.reverse_b = prefs.getUChar("reverse_b", kDefaults.reverse_b);

    g_settings.run_r = prefs.getUChar("run_r", kDefaults.run_r);
    g_settings.run_g = prefs.getUChar("run_g", kDefaults.run_g);
    g_settings.run_b = prefs.getUChar("run_b", kDefaults.run_b);

    g_settings.brake_anim   = prefs.getUChar("brake_anim",   kDefaults.brake_anim);
    g_settings.turn_anim    = prefs.getUChar("turn_anim",    kDefaults.turn_anim);
    g_settings.reverse_anim = prefs.getUChar("reverse_anim", kDefaults.reverse_anim);
    g_settings.run_anim     = prefs.getUChar("run_anim",     kDefaults.run_anim);
    g_settings.lens_preset  = prefs.getUChar("lens_preset",  kDefaults.lens_preset);

    g_settings.startup_anim = prefs.getUChar("startup_anim", kDefaults.startup_anim);
    g_settings.rest_mode    = prefs.getUChar("rest_mode",    kDefaults.rest_mode);

    // show_mode is intentionally NOT loaded from NVS so it always resets to
    // off (0) on every power cycle.  The user must re-enable it each session.
    g_settings.show_anim  = prefs.getUChar("show_anim",  kDefaults.show_anim);
    g_settings.show_speed = prefs.getUChar("show_speed", kDefaults.show_speed);
    prefs.getString("show_text", g_settings.show_text, sizeof(g_settings.show_text));
    if (g_settings.show_text[0] == '\0')
        strncpy(g_settings.show_text, kDefaults.show_text, sizeof(g_settings.show_text) - 1);

    g_settings.wifi_mode = prefs.getUChar("wifi_mode", kDefaults.wifi_mode);

    prefs.getString("ap_ssid",  g_settings.ap_ssid,  sizeof(g_settings.ap_ssid));
    prefs.getString("ap_pass",  g_settings.ap_pass,  sizeof(g_settings.ap_pass));
    prefs.getString("sta_ssid", g_settings.sta_ssid, sizeof(g_settings.sta_ssid));
    prefs.getString("sta_pass", g_settings.sta_pass, sizeof(g_settings.sta_pass));

    prefs.end();

    // NVS can contain stale values from older firmware or corrupted writes.
    // Clamp after loading so animation math never sees zero periods or
    // out-of-range selectors.
    g_settings.brightness     = (uint8_t)constrain(g_settings.brightness, 10, 255);
    g_settings.brightness_dim = (uint8_t)constrain(g_settings.brightness_dim, 5, RUNNING_BRIGHTNESS_MAX_PERCENT);
    g_settings.turn_blink_ms  = (uint16_t)constrain((int)g_settings.turn_blink_ms, 200, 1500);
    g_settings.turn_custom = constrain(g_settings.turn_custom, 0, 1);
    g_settings.turn_sweep_ms = constrain(g_settings.turn_sweep_ms, 50, 1500);
    g_settings.turn_hold_ms = constrain(g_settings.turn_hold_ms, 0, 1500);
    g_settings.turn_off_ms = constrain(g_settings.turn_off_ms, 50, 1500);
    g_settings.brake_speed = (uint8_t)constrain(g_settings.brake_speed, 50, 200);
    g_settings.reverse_speed = (uint8_t)constrain(g_settings.reverse_speed, 50, 200);
    g_settings.run_speed = (uint8_t)constrain(g_settings.run_speed, 50, 200);
    g_settings.frame_ms       = (uint8_t)constrain(g_settings.frame_ms, 10, 100);

    g_settings.brake_anim     = (uint8_t)constrain(g_settings.brake_anim, 0, BRAKE_ANIM_MAX);
    g_settings.turn_anim      = (uint8_t)constrain(g_settings.turn_anim, 0, TURN_ANIM_MAX);
    g_settings.reverse_anim   = (uint8_t)constrain(g_settings.reverse_anim, 0, REVERSE_ANIM_MAX);
    g_settings.run_anim       = (uint8_t)constrain(g_settings.run_anim, 0, RUN_ANIM_MAX);
    g_settings.lens_preset    = (uint8_t)constrain(g_settings.lens_preset, 0, 3);
    g_settings.startup_anim   = (uint8_t)constrain(g_settings.startup_anim, 0, 1);
    g_settings.rest_mode      = (uint8_t)constrain(g_settings.rest_mode, 0, 1);
    g_settings.show_mode      = 0;
    g_settings.show_anim      = (uint8_t)constrain(g_settings.show_anim, 0, SHOW_ANIM_MAX);
    g_settings.show_speed     = (uint8_t)constrain(g_settings.show_speed, 50, 200);
    g_settings.wifi_mode      = (uint8_t)constrain(g_settings.wifi_mode, 0, 1);
    savedSettings = g_settings;
}

// ---------------------------------------------------------------------------
bool settings_save() {
    Preferences prefs;
    if (!prefs.begin("tailsettings", /*readOnly=*/false)) return false;
    bool ok = true;
    auto putText = [&prefs](const char* key, const char* value) {
        if (value[0]) return prefs.putString(key, value) > 0;
        return !prefs.isKey(key) || prefs.remove(key);
    };

    ok = (prefs.putUChar ("brightness",     g_settings.brightness) > 0) && ok;
    ok = (prefs.putUChar ("brightness_dim", g_settings.brightness_dim) > 0) && ok;
    ok = (prefs.putUShort("turn_blink_ms",  g_settings.turn_blink_ms) > 0) && ok;
    ok = (prefs.putUChar("turn_custom", g_settings.turn_custom) > 0) && ok;
    ok = (prefs.putUShort("turn_sweep_ms", g_settings.turn_sweep_ms) > 0) && ok;
    ok = (prefs.putUShort("turn_hold_ms", g_settings.turn_hold_ms) > 0) && ok;
    ok = (prefs.putUShort("turn_off_ms", g_settings.turn_off_ms) > 0) && ok;
    ok = (prefs.putUChar("brake_speed", g_settings.brake_speed) > 0) && ok;
    ok = (prefs.putUChar("reverse_speed", g_settings.reverse_speed) > 0) && ok;
    ok = (prefs.putUChar("run_speed", g_settings.run_speed) > 0) && ok;
    ok = (prefs.putUChar ("frame_ms",       g_settings.frame_ms) > 0) && ok;

    ok = (prefs.putUChar("brake_r", g_settings.brake_r) > 0) && ok;
    ok = (prefs.putUChar("brake_g", g_settings.brake_g) > 0) && ok;
    ok = (prefs.putUChar("brake_b", g_settings.brake_b) > 0) && ok;

    ok = (prefs.putUChar("turn_r",  g_settings.turn_r) > 0) && ok;
    ok = (prefs.putUChar("turn_g",  g_settings.turn_g) > 0) && ok;
    ok = (prefs.putUChar("turn_b",  g_settings.turn_b) > 0) && ok;

    ok = (prefs.putUChar("reverse_r", g_settings.reverse_r) > 0) && ok;
    ok = (prefs.putUChar("reverse_g", g_settings.reverse_g) > 0) && ok;
    ok = (prefs.putUChar("reverse_b", g_settings.reverse_b) > 0) && ok;

    ok = (prefs.putUChar("run_r", g_settings.run_r) > 0) && ok;
    ok = (prefs.putUChar("run_g", g_settings.run_g) > 0) && ok;
    ok = (prefs.putUChar("run_b", g_settings.run_b) > 0) && ok;

    ok = (prefs.putUChar("brake_anim",   g_settings.brake_anim) > 0) && ok;
    ok = (prefs.putUChar("turn_anim",    g_settings.turn_anim) > 0) && ok;
    ok = (prefs.putUChar("reverse_anim", g_settings.reverse_anim) > 0) && ok;
    ok = (prefs.putUChar("run_anim",     g_settings.run_anim) > 0) && ok;
    ok = (prefs.putUChar("lens_preset",  g_settings.lens_preset) > 0) && ok;

    ok = (prefs.putUChar("startup_anim", g_settings.startup_anim) > 0) && ok;
    ok = (prefs.putUChar("rest_mode",    g_settings.rest_mode) > 0) && ok;

    // show_mode is not persisted — it resets to off on every boot.
    ok = (prefs.putUChar("show_anim",  g_settings.show_anim) > 0) && ok;
    ok = (prefs.putUChar("show_speed", g_settings.show_speed) > 0) && ok;
    ok = putText("show_text", g_settings.show_text) && ok;

    ok = (prefs.putUChar("wifi_mode", g_settings.wifi_mode) > 0) && ok;

    ok = putText("ap_ssid",  g_settings.ap_ssid) && ok;
    ok = putText("ap_pass",  g_settings.ap_pass) && ok;
    ok = putText("sta_ssid", g_settings.sta_ssid) && ok;
    ok = putText("sta_pass", g_settings.sta_pass) && ok;

    prefs.end();
    if (ok) savedSettings = g_settings;
    return ok;
}

void settings_revert() {
    g_settings = savedSettings;
    g_settings.show_mode = 0;
}
bool settings_pending() {
    Settings current = g_settings, saved = savedSettings;
    current.show_mode = saved.show_mode = 0; // runtime-only mode switch
    return memcmp(&current, &saved, sizeof(Settings)) != 0;
}
