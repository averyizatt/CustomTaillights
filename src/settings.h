#pragma once

// ---------------------------------------------------------------------------
// settings.h
// Run-time configurable parameters persisted to NVS (Preferences).
// All fields that the web UI can modify live here.
//
// Usage:
//   Call settings_load() once in setup() before any code reads g_settings.
//   Call settings_save() after modifying g_settings to persist changes.
//   Call settings_reset() to restore factory defaults (also call settings_save()
//   afterward if you want the defaults written to NVS immediately).
// ---------------------------------------------------------------------------

#include <stdint.h>

struct Settings {
    // ── Brightness ────────────────────────────────────────────────────────────
    uint8_t  brightness;       // main brightness (10–255)
    uint8_t  brightness_dim;   // running-light / parked level (5–100)

    // ── Animation timing ──────────────────────────────────────────────────────
    uint16_t turn_blink_ms;    // full on+off blink cycle period (200–1500 ms)
    uint8_t  frame_ms;         // animation frame interval (10–100 ms)

    // ── Colors ────────────────────────────────────────────────────────────────
    uint8_t  brake_r,   brake_g,   brake_b;    // brake state color
    uint8_t  turn_r,    turn_g,    turn_b;     // turn signal color
    uint8_t  reverse_r, reverse_g, reverse_b;  // reverse state color
    uint8_t  run_r,     run_g,     run_b;      // running / parking light color
    // ── Animation style selectors ─────────────────────────────────────────────
    uint8_t  brake_anim;    // 0=solid 1=pulse 2=center-out 3=strobe 4=outer-in 5=heartbeat
    uint8_t  turn_anim;     // 0=sequential 1=simple-flash 2=group-chase 3=bounce 4=split-out 5=fast-chase
    uint8_t  reverse_anim;  // 0=solid 1=pulse 2=sparkle 3=scanner
    uint8_t  run_anim;      // 0=dim-solid 1=breathe 2=shimmer 3=comet

    // ── Hardware preset ───────────────────────────────────────────────────────
    uint8_t  lens_preset;   // 0=full-panel 1=GT 2=LX 3=cobra-bar

    // ── Startup behavior ──────────────────────────────────────────────────
    uint8_t  startup_anim;  // 1 = play sequential sweep at boot, 0 = skip

    // ── Rest mode ─────────────────────────────────────────────────────────
    uint8_t  rest_mode;     // 0 = off, 1 = force RUNNING when all inputs idle

    // ── Show Mode ─────────────────────────────────────────────────────────────
    uint8_t  show_mode;    // 0 = off, 1 = on (overrides car-signal animations)
    uint8_t  show_anim;    // 0-19 which standalone show animation to play
    uint8_t  show_speed;   // 50-200 (% of base speed, 100 = normal)
    char     show_text[64]; // Text for show_anim=19 scrolling text mode

    // ── WiFi ──────────────────────────────────────────────────────────────────
    uint8_t  wifi_mode;         // 0 = Access Point, 1 = Station
    char     ap_ssid[32];       // AP network name
    char     ap_pass[64];       // AP password (WPA2, min 8 chars)
    char     sta_ssid[32];      // station: home network name
    char     sta_pass[64];      // station: home network password
};

// Global settings instance — defined in settings.cpp.
extern Settings g_settings;

// Load settings from NVS.  Missing keys fall back to compiled-in defaults.
void settings_load();

// Persist the current g_settings to NVS.
void settings_save();

// Reset g_settings to compiled-in defaults (does NOT automatically persist).
void settings_reset();
