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
        // NVS namespace not yet created — first boot, keep defaults.
        return;
    }

    g_settings.brightness     = prefs.getUChar ("brightness",     kDefaults.brightness);
    g_settings.brightness_dim = prefs.getUChar ("brightness_dim", kDefaults.brightness_dim);
    g_settings.turn_blink_ms  = prefs.getUShort("turn_blink_ms",  kDefaults.turn_blink_ms);
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
    g_settings.frame_ms       = (uint8_t)constrain(g_settings.frame_ms, 10, 100);

    g_settings.brake_anim     = (uint8_t)constrain(g_settings.brake_anim, 0, 5);
    g_settings.turn_anim      = (uint8_t)constrain(g_settings.turn_anim, 0, 5);
    g_settings.reverse_anim   = (uint8_t)constrain(g_settings.reverse_anim, 0, 3);
    g_settings.run_anim       = (uint8_t)constrain(g_settings.run_anim, 0, 3);
    g_settings.lens_preset    = (uint8_t)constrain(g_settings.lens_preset, 0, 3);
    g_settings.startup_anim   = (uint8_t)constrain(g_settings.startup_anim, 0, 1);
    g_settings.rest_mode      = (uint8_t)constrain(g_settings.rest_mode, 0, 1);
    g_settings.show_mode      = 0;
    g_settings.show_anim      = (uint8_t)constrain(g_settings.show_anim, 0, 32);
    g_settings.show_speed     = (uint8_t)constrain(g_settings.show_speed, 50, 200);
    g_settings.wifi_mode      = (uint8_t)constrain(g_settings.wifi_mode, 0, 1);
}

// ---------------------------------------------------------------------------
void settings_save() {
    Preferences prefs;
    prefs.begin("tailsettings", /*readOnly=*/false);

    prefs.putUChar ("brightness",     g_settings.brightness);
    prefs.putUChar ("brightness_dim", g_settings.brightness_dim);
    prefs.putUShort("turn_blink_ms",  g_settings.turn_blink_ms);
    prefs.putUChar ("frame_ms",       g_settings.frame_ms);

    prefs.putUChar("brake_r", g_settings.brake_r);
    prefs.putUChar("brake_g", g_settings.brake_g);
    prefs.putUChar("brake_b", g_settings.brake_b);

    prefs.putUChar("turn_r",  g_settings.turn_r);
    prefs.putUChar("turn_g",  g_settings.turn_g);
    prefs.putUChar("turn_b",  g_settings.turn_b);

    prefs.putUChar("reverse_r", g_settings.reverse_r);
    prefs.putUChar("reverse_g", g_settings.reverse_g);
    prefs.putUChar("reverse_b", g_settings.reverse_b);

    prefs.putUChar("run_r", g_settings.run_r);
    prefs.putUChar("run_g", g_settings.run_g);
    prefs.putUChar("run_b", g_settings.run_b);

    prefs.putUChar("brake_anim",   g_settings.brake_anim);
    prefs.putUChar("turn_anim",    g_settings.turn_anim);
    prefs.putUChar("reverse_anim", g_settings.reverse_anim);
    prefs.putUChar("run_anim",     g_settings.run_anim);
    prefs.putUChar("lens_preset",  g_settings.lens_preset);

    prefs.putUChar("startup_anim", g_settings.startup_anim);
    prefs.putUChar("rest_mode",    g_settings.rest_mode);

    // show_mode is not persisted — it resets to off on every boot.
    prefs.putUChar("show_anim",  g_settings.show_anim);
    prefs.putUChar("show_speed", g_settings.show_speed);
    prefs.putString("show_text", g_settings.show_text);

    prefs.putUChar("wifi_mode", g_settings.wifi_mode);

    prefs.putString("ap_ssid",  g_settings.ap_ssid);
    prefs.putString("ap_pass",  g_settings.ap_pass);
    prefs.putString("sta_ssid", g_settings.sta_ssid);
    prefs.putString("sta_pass", g_settings.sta_pass);

    prefs.end();
}
