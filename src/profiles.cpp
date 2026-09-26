#include "profiles.h"
#include "lighting_config.h"
#include <Preferences.h>
#include <cstdio>

static bool profileKey(int slot, char* key) {
    if (slot < 0 || slot >= PROFILE_COUNT) return false;
    snprintf(key, 8, "slot%d", slot);
    return true;
}
bool profile_read(int slot, char* name, Settings& settings) {
    char key[8], data[1536];
    if (!profileKey(slot, key)) return false;
    Preferences prefs;
    if (!prefs.begin("tailprofiles", true)) return false;
    const size_t length = prefs.getString(key, data, sizeof(data));
    prefs.end();
    if (!length) return false;
    JsonDocument doc;
    if (deserializeJson(doc, data) || doc["version"].as<int>() != 1) return false;
    const char* label = doc["name"].as<const char*>();
    if (!label || !label[0] || strlen(label) >= PROFILE_NAME_SIZE) return false;
    if (!lightingFromJson(doc["settings"].as<JsonObjectConst>(), settings)) return false;
    strcpy(name, label);
    return true;
}
bool profile_write(int slot, const char* name, const Settings& settings) {
    char key[8], data[1536];
    if (!profileKey(slot, key) || !name || !name[0] || strlen(name) >= PROFILE_NAME_SIZE) return false;
    JsonDocument doc;
    doc["version"] = 1;
    doc["name"] = name;
    lightingToJson(doc["settings"].to<JsonObject>(), settings);
    if (measureJson(doc) >= sizeof(data)) return false;
    const size_t length = serializeJson(doc, data, sizeof(data));
    Preferences prefs;
    if (!prefs.begin("tailprofiles", false)) return false;
    const bool ok = prefs.putString(key, data) == length;
    prefs.end();
    return ok;
}
bool profile_delete(int slot) {
    char key[8];
    if (!profileKey(slot, key)) return false;
    Preferences prefs;
    if (!prefs.begin("tailprofiles", false)) return false;
    const bool ok = !prefs.isKey(key) || prefs.remove(key);
    prefs.end();
    return ok;
}
