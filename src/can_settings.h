#pragma once
// ---------------------------------------------------------------------------
// can_settings.h
// Lighting settings over CAN (can_protocol.h extension 3): the dashboard's
// replacement for the WiFi settings page on the PCB build. Keys, ranges and
// report layouts are defined in the shared contract; this module applies them
// to g_settings using the same limits as settings_load().
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "settings.h"

namespace can_settings {

// Current value of a taillight_setting key (0 for unknown keys).
uint16_t get(uint8_t key);

// Apply a value clamped to the key's range. Returns false for unknown keys.
// `applied` receives the stored value; `clamped` is true when it was limited.
bool set(uint8_t key, uint16_t value, uint16_t& applied, bool& clamped);

// taillight_color index. Returns false for unknown colors.
bool setColor(uint8_t which, uint8_t red, uint8_t green, uint8_t blue);
bool color(uint8_t which, uint8_t& red, uint8_t& green, uint8_t& blue);

// Show text in chunks: offset 0 starts a new text; a chunk shorter than six
// characters or containing NUL ends it. Returns false for invalid offsets.
bool setTextChunk(uint8_t offset, const uint8_t* chars, uint8_t length);

// taillight_action SAVE/REVERT/FACTORY_DEFAULTS/PROFILE_*; REPORT is handled by
// the CAN service. Returns a config_ack_status / TAILLIGHT_ACK_* code.
uint8_t action(uint8_t action, uint8_t argument);

// Bit per profile slot in use, cached so the 500 ms status needs no flash reads.
uint8_t profileSlots();
void refreshProfileSlots();

}  // namespace can_settings
