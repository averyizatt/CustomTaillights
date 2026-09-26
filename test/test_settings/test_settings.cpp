#include <cassert>
#include <cstdio>
#include <cstring>
#include "settings.h"
#include "profiles.h"
#include "lighting_config.h"
#include "turn_timing.h"
#include <Preferences.h>

int main() {
    settings_load();
    assert(!settings_pending());
    assert(g_settings.turn_custom == 0 && turnTiming(g_settings).period() == 600);
    assert(settings_save()); // Empty station credentials are valid.
    const auto original = g_settings;
    g_settings.turn_custom = 1;
    g_settings.turn_sweep_ms = 160; g_settings.turn_hold_ms = 240; g_settings.turn_off_ms = 500;
    assert(settings_pending());
    settings_revert();
    assert(!settings_pending() && g_settings.turn_custom == 0);
    g_settings.turn_hold_ms = 220;
    settings_load(); // Reboot loses unapplied-to-storage edits.
    assert(g_settings.turn_hold_ms == 0);
    g_settings.turn_custom = 1; g_settings.turn_hold_ms = 220;
    assert(settings_save());
    settings_load();
    assert(g_settings.turn_custom == 1 && g_settings.turn_hold_ms == 220);
    Preferences::failWrites = true;
    g_settings.brightness = 111;
    assert(!settings_save());
    settings_revert();
    assert(g_settings.brightness == original.brightness);
    Preferences::failWrites = false;

    // Profiles include all lighting settings, but do not capture runtime/hardware/WiFi.
    Settings profile = g_settings;
    profile.brake_r = 83; profile.run_speed = 160;
    strcpy(profile.ap_ssid, "DO NOT COPY"); profile.lens_preset = 3; profile.show_mode = 1;
    for (int slot = 0; slot < PROFILE_COUNT; ++slot) {
        profile.turn_hold_ms = slot * 100;
        assert(profile_write(slot, "Cruise", profile));
    }
    assert(!settings_pending());
    for (int slot = 0; slot < PROFILE_COUNT; ++slot) {
        Settings loaded = original;
        char name[PROFILE_NAME_SIZE];
        assert(profile_read(slot, name, loaded));
        assert(strcmp(name, "Cruise") == 0 && loaded.brake_r == 83 && loaded.run_speed == 160);
        assert(loaded.turn_hold_ms == slot * 100);
        assert(strcmp(loaded.ap_ssid, original.ap_ssid) == 0);
        assert(loaded.lens_preset == original.lens_preset && loaded.show_mode == 0);
    }
    assert(!profile_write(6, "Invalid", profile));
    assert(!profile_write(0, "", profile));
    assert(!profile_write(0, "1234567890123456789012345", profile));
    Preferences::failWrites = true;
    assert(!profile_write(0, "Failed", profile));
    assert(!profile_delete(0));
    Preferences::failWrites = false;
    assert(profile_delete(0));
    char name[PROFILE_NAME_SIZE]; Settings loaded = original;
    assert(!profile_read(0, name, loaded));
    assert(profile_delete(0));
    Preferences::store["tailprofiles/slot1"] = "{broken";
    assert(!profile_read(1, name, loaded));
    JsonDocument doc; lightingToJson(doc.to<JsonObject>(), profile);
    doc["turn_sweep_ms"] = 0;
    assert(!lightingFromJson(doc.as<JsonObjectConst>(), loaded));
    assert(loaded.brake_r == original.brake_r); // Invalid input is atomic.
    doc["turn_sweep_ms"] = 100; doc.remove("run_speed");
    assert(!lightingFromJson(doc.as<JsonObjectConst>(), loaded));

    // Missing keys on an older device preserve its original blink behavior.
    Preferences::store.erase("tailsettings/turn_custom");
    Preferences::store["tailsettings/turn_blink_ms"] = "601";
    settings_load();
    assert(!g_settings.turn_custom);
    assert(turnTiming(g_settings).sweep == 300 && turnTiming(g_settings).off == 301);
    g_settings.turn_custom=1; g_settings.turn_sweep_ms=0; g_settings.turn_off_ms=0; g_settings.turn_hold_ms=65535;
    auto timing=turnTiming(g_settings);
    assert(timing.sweep==50 && timing.off==50 && timing.hold==1500);
    puts("Settings persistence, revert, six profiles, isolation, failures and timing migration passed");
}
