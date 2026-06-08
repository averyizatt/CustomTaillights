// ---------------------------------------------------------------------------
// wifi_server.cpp
// WiFi (AP / Station) + synchronous HTTP server.
// The settings page HTML/CSS/JS is embedded as a raw-string literal so no
// filesystem (SPIFFS / LittleFS) is required.
// ---------------------------------------------------------------------------

#include "wifi_server.h"
#include "settings.h"
#include "states.h"

#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

// Preview state — set by POST /api/preview, consumed in main.cpp loop().
// Two separate states (driver = left, passenger = right) allow per-side previews.
// Declared here and extern'd in main.cpp.
volatile LightState    g_preview_driver    = LightState::OFF;
volatile LightState    g_preview_passenger = LightState::OFF;
volatile unsigned long g_preview_until_ms  = 0;
volatile unsigned long g_rest_pulse_until_ms = 0;
volatile uint8_t       g_soft_driver_mask    = 0; // bit0 brake, bit1 running, bit2 turn, bit3 reverse
volatile uint8_t       g_soft_passenger_mask = 0; // bit0 brake, bit1 running, bit2 turn, bit3 reverse
volatile uint8_t       g_soft_inputs_enabled = 0;
volatile uint8_t       g_live_driver_inputs  = 0; // physical/debounced snapshot from input task
volatile uint8_t       g_live_passenger_inputs = 0;

static WebServer _server(80);
static DNSServer _dns;
static bool g_ap_mode_active = false;
static constexpr unsigned long REST_PULSE_DURATION_MS = 1800UL;
static constexpr unsigned long PREVIEW_LOCKOUT_AFTER_MS = 2000UL;
static uint8_t g_preview_lockout_enabled = 0;
static unsigned long g_preview_drive_active_since_ms = 0;
static char g_preview_last_action[40] = "none";
static unsigned long g_preview_last_action_ms = 0;

// ---------------------------------------------------------------------------
// Embedded web page (stored in flash — no SRAM copy needed via send_P)
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Foxbody Taillights</title>
<style>
/* ── Variables ──────────────────────────────────────────────────────────── */
:root {
  --bg:      #0d0d0f;
  --surface: #1c1c1e;
  --surf2:   #2c2c2e;
  --border:  #3a3a3c;
  --accent:  #e31c25;
  --green:   #34c759;
  --blue:    #0a84ff;
  --purple:  #af52de;
  --text:    #ffffff;
  --text2:   rgba(235,235,240,.8);
  --text3:   #8e8e93;
  --r:       12px;
}

/* ── Reset ──────────────────────────────────────────────────────────────── */
*, *::before, *::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
  -webkit-tap-highlight-color: transparent;
}

/* ── Body ───────────────────────────────────────────────────────────────── */
body {
  background: var(--bg);
  color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  font-size: 16px;
  min-height: 100dvh;
  padding-bottom: 80px;
}

/* ── Header ─────────────────────────────────────────────────────────────── */
.header {
  position: sticky;
  top: 0;
  z-index: 200;
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 56px;
  padding: 0 16px;
  background: rgba(28,28,30,.94);
  border-bottom: 1px solid var(--border);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
}
.header__left   { display: flex; align-items: center; gap: 10px; }
.header__icon   { width: 30px; height: 30px; background: var(--accent); border-radius: 7px; display: flex; align-items: center; justify-content: center; font-size: 15px; flex-shrink: 0; }
.header__name   { font-size: 17px; font-weight: 700; letter-spacing: -.3px; }
.header__ip     { font-size: 11px; color: var(--text3); margin-top: 1px; }
.header__sync   { font-size: 10px; color: var(--text3); margin-top: 1px; }
.header__badge  { display: flex; align-items: center; gap: 5px; font-size: 12px; font-weight: 600; color: var(--text3); background: var(--surf2); padding: 5px 10px; border-radius: 20px; border: 1px solid var(--border); }
.badge-dot      { width: 7px; height: 7px; border-radius: 50%; background: var(--green); animation: pulse 2s infinite; }
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }

/* ── Mode grid (replaces tab bar) ──────────────────────────────────────── */
.mode-grid {
  position: sticky;
  top: 56px;
  z-index: 190;
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 6px;
  padding: 10px 12px;
  background: rgba(28,28,30,.96);
  border-bottom: 1px solid var(--border);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
}
.mode-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 5px;
  padding: 10px 6px 9px;
  background: var(--surf2);
  border: 1.5px solid var(--border);
  border-radius: 10px;
  font-size: 11px;
  font-weight: 600;
  color: var(--text3);
  cursor: pointer;
  transition: background .15s, color .15s, border-color .15s;
  -webkit-appearance: none;
  letter-spacing: -.1px;
  line-height: 1.2;
}
.mode-btn:active { transform: scale(.95); }
.mode-btn--active { background: rgba(227,28,37,.12); border-color: var(--accent); color: var(--text); }
.mode-btn__bar {
  width: 26px;
  height: 4px;
  border-radius: 2px;
  opacity: .5;
  transition: opacity .15s;
}
.mode-btn--active .mode-btn__bar { opacity: 1; }

/* ── Pages ──────────────────────────────────────────────────────────────── */
.page        { padding: 16px; display: flex; flex-direction: column; gap: 16px; }
.page--hidden { display: none; }

/* ── Group label ────────────────────────────────────────────────────────── */
.group-label {
  font-size: 11px;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: .9px;
  color: var(--text3);
  padding: 0 4px;
  margin-bottom: -4px;
}

/* ── Card ───────────────────────────────────────────────────────────────── */
.card      { background: var(--surface); border-radius: var(--r); border: 1px solid var(--border); overflow: hidden; }
.card-hd   { padding: 14px 16px 0; display: flex; align-items: center; gap: 10px; margin-bottom: 2px; }
.card-icon { width: 32px; height: 32px; border-radius: 8px; display: flex; align-items: center; justify-content: center; font-size: 15px; flex-shrink: 0; }
.card-icon--red    { background: rgba(227,28,37,.18); }
.card-icon--amber  { background: rgba(255,140,0,.18); }
.card-icon--green  { background: rgba(52,199,89,.18); }
.card-icon--blue   { background: rgba(10,132,255,.18); }
.card-icon--purple { background: rgba(175,82,222,.18); }
.card-title { font-size: 15px; font-weight: 600; }
.card-desc  { font-size: 12px; color: var(--text3); margin-top: 1px; }
.card-body  { padding: 10px 16px 16px; }

/* ── Row ────────────────────────────────────────────────────────────────── */
.row               { display: flex; align-items: center; justify-content: space-between; padding: 11px 0; gap: 12px; }
.row:not(.row--last) { border-bottom: 1px solid var(--border); }
.row__label        { flex: 1; }
.row__label span   { display: block; font-size: 15px; }
.row__label small  { color: var(--text3); font-size: 12px; }
.row__value        { color: var(--text3); font-size: 15px; }

/* ── Divider ────────────────────────────────────────────────────────────── */
.divider { height: 1px; background: var(--border); margin: 4px 0; }

/* ── Slider ─────────────────────────────────────────────────────────────── */
.slider-row { display: flex; align-items: center; gap: 10px; width: 100%; padding: 6px 0; }
.slider-row input[type=range] {
  flex: 1;
  -webkit-appearance: none;
  appearance: none;
  height: 5px;
  border-radius: 3px;
  background: var(--surf2);
  outline: none;
  cursor: pointer;
}
.slider-row input[type=range]::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 24px; height: 24px;
  border-radius: 50%;
  background: #fff;
  cursor: pointer;
  box-shadow: 0 1px 8px rgba(0,0,0,.5);
}
.slider-row input[type=range]::-moz-range-thumb {
  width: 24px; height: 24px;
  border-radius: 50%;
  border: none;
  background: #fff;
  cursor: pointer;
  box-shadow: 0 1px 8px rgba(0,0,0,.5);
}
.slider-val {
  min-width: 58px;
  text-align: right;
  font-size: 15px;
  font-variant-numeric: tabular-nums;
  color: var(--text2);
  font-weight: 500;
}

/* ── Color item ─────────────────────────────────────────────────────────── */
.color-item                    { padding: 10px 0; }
.color-item:not(.color-item--last) { border-bottom: 1px solid var(--border); }
.color-item__hd                { display: flex; align-items: center; justify-content: space-between; margin-bottom: 4px; }
.color-item__name              { font-size: 15px; font-weight: 500; }
.color-item__hint              { font-size: 12px; color: var(--text3); margin-top: 2px; }
.color-item__controls          { display: flex; align-items: center; gap: 8px; }
input[type=color] {
  -webkit-appearance: none;
  width: 44px; height: 36px;
  border: none; border-radius: 8px;
  cursor: pointer; padding: 0; background: none;
}
input[type=color]::-webkit-color-swatch-wrapper { padding: 0; border-radius: 8px; overflow: hidden; }
input[type=color]::-webkit-color-swatch         { border: none; border-radius: 8px; }
.hex-input {
  font-family: 'SF Mono', Menlo, monospace;
  font-size: 13px;
  color: var(--text2);
  background: var(--surf2);
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: 6px 10px;
  width: 84px;
  outline: none;
  -webkit-appearance: none;
}
.hex-input:focus { border-color: var(--accent); }

/* ── Color presets ──────────────────────────────────────────────────────── */
.presets        { display: flex; gap: 6px; flex-wrap: wrap; margin-top: 12px; padding-top: 12px; border-top: 1px solid var(--border); }
.preset-btn {
  padding: 9px 14px;
  font-size: 13px; font-weight: 500;
  color: var(--text);
  background: var(--surf2);
  border: 1px solid var(--border);
  border-radius: 10px;
  cursor: pointer;
  display: flex; align-items: center; gap: 7px;
  transition: background .12s, transform .1s;
  -webkit-appearance: none;
}
.preset-btn:active { background: var(--border); transform: scale(.96); }
.preset-dot { width: 9px; height: 9px; border-radius: 50%; flex-shrink: 0; }

/* ── Select ─────────────────────────────────────────────────────────────── */
.select {
  background: var(--surf2);
  color: var(--text);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 9px 32px 9px 12px;
  font-size: 15px;
  outline: none;
  -webkit-appearance: none;
  appearance: none;
  cursor: pointer;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='7' viewBox='0 0 12 7'%3E%3Cpath d='M1 1l5 5 5-5' stroke='%238e8e93' stroke-width='1.5' fill='none' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E");
  background-repeat: no-repeat;
  background-position: right 12px center;
  width: 100%;
}

/* ── Text input ─────────────────────────────────────────────────────────── */
.txt-input {
  background: var(--surf2);
  color: var(--text);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 11px 12px;
  font-size: 15px;
  outline: none;
  width: 100%;
  -webkit-appearance: none;
}
.txt-input:focus { border-color: var(--accent); }
.anim-search { margin-bottom: 10px; }

/* ── Field ──────────────────────────────────────────────────────────────── */
.field              { display: flex; flex-direction: column; gap: 6px; padding: 10px 0; }
.field:not(.field--last) { border-bottom: 1px solid var(--border); }
.field__label       { font-size: 11px; font-weight: 700; text-transform: uppercase; letter-spacing: .6px; color: var(--text3); }
.field__hint        { font-size: 12px; color: var(--text3); line-height: 1.5; margin-top: 2px; }

/* ── Toggle ─────────────────────────────────────────────────────────────── */
.toggle-row                        { display: flex; align-items: center; justify-content: space-between; padding: 11px 0; }
.toggle-row:not(.toggle-row--last) { border-bottom: 1px solid var(--border); }
.toggle-label                      { flex: 1; }
.toggle-label span                 { display: block; font-size: 15px; }
.toggle-label small                { color: var(--text3); font-size: 12px; }
.toggle        { position: relative; width: 51px; height: 31px; flex-shrink: 0; }
.toggle input  { opacity: 0; width: 0; height: 0; }
.toggle__track {
  position: absolute; inset: 0;
  background: var(--surf2);
  border: 1px solid var(--border);
  border-radius: 31px;
  cursor: pointer;
  transition: background .2s, border-color .2s;
}
.toggle input:checked + .toggle__track { background: var(--accent); border-color: var(--accent); }
.toggle__thumb {
  position: absolute;
  top: 3px; left: 3px;
  width: 25px; height: 25px;
  border-radius: 50%;
  background: #fff;
  box-shadow: 0 1px 4px rgba(0,0,0,.3);
  transition: transform .2s;
  pointer-events: none;
}
.toggle input:checked ~ .toggle__thumb { transform: translateX(20px); }

/* ── WiFi blocks ────────────────────────────────────────────────────────── */
.wifi-block          { display: none; }
.wifi-block--active  { display: block; }

/* ── Preview grid ───────────────────────────────────────────────────────── */
.preview-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
.preview-toolbar { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-bottom: 10px; }
.preview-status {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  margin-bottom: 10px;
  padding: 10px;
  border: 1px solid var(--border);
  border-radius: 10px;
  background: rgba(10,132,255,.08);
}
.preview-status__text { font-size: 12px; color: var(--text2); }
.preview-warning {
  display: none;
  margin-bottom: 10px;
  padding: 9px 10px;
  border: 1px solid rgba(255,149,0,.5);
  border-radius: 9px;
  color: #ffd08a;
  font-size: 12px;
  background: rgba(255,149,0,.12);
}
.preview-warning--show { display: block; }
.preview-progress {
  height: 6px;
  border-radius: 4px;
  background: var(--surf2);
  border: 1px solid var(--border);
  overflow: hidden;
  margin-bottom: 10px;
}
.preview-progress__bar {
  width: 0;
  height: 100%;
  background: linear-gradient(90deg, #34c759 0%, #0a84ff 100%);
  transition: width .12s linear;
}
.preview-card {
  padding: 13px 8px;
  background: var(--surf2);
  border: 1px solid var(--border);
  border-radius: 10px;
  color: var(--text);
  font-size: 14px; font-weight: 500;
  cursor: pointer;
  display: flex; align-items: center; justify-content: center; gap: 7px;
  transition: background .12s, transform .1s;
  -webkit-appearance: none;
  width: 100%;
}
.preview-card:active { background: var(--border); transform: scale(.96); }
.preview-card--active {
  border-color: var(--accent);
  background: rgba(227,28,37,.14);
}
.preview-card[disabled] { opacity: .55; cursor: not-allowed; }
.preview-dot { width: 9px; height: 9px; border-radius: 50%; flex-shrink: 0; }
.preview-side-badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 5px 9px;
  border-radius: 999px;
  font-size: 11px;
  font-weight: 700;
  border: 1px solid var(--border);
  color: var(--text2);
  background: var(--surf2);
}
.quick-chip-row { display: flex; flex-wrap: wrap; gap: 6px; margin-bottom: 10px; }
.quick-chip {
  font-size: 12px;
  padding: 7px 10px;
  border-radius: 999px;
  border: 1px solid var(--border);
  background: var(--surf2);
  color: var(--text2);
}

/* ── Animation tiles ───────────────────────────────────────────────────── */
.anim-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 8px;
}
.anim-tile {
  padding: 12px 10px;
  background: var(--surf2);
  border: 1.5px solid var(--border);
  border-radius: 10px;
  color: var(--text);
  cursor: pointer;
  display: flex;
  flex-direction: column;
  gap: 3px;
  transition: background .12s, border-color .12s, transform .1s;
  -webkit-appearance: none;
  text-align: left;
  width: 100%;
}
.anim-tile:active  { transform: scale(.96); }
.anim-tile--active { background: rgba(227,28,37,.12); border-color: var(--accent); }
.anim-tile--fade { opacity: .7; transition: opacity .5s ease; }
.anim-tile--hidden { display: none; }
.anim-tile__name   { font-size: 13px; font-weight: 600; line-height: 1.3; }
.anim-tile__desc   { font-size: 11px; color: var(--text3); line-height: 1.4; }

/* ── Action bar ─────────────────────────────────────────────────────────── */
.action-bar {
  position: fixed;
  bottom: 0; left: 0; right: 0;
  display: flex; gap: 10px;
  padding: 12px 16px;
  padding-bottom: calc(12px + env(safe-area-inset-bottom));
  background: var(--surface);
  border-top: 1px solid var(--border);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  z-index: 300;
}
.btn {
  flex: 1;
  padding: 14px 8px;
  border: none; border-radius: 10px;
  font-size: 15px; font-weight: 600;
  cursor: pointer;
  transition: opacity .15s, transform .1s;
  -webkit-appearance: none;
  letter-spacing: -.2px;
}
.btn:active         { opacity: .7; transform: scale(.97); }
.btn--primary       { background: var(--accent); color: #fff; }
.btn--secondary     { background: var(--surf2); color: var(--text); border: 1px solid var(--border); }
.btn--ghost         { background: transparent; color: var(--text3); border: 1px solid var(--border); }

/* ── Toast ──────────────────────────────────────────────────────────────── */
.toast {
  position: fixed;
  top: 120px; left: 50%;
  transform: translateX(-50%) translateY(-8px);
  background: var(--surf2);
  color: var(--text);
  padding: 10px 20px;
  border-radius: 20px;
  font-size: 14px; font-weight: 500;
  box-shadow: 0 2px 20px rgba(0,0,0,.6);
  opacity: 0;
  pointer-events: none;
  transition: opacity .22s, transform .22s;
  white-space: nowrap;
  border: 1px solid var(--border);
  z-index: 400;
}
.toast--show { opacity: 1; transform: translateX(-50%) translateY(0); }
.toast--ok   { border-color: var(--green); color: var(--green); }
.toast--err  { border-color: var(--accent); color: var(--accent); }

/* ── Responsive ─────────────────────────────────────────────────────────── */
@media (min-width: 520px) {
  .page { max-width: 520px; margin: 0 auto; }
}
</style>
</head>
<body>

<!-- ── Header ─────────────────────────────────────────────────────────────── -->
<header class="header">
  <div class="header__left">
    <div class="header__icon" style="font-size:12px;font-weight:700;">FT</div>
    <div>
      <div class="header__name">Foxbody Taillights</div>
      <div class="header__ip" id="hd-ip">Connecting&hellip;</div>
      <div class="header__sync" id="hd-sync">Waiting for sync&hellip;</div>
    </div>
  </div>
  <div class="header__badge">
    <div class="badge-dot"></div>
    <span id="hd-mode">AP</span>
  </div>
</header>

<!-- ── Mode grid ─────────────────────────────────────────────────────────── -->
<nav class="mode-grid" role="tablist">
  <button class="mode-btn mode-btn--active" data-tab="display" role="tab">
    <div class="mode-btn__bar" style="background:#e31c25"></div>Display
  </button>
  <button class="mode-btn" data-tab="colors" role="tab">
    <div class="mode-btn__bar" style="background:#ff9500"></div>Colors
  </button>
  <button class="mode-btn" data-tab="animations" role="tab">
    <div class="mode-btn__bar" style="background:#0a84ff"></div>Animations
  </button>
  <button class="mode-btn" data-tab="show" role="tab">
    <div class="mode-btn__bar" style="background:#af52de"></div>Show
  </button>
  <button class="mode-btn" data-tab="network" role="tab">
    <div class="mode-btn__bar" style="background:#34c759"></div>Network
  </button>
  <button class="mode-btn" data-tab="preview" role="tab">
    <div class="mode-btn__bar" style="background:#5ac8fa"></div>Preview
  </button>
</nav>

<!-- ── Display tab ────────────────────────────────────────────────────────── -->
<div id="tab-display" class="page">

  <p class="group-label">Brightness</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--red"></div>
      <div>
        <div class="card-title">LED Brightness</div>
        <div class="card-desc">Global intensity levels</div>
      </div>
    </div>
    <div class="card-body">
      <div class="row">
        <div class="row__label">
          <span>Main Brightness</span>
          <small>Brake, turn &amp; reverse states</small>
        </div>
      </div>
      <div class="slider-row">
        <input type="range" id="brightness" min="10" max="255" step="1" value="128">
        <span class="slider-val" id="brightness-v">128</span>
      </div>
      <div class="divider"></div>
      <div class="row row--last">
        <div class="row__label">
          <span>Running Light Level</span>
          <small>Dim glow at rest &amp; parked</small>
        </div>
      </div>
      <div class="slider-row">
        <input type="range" id="brightness_dim" min="5" max="100" step="1" value="40">
        <span class="slider-val" id="brightness_dim-v">40%</span>
      </div>
    </div>
  </div>

  <p class="group-label">Hardware</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--purple"></div>
      <div>
        <div class="card-title">Lens Preset</div>
        <div class="card-desc">LED zone layout for your taillight housing</div>
      </div>
    </div>
    <div class="card-body">
      <div class="field field--last">
        <label class="field__label" for="lens_preset">Taillight Style</label>
        <select class="select" id="lens_preset">
          <option value="0">Full Panel &mdash; all LEDs active (custom / generic)</option>
          <option value="1">GT Cheese Grater &mdash; 3-section chrome trim (87&ndash;93 GT)</option>
          <option value="2">LX / Base &mdash; 2-section clean lens (87&ndash;93 LX)</option>
          <option value="3">Cobra Bar &mdash; horizontal center-bar style (custom / Cobra)</option>
        </select>
        <div class="field__hint" id="lens-desc"></div>
      </div>
    </div>
  </div>

  <p class="group-label">Options</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--amber"></div>
      <div>
        <div class="card-title">Boot Options</div>
        <div class="card-desc">Startup behavior</div>
      </div>
    </div>
    <div class="card-body">
      <div class="toggle-row">
        <div class="toggle-label">
          <span>Startup Animation</span>
          <small>Sequential red sweep on every power-on</small>
        </div>
        <label class="toggle">
          <input type="checkbox" id="startup_anim" checked>
          <span class="toggle__track"></span>
          <span class="toggle__thumb"></span>
        </label>
      </div>
      <div class="toggle-row toggle-row--last">
        <div class="toggle-label">
          <span>Rest Mode</span>
          <small>Keeps running-light glow active whenever all inputs are idle</small>
        </div>
        <label class="toggle">
          <input type="checkbox" id="rest_mode" onchange="postRestSettings(true)">
          <span class="toggle__track"></span>
          <span class="toggle__thumb"></span>
        </label>
      </div>
    </div>
  </div>

</div><!-- /tab-display -->

<!-- ── Colors tab ─────────────────────────────────────────────────────────── -->
<div id="tab-colors" class="page page--hidden">

  <p class="group-label">Light Colors</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--amber"></div>
      <div>
        <div class="card-title">Per-State Colors</div>
        <div class="card-desc">Color override for each light function</div>
      </div>
    </div>
    <div class="card-body">

      <div class="color-item">
        <div class="color-item__hd">
          <div>
            <div class="color-item__name">Brake</div>
            <div class="color-item__hint">Solid brake-light color</div>
          </div>
          <div class="color-item__controls">
            <input type="color" id="brake_color" value="#ff0000">
            <input type="text" class="hex-input" id="brake_hex" value="#ff0000" maxlength="7" spellcheck="false">
          </div>
        </div>
      </div>

      <div class="color-item">
        <div class="color-item__hd">
          <div>
            <div class="color-item__name">Turn Signal</div>
            <div class="color-item__hint">Sequential sweep &amp; hazard flash</div>
          </div>
          <div class="color-item__controls">
            <input type="color" id="turn_color" value="#ff6400">
            <input type="text" class="hex-input" id="turn_hex" value="#ff6400" maxlength="7" spellcheck="false">
          </div>
        </div>
      </div>

      <div class="color-item">
        <div class="color-item__hd">
          <div>
            <div class="color-item__name">Reverse</div>
            <div class="color-item__hint">Backup-light color</div>
          </div>
          <div class="color-item__controls">
            <input type="color" id="reverse_color" value="#ffffff">
            <input type="text" class="hex-input" id="reverse_hex" value="#ffffff" maxlength="7" spellcheck="false">
          </div>
        </div>
      </div>

      <div class="color-item color-item--last">
        <div class="color-item__hd">
          <div>
            <div class="color-item__name">Running Light</div>
            <div class="color-item__hint">Dim glow color when parked</div>
          </div>
          <div class="color-item__controls">
            <input type="color" id="run_color" value="#1e0000">
            <input type="text" class="hex-input" id="run_hex" value="#1e0000" maxlength="7" spellcheck="false">
          </div>
        </div>
      </div>

      <div class="presets">
        <button class="preset-btn" onclick="applyPreset('stock')">
          <span class="preset-dot" style="background:#f00"></span>Stock
        </button>
        <button class="preset-btn" onclick="applyPreset('cool')">
          <span class="preset-dot" style="background:#0055ff"></span>Cool
        </button>
        <button class="preset-btn" onclick="applyPreset('purple')">
          <span class="preset-dot" style="background:#aa00ff"></span>Purple
        </button>
        <button class="preset-btn" onclick="applyPreset('sport')">
          <span class="preset-dot" style="background:#ffd700"></span>Sport
        </button>
        <button class="preset-btn" onclick="applyPreset('murdered')">
          <span class="preset-dot" style="background:#550000"></span>Murdered
        </button>
      </div>

    </div>
  </div>

</div><!-- /tab-colors -->

<!-- ── Animations tab ─────────────────────────────────────────────────────── -->
<div id="tab-animations" class="page page--hidden">

  <p class="group-label">Styles</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--amber"></div>
      <div>
        <div class="card-title">Animation Styles</div>
        <div class="card-desc">How each state is displayed on the LEDs</div>
      </div>
    </div>
    <div class="card-body">
      <div class="field">
        <label class="field__label" for="brake_anim">Brake</label>
        <select class="select" id="brake_anim" onchange="postAnimSettings()">
          <option value="0">Solid Fill &mdash; instant-on, stays lit</option>
          <option value="1">Pulse / Breathe &mdash; gently fades in and out</option>
          <option value="2">Center-Out Fill &mdash; expands from center then holds</option>
          <option value="3">Strobe Flash &mdash; rapid 8 Hz attention strobe</option>
          <option value="4">Outer-In Fill &mdash; sweeps from edges to center then holds</option>
          <option value="5">Heartbeat &mdash; lub-dub double-pulse pattern</option>
        </select>
      </div>
      <div class="field">
        <label class="field__label" for="turn_anim">Turn Signal</label>
        <select class="select" id="turn_anim" onchange="postAnimSettings()">
          <option value="0">Sequential Sweep &mdash; classic column-by-column sweep</option>
          <option value="1">Simple Flash &mdash; whole panel blinks on / off</option>
          <option value="2">Group Chase &mdash; 4-column groups sweep in sequence</option>
          <option value="3">Bounce Sweep &mdash; Knight Rider beam bounces across</option>
          <option value="4">Split Out &mdash; dual sweep races from center to edges</option>
          <option value="5">Fast Chase &mdash; rapid triple-flash per blink</option>
        </select>
      </div>
      <div class="field">
        <label class="field__label" for="reverse_anim">Reverse</label>
        <select class="select" id="reverse_anim" onchange="postAnimSettings()">
          <option value="0">Solid &mdash; instant-on, stays lit</option>
          <option value="1">Pulse / Breathe &mdash; gently fades in and out</option>
          <option value="2">Sparkle &mdash; scattered pixel bursts</option>
          <option value="3">Scanner &mdash; slow-moving bright column</option>
        </select>
      </div>
      <div class="field field--last">
        <label class="field__label" for="run_anim">Running Light</label>
        <select class="select" id="run_anim" onchange="postAnimSettings();triggerRestPulse();">
          <option value="0">Dim Solid &mdash; constant dim glow at rest</option>
          <option value="1">Breathe &mdash; slow pulse dim glow</option>
          <option value="2">Shimmer &mdash; subtle per-pixel brightness variation</option>
          <option value="3">Slow Comet &mdash; very dim wandering comet</option>
        </select>
      </div>
    </div>
  </div>

  <p class="group-label">Timing</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--amber"></div>
      <div>
        <div class="card-title">Animation Timing</div>
        <div class="card-desc">Blink speed &amp; frame rate</div>
      </div>
    </div>
    <div class="card-body">
      <div class="row">
        <div class="row__label">
          <span>Turn Blink Period</span>
          <small>Full on+off cycle time</small>
        </div>
      </div>
      <div class="slider-row">
        <input type="range" id="turn_blink_ms" min="200" max="1500" step="50" value="600">
        <span class="slider-val" id="turn_blink_ms-v">600 ms</span>
      </div>
      <div class="divider"></div>
      <div class="row row--last">
        <div class="row__label">
          <span>Frame Interval</span>
          <small>Animation update rate (~60 fps default)</small>
        </div>
      </div>
      <div class="slider-row">
        <input type="range" id="frame_ms" min="10" max="100" step="5" value="20">
        <span class="slider-val" id="frame_ms-v">20 ms</span>
      </div>
    </div>
  </div>

</div><!-- /tab-animations -->

<!-- ── Network tab ────────────────────────────────────────────────────────── -->
<div id="tab-network" class="page page--hidden">

  <p class="group-label">WiFi</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--blue"></div>
      <div>
        <div class="card-title">WiFi Configuration</div>
        <div class="card-desc">Access point &amp; station settings</div>
      </div>
    </div>
    <div class="card-body">
      <div class="field">
        <label class="field__label" for="wifi_mode">Mode</label>
        <select class="select" id="wifi_mode" onchange="updateWifiBlocks()">
          <option value="0">Access Point (AP) &mdash; device creates its own hotspot</option>
          <option value="1">Station (STA) &mdash; connect to existing WiFi</option>
        </select>
      </div>
      <div id="ap-block" class="wifi-block wifi-block--active">
        <div class="field">
          <label class="field__label" for="ap_ssid">AP Network Name (SSID)</label>
          <input type="text" class="txt-input" id="ap_ssid" placeholder="Foxbody-Taillights" autocomplete="off">
        </div>
        <div class="field field--last">
          <label class="field__label" for="ap_pass">AP Password (min 8 chars)</label>
          <input type="password" class="txt-input" id="ap_pass" placeholder="Enter password" autocomplete="new-password">
        </div>
      </div>
      <div id="sta-block" class="wifi-block">
        <div class="field">
          <label class="field__label" for="sta_ssid">Home WiFi SSID</label>
          <input type="text" class="txt-input" id="sta_ssid" placeholder="Your network name" autocomplete="off">
        </div>
        <div class="field field--last">
          <label class="field__label" for="sta_pass">Home WiFi Password</label>
          <input type="password" class="txt-input" id="sta_pass" placeholder="Your network password" autocomplete="current-password">
        </div>
      </div>
    </div>
  </div>

  <p class="group-label">Device</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--purple"></div>
      <div>
        <div class="card-title">Device Info</div>
        <div class="card-desc">ESP32-S3 &middot; FastLED &middot; 2&times;380 px WS2812B</div>
      </div>
    </div>
    <div class="card-body">
      <div class="row"><span>IP Address</span><span class="row__value" id="info-ip">&ndash;</span></div>
      <div class="row"><span>WiFi Mode</span><span class="row__value" id="info-mode">&ndash;</span></div>
      <div class="row row--last"><span>Uptime</span><span class="row__value" id="info-uptime">&ndash;</span></div>
    </div>
  </div>

</div><!-- /tab-network -->

<!-- ── Show tab ───────────────────────────────────────────────────────────── -->
<div id="tab-show" class="page page--hidden">

  <p class="group-label">Show Mode</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--purple"></div>
      <div>
        <div class="card-title">Show Mode</div>
        <div class="card-desc">Standalone display animations &mdash; brake &amp; reverse always override</div>
      </div>
    </div>
    <div class="card-body">
      <div class="toggle-row toggle-row--last">
        <div class="toggle-label">
          <span>Enable Show Mode</span>
          <small>Overrides idle state with the selected show animation</small>
        </div>
        <label class="toggle">
          <input type="checkbox" id="show_mode" onchange="postShowSettings()">
          <span class="toggle__track"></span>
          <span class="toggle__thumb"></span>
        </label>
      </div>
    </div>
  </div>

  <p class="group-label">Animation</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--amber"></div>
      <div>
        <div class="card-title">Show Animation</div>
        <div class="card-desc">Pick the standalone light show effect</div>
      </div>
    </div>
    <div class="card-body">
      <div class="field field--last">
        <input type="text" class="txt-input anim-search" id="show_anim_search" placeholder="Filter show animations" autocomplete="off" spellcheck="false">
        <div class="anim-grid" id="anim-grid">
          <button class="anim-tile" onclick="selectAnim(0)"><span class="anim-tile__name">Rainbow Scroll</span><span class="anim-tile__desc">Hue spectrum scrolls across</span></button>
          <button class="anim-tile" onclick="selectAnim(1)"><span class="anim-tile__name">Comet Chase</span><span class="anim-tile__desc">Bright tail races across</span></button>
          <button class="anim-tile" onclick="selectAnim(2)"><span class="anim-tile__name">Theater Chase</span><span class="anim-tile__desc">Marching dots</span></button>
          <button class="anim-tile" onclick="selectAnim(3)"><span class="anim-tile__name">Fire</span><span class="anim-tile__desc">Flickering heat simulation</span></button>
          <button class="anim-tile" onclick="selectAnim(4)"><span class="anim-tile__name">Meteor Shower</span><span class="anim-tile__desc">Two crossing streaks</span></button>
          <button class="anim-tile" onclick="selectAnim(5)"><span class="anim-tile__name">Police Strobe</span><span class="anim-tile__desc">Red / blue alternating flash</span></button>
          <button class="anim-tile" onclick="selectAnim(6)"><span class="anim-tile__name">Night Rider</span><span class="anim-tile__desc">KITT scanner bounce</span></button>
          <button class="anim-tile" onclick="selectAnim(7)"><span class="anim-tile__name">Color Cycle</span><span class="anim-tile__desc">Full-panel hue rotation</span></button>
          <button class="anim-tile" onclick="selectAnim(8)"><span class="anim-tile__name">Sparkle</span><span class="anim-tile__desc">Random hue pixel bursts</span></button>
          <button class="anim-tile" onclick="selectAnim(9)"><span class="anim-tile__name">Plasma</span><span class="anim-tile__desc">Sine-wave color field</span></button>
          <button class="anim-tile" onclick="selectAnim(10)"><span class="anim-tile__name">Matrix Rain</span><span class="anim-tile__desc">Green digital-rain columns</span></button>
          <button class="anim-tile" onclick="selectAnim(11)"><span class="anim-tile__name">Juggle</span><span class="anim-tile__desc">Six balls with glow halos</span></button>
          <button class="anim-tile" onclick="selectAnim(12)"><span class="anim-tile__name">BPM Bars</span><span class="anim-tile__desc">Spectrum-analyzer columns</span></button>
          <button class="anim-tile" onclick="selectAnim(13)"><span class="anim-tile__name">Confetti</span><span class="anim-tile__desc">Random-colored sparks</span></button>
          <button class="anim-tile" onclick="selectAnim(14)"><span class="anim-tile__name">Ocean Waves</span><span class="anim-tile__desc">Pacifica blue / teal waves</span></button>
          <button class="anim-tile" onclick="selectAnim(15)"><span class="anim-tile__name">Lightning</span><span class="anim-tile__desc">Dramatic white flashes</span></button>
          <button class="anim-tile" onclick="selectAnim(16)"><span class="anim-tile__name">Heartbeat</span><span class="anim-tile__desc">Lub-dub pulse in red</span></button>
          <button class="anim-tile" onclick="selectAnim(17)"><span class="anim-tile__name">Ripple</span><span class="anim-tile__desc">Rainbow rings from center</span></button>
          <button class="anim-tile" onclick="selectAnim(18)"><span class="anim-tile__name">Sunrise</span><span class="anim-tile__desc">Red to orange to yellow</span></button>
          <button class="anim-tile" onclick="selectAnim(19)"><span class="anim-tile__name">Text Scroll</span><span class="anim-tile__desc">Custom message scrolls</span></button>
          <button class="anim-tile" onclick="selectAnim(20)"><span class="anim-tile__name">Colorwaves</span><span class="anim-tile__desc">WLED silky colour bands</span></button>
          <button class="anim-tile" onclick="selectAnim(21)"><span class="anim-tile__name">Twinkle Fox</span><span class="anim-tile__desc">WLED independent pixel twinkle</span></button>
          <button class="anim-tile" onclick="selectAnim(22)"><span class="anim-tile__name">Bouncing Balls</span><span class="anim-tile__desc">WLED gravity-physics balls</span></button>
          <button class="anim-tile" onclick="selectAnim(23)"><span class="anim-tile__name">Fireworks</span><span class="anim-tile__desc">WLED flare and burst sparks</span></button>
          <button class="anim-tile" onclick="selectAnim(24)"><span class="anim-tile__name">Drip</span><span class="anim-tile__desc">WLED drops fall and bounce</span></button>
          <button class="anim-tile" onclick="selectAnim(25)"><span class="anim-tile__name">Cylon Dual</span><span class="anim-tile__desc">Two scanners converge and flash</span></button>
          <button class="anim-tile" onclick="selectAnim(26)"><span class="anim-tile__name">V8 Engine</span><span class="anim-tile__desc">Firing order 1-8-4-3-6-5-7-2</span></button>
          <button class="anim-tile" onclick="selectAnim(27)"><span class="anim-tile__name">Drag Launch</span><span class="anim-tile__desc">Tree countdown, green GO, comets</span></button>
          <button class="anim-tile" onclick="selectAnim(28)"><span class="anim-tile__name">Neon Glow</span><span class="anim-tile__desc">Tuner car colour wash</span></button>
          <button class="anim-tile" onclick="selectAnim(29)"><span class="anim-tile__name">Speed Streaks</span><span class="anim-tile__desc">Motion-blur racing lines</span></button>
          <button class="anim-tile" onclick="selectAnim(30)"><span class="anim-tile__name">Radar Sweep</span><span class="anim-tile__desc">Sweeping column scanner</span></button>
          <button class="anim-tile" onclick="selectAnim(31)"><span class="anim-tile__name">Aurora</span><span class="anim-tile__desc">Flowing northern lights</span></button>
          <button class="anim-tile" onclick="selectAnim(32)"><span class="anim-tile__name">Glitch</span><span class="anim-tile__desc">Digital colour disruption</span></button>
        </div>
      </div>
      <div class="field field--last" id="show-text-row" style="display:none">
        <label class="field__label" for="show_text">Message</label>
        <input type="text" class="text-input" id="show_text" maxlength="63"
               placeholder="FOXBODY MUSTANG" autocomplete="off" spellcheck="false">
        <small style="color:var(--text-dim);margin-top:4px;display:block">
          Up to 63 characters &bull; A&ndash;Z, 0&ndash;9, spaces, basic punctuation
        </small>
      </div>
    </div>
  </div>

  <p class="group-label">Speed</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--blue"></div>
      <div>
        <div class="card-title">Animation Speed</div>
        <div class="card-desc">Also scales brake pulse &amp; turn bounce timing</div>
      </div>
    </div>
    <div class="card-body">
      <div class="row row--last">
        <div class="row__label">
          <span>Speed</span>
          <small>50% = half speed &bull; 100% = normal &bull; 200% = double</small>
        </div>
      </div>
      <div class="slider-row">
        <input type="range" id="show_speed" min="50" max="200" step="10" value="100" onchange="postShowSettings()">
        <span class="slider-val" id="show_speed-v">100%</span>
      </div>
    </div>
  </div>

</div><!-- /tab-show -->

<!-- ── Preview tab ────────────────────────────────────────────────────────── -->
<div id="tab-preview" class="page page--hidden">

  <p class="group-label">Live Preview</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--green"></div>
      <div>
        <div class="card-title">Test Animations</div>
        <div class="card-desc">Live preview using current color settings</div>
      </div>
    </div>
    <div class="card-body">
      <div id="preview-warning" class="preview-warning"></div>
      <div class="preview-status">
        <div class="preview-status__text" id="preview-status-text">Idle</div>
        <span class="preview-side-badge" id="preview-side">None</span>
      </div>
      <div class="preview-progress"><div class="preview-progress__bar" id="preview-progress"></div></div>
      <div class="preview-toolbar">
        <select class="select" id="preview_duration_ms">
          <option value="1000">1 second</option>
          <option value="3000" selected>3 seconds</option>
          <option value="5000">5 seconds</option>
        </select>
        <button class="btn btn--secondary" id="preview-stop-btn" onclick="stopPreview()" style="padding:10px 8px">Stop Preview</button>
      </div>
      <div class="preview-grid">
        <button id="preview-brake" class="preview-card preview-trigger" onclick="preview('brake', this)">
          <span class="preview-dot" style="background:#f00"></span>Brake
        </button>
        <button id="preview-left_turn" class="preview-card preview-trigger" onclick="preview('left_turn', this)">
          <span class="preview-dot" style="background:#f80"></span>Left Turn
        </button>
        <button id="preview-right_turn" class="preview-card preview-trigger" onclick="preview('right_turn', this)">
          <span class="preview-dot" style="background:#f80"></span>Right Turn
        </button>
        <button id="preview-hazard" class="preview-card preview-trigger" onclick="preview('hazard', this)">
          <span class="preview-dot" style="background:#f80"></span>Hazard
        </button>
        <button id="preview-reverse" class="preview-card preview-trigger" onclick="preview('reverse', this)">
          <span class="preview-dot" style="background:#ddd"></span>Reverse
        </button>
        <button id="preview-running" class="preview-card preview-trigger" onclick="preview('running', this)">
          <span class="preview-dot" style="background:#a00"></span>Running
        </button>
        <button id="preview-off" class="preview-card preview-trigger" onclick="preview('off', this)">
          <span class="preview-dot" style="background:#3a3a3c"></span>Off
        </button>
        <button class="preview-card preview-trigger" onclick="runCommonSequence()">
          <span class="preview-dot" style="background:#5ac8fa"></span>Quick Sequence
        </button>
      </div>
      <div class="field field--last">
        <label class="field__label">Per-Side Manual Tests</label>
        <div class="preview-grid">
          <button class="preview-card preview-trigger" onclick="preview('driver_brake', this)">Driver Brake</button>
          <button class="preview-card preview-trigger" onclick="preview('passenger_brake', this)">Passenger Brake</button>
          <button class="preview-card preview-trigger" onclick="preview('driver_running', this)">Driver Running</button>
          <button class="preview-card preview-trigger" onclick="preview('passenger_running', this)">Passenger Running</button>
          <button class="preview-card preview-trigger" onclick="preview('driver_reverse', this)">Driver Reverse</button>
          <button class="preview-card preview-trigger" onclick="preview('passenger_reverse', this)">Passenger Reverse</button>
        </div>
      </div>
      <div class="toggle-row toggle-row--last">
        <div class="toggle-label">
          <span>Preview Lockout</span>
          <small>Block preview while live driving inputs stay active for over 2s</small>
        </div>
        <label class="toggle">
          <input type="checkbox" id="preview_lockout" onchange="postPreviewLockout()">
          <span class="toggle__track"></span>
          <span class="toggle__thumb"></span>
        </label>
      </div>
    </div>
  </div>

  <p class="group-label">Software Inputs</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--blue"></div>
      <div>
        <div class="card-title">Bench Input Toggles</div>
        <div class="card-desc">Force signal inputs in software for debug testing</div>
      </div>
    </div>
    <div class="card-body">
      <div class="toggle-row">
        <div class="toggle-label">
          <span>Enable Software Inputs</span>
          <small>When ON, selected toggles are OR'ed with physical input lines</small>
        </div>
        <label class="toggle">
          <input type="checkbox" id="soft_enable" onchange="postSoftInputs()">
          <span class="toggle__track"></span>
          <span class="toggle__thumb"></span>
        </label>
      </div>
      <div class="field">
        <label class="field__label">Driver Side</label>
        <div class="preview-grid">
          <button type="button" id="soft-driver_brake" class="preview-card" onclick="toggleSoft('driver_brake')">Brake</button>
          <button type="button" id="soft-driver_running" class="preview-card" onclick="toggleSoft('driver_running')">Running</button>
          <button type="button" id="soft-driver_turn" class="preview-card" onclick="toggleSoft('driver_turn')">Turn</button>
          <button type="button" id="soft-driver_reverse" class="preview-card" onclick="toggleSoft('driver_reverse')">Reverse</button>
        </div>
      </div>
      <div class="field field--last">
        <label class="field__label">Passenger Side</label>
        <div class="preview-grid">
          <button type="button" id="soft-passenger_brake" class="preview-card" onclick="toggleSoft('passenger_brake')">Brake</button>
          <button type="button" id="soft-passenger_running" class="preview-card" onclick="toggleSoft('passenger_running')">Running</button>
          <button type="button" id="soft-passenger_turn" class="preview-card" onclick="toggleSoft('passenger_turn')">Turn</button>
          <button type="button" id="soft-passenger_reverse" class="preview-card" onclick="toggleSoft('passenger_reverse')">Reverse</button>
        </div>
      </div>
    </div>
  </div>

  <p class="group-label">Show Animations</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--purple"></div>
      <div>
        <div class="card-title">Preview Show Effects</div>
        <div class="card-desc">Tap a tile to play it on the LEDs for 5 seconds</div>
      </div>
    </div>
    <div class="card-body">
      <div class="quick-chip-row" id="preview-favorites"></div>
      <div class="quick-chip-row" id="preview-recents"></div>
      <input type="text" class="txt-input anim-search" id="preview_anim_search" placeholder="Filter preview animations" autocomplete="off" spellcheck="false">
      <select class="select" id="preview_anim_category" style="margin-bottom:10px">
        <option value="">All categories</option>
        <option value="warning">Warning</option>
        <option value="show">Show</option>
        <option value="subtle">Subtle</option>
        <option value="aggressive">Aggressive</option>
      </select>
      <div class="anim-tile-grid" id="preview-anim-grid">
          <button class="anim-tile preview-trigger" onclick="previewShow(0)"><span class="anim-tile__name">Rainbow Scroll</span><span class="anim-tile__desc">Hue spectrum scrolls across</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(1)"><span class="anim-tile__name">Comet Chase</span><span class="anim-tile__desc">Bright tail races across</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(2)"><span class="anim-tile__name">Theater Chase</span><span class="anim-tile__desc">Marching dots</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(3)"><span class="anim-tile__name">Fire</span><span class="anim-tile__desc">Flickering heat simulation</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(4)"><span class="anim-tile__name">Meteor Shower</span><span class="anim-tile__desc">Two crossing streaks</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(5)"><span class="anim-tile__name">Police Strobe</span><span class="anim-tile__desc">Red / blue alternating flash</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(6)"><span class="anim-tile__name">Night Rider</span><span class="anim-tile__desc">KITT scanner bounce</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(7)"><span class="anim-tile__name">Color Cycle</span><span class="anim-tile__desc">Full-panel hue rotation</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(8)"><span class="anim-tile__name">Sparkle</span><span class="anim-tile__desc">Random hue pixel bursts</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(9)"><span class="anim-tile__name">Plasma</span><span class="anim-tile__desc">Sine-wave color field</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(10)"><span class="anim-tile__name">Matrix Rain</span><span class="anim-tile__desc">Green digital-rain columns</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(11)"><span class="anim-tile__name">Juggle</span><span class="anim-tile__desc">Six balls with glow halos</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(12)"><span class="anim-tile__name">BPM Bars</span><span class="anim-tile__desc">Spectrum-analyzer columns</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(13)"><span class="anim-tile__name">Confetti</span><span class="anim-tile__desc">Random-colored sparks</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(14)"><span class="anim-tile__name">Ocean Waves</span><span class="anim-tile__desc">Pacifica blue / teal waves</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(15)"><span class="anim-tile__name">Lightning</span><span class="anim-tile__desc">Dramatic white flashes</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(16)"><span class="anim-tile__name">Heartbeat</span><span class="anim-tile__desc">Lub-dub pulse in red</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(17)"><span class="anim-tile__name">Ripple</span><span class="anim-tile__desc">Rainbow rings from center</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(18)"><span class="anim-tile__name">Sunrise</span><span class="anim-tile__desc">Red to orange to yellow</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(19)"><span class="anim-tile__name">Text Scroll</span><span class="anim-tile__desc">Custom message scrolls</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(20)"><span class="anim-tile__name">Colorwaves</span><span class="anim-tile__desc">WLED silky colour bands</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(21)"><span class="anim-tile__name">Twinkle Fox</span><span class="anim-tile__desc">WLED independent pixel twinkle</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(22)"><span class="anim-tile__name">Bouncing Balls</span><span class="anim-tile__desc">WLED gravity-physics balls</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(23)"><span class="anim-tile__name">Fireworks</span><span class="anim-tile__desc">WLED flare and burst sparks</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(24)"><span class="anim-tile__name">Drip</span><span class="anim-tile__desc">WLED drops fall and bounce</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(25)"><span class="anim-tile__name">Cylon Dual</span><span class="anim-tile__desc">Two scanners converge and flash</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(26)"><span class="anim-tile__name">V8 Engine</span><span class="anim-tile__desc">Firing order 1-8-4-3-6-5-7-2</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(27)"><span class="anim-tile__name">Drag Launch</span><span class="anim-tile__desc">Tree countdown, green GO, comets</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(28)"><span class="anim-tile__name">Neon Glow</span><span class="anim-tile__desc">Tuner car colour wash</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(29)"><span class="anim-tile__name">Speed Streaks</span><span class="anim-tile__desc">Motion-blur racing lines</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(30)"><span class="anim-tile__name">Radar Sweep</span><span class="anim-tile__desc">Sweeping column scanner</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(31)"><span class="anim-tile__name">Aurora</span><span class="anim-tile__desc">Flowing northern lights</span></button>
          <button class="anim-tile preview-trigger" onclick="previewShow(32)"><span class="anim-tile__name">Glitch</span><span class="anim-tile__desc">Digital colour disruption</span></button>
      </div>
    </div>
  </div>

  <p class="group-label">Diagnostics</p>
  <div class="card">
    <div class="card-hd">
      <div class="card-icon card-icon--amber"></div>
      <div>
        <div class="card-title">Preview Diagnostics</div>
        <div class="card-desc">Last preview event and live input visibility</div>
      </div>
    </div>
    <div class="card-body">
      <div class="row"><span>Last Preview Action</span><span class="row__value" id="preview-last-action">none</span></div>
      <div class="row"><span>Last Action Age</span><span class="row__value" id="preview-last-age">--</span></div>
      <div class="row"><span>Driver Inputs</span><span class="row__value" id="preview-driver-live">0x00</span></div>
      <div class="row row--last"><span>Passenger Inputs</span><span class="row__value" id="preview-passenger-live">0x00</span></div>
    </div>
  </div>

</div><!-- /tab-preview -->

<!-- ── Action bar ─────────────────────────────────────────────────────────── -->
<div class="action-bar">
  <button class="btn btn--ghost"     onclick="resetDefaults()">Reset</button>
  <button class="btn btn--secondary" onclick="rebootDevice()">Reboot</button>
  <button class="btn btn--primary"   id="save-btn" onclick="saveSettings()">Save</button>
</div>

<!-- ── Toast ──────────────────────────────────────────────────────────────── -->
<div class="toast" id="toast"></div>

<script>
/* ── Tab navigation ──────────────────────────────────────────────────────── */
var tabs = document.querySelectorAll('.mode-btn');
tabs.forEach(function(btn) {
  btn.addEventListener('click', function() {
    tabs.forEach(function(b) { b.classList.remove('mode-btn--active'); });
    document.querySelectorAll('.page').forEach(function(p) { p.classList.add('page--hidden'); });
    btn.classList.add('mode-btn--active');
    document.getElementById('tab-' + btn.dataset.tab).classList.remove('page--hidden');
  });
});

/* ── Toast ───────────────────────────────────────────────────────────────── */
function toast(msg, type) {
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast toast--show' + (type ? ' toast--' + type : '');
  clearTimeout(t._tid);
  t._tid = setTimeout(function() { t.className = 'toast'; }, 2600);
}

/* ── Request/sync status ──────────────────────────────────────────────────── */
var g_loadInFlight = false;
var g_saveInFlight = false;
var g_lastSyncMs = 0;

function setSyncStatus(msg) {
  var el = document.getElementById('hd-sync');
  if (el) el.textContent = msg;
}

function updateSyncAge() {
  if (!g_lastSyncMs) return;
  var sec = Math.max(0, Math.floor((Date.now() - g_lastSyncMs) / 1000));
  if (sec < 5) setSyncStatus('Synced just now');
  else if (sec < 60) setSyncStatus('Synced ' + sec + 's ago');
  else setSyncStatus('Synced ' + Math.floor(sec / 60) + 'm ago');
}

/* ── Sliders ─────────────────────────────────────────────────────────────── */
function bindSlider(id, suffix) {
  var el = document.getElementById(id);
  var vl = document.getElementById(id + '-v');
  el.addEventListener('input', function() { vl.textContent = el.value + (suffix || ''); });
}
bindSlider('brightness');
bindSlider('brightness_dim', '%');
bindSlider('turn_blink_ms', ' ms');
bindSlider('frame_ms', ' ms');
bindSlider('show_speed', '%');

/* ── Display tab — live auto-persist ──────────────────────────────────────── */
function postDisplaySettings() {
  var brightnessEl = document.getElementById('brightness');
  var brightnessDimEl = document.getElementById('brightness_dim');
  var lensPresetEl = document.getElementById('lens_preset');
  var startupAnimEl = document.getElementById('startup_anim');
  if (!brightnessEl || !brightnessDimEl || !lensPresetEl || !startupAnimEl) return;
  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      brightness:     +brightnessEl.value,
      brightness_dim: +brightnessDimEl.value,
      lens_preset:    +lensPresetEl.value,
      startup_anim:   startupAnimEl.checked ? 1 : 0
    })
  })
  .then(function(r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    g_lastSyncMs = Date.now();
    updateSyncAge();
  })
  .catch(function(e) {
    console.warn('Display auto-save failed:', e && e.message ? e.message : e);
  });
}
var brightnessEl = document.getElementById('brightness');
if (brightnessEl) brightnessEl.addEventListener('change', postDisplaySettings);
var brightnessDimEl = document.getElementById('brightness_dim');
if (brightnessDimEl) brightnessDimEl.addEventListener('change', postDisplaySettings);
var startupAnimEl = document.getElementById('startup_anim');
if (startupAnimEl) startupAnimEl.addEventListener('change', postDisplaySettings);

/* ── Color pickers ───────────────────────────────────────────────────────── */
function bindColor(pid, hid) {
  var p = document.getElementById(pid);
  var h = document.getElementById(hid);
  p.addEventListener('input', function() { h.value = p.value; });
  h.addEventListener('input', function() {
    if (/^#[0-9a-fA-F]{6}$/.test(h.value)) p.value = h.value;
  });
}
bindColor('brake_color',   'brake_hex');
bindColor('turn_color',    'turn_hex');
bindColor('reverse_color', 'reverse_hex');
bindColor('run_color',     'run_hex');
var runColorEl = document.getElementById('run_color');
if (runColorEl) runColorEl.addEventListener('input', triggerRestPulse);
var runHexEl = document.getElementById('run_hex');
if (runHexEl) runHexEl.addEventListener('change', triggerRestPulse);
var brightDimEl = document.getElementById('brightness_dim');
if (brightDimEl) brightDimEl.addEventListener('change', triggerRestPulse);

/* ── Show mode — animation tiles ────────────────────────────────────────── */
var g_showAnim = 0;
var g_showAnimTiles = [];
var g_previewAnimTiles = [];
var g_previewBusy = false;
var g_previewCountdownTimer = null;
var g_previewRecent = [];
var MAX_RECENT_ANIMS = 6;
var PREVIEW_FAVORITES = [0, 5, 26, 27];
var PREVIEW_ANIM_CATS = [
  'show','show','warning','warning','aggressive','warning','warning','show','subtle','show','warning',
  'show','show','subtle','subtle','warning','subtle','show','subtle','show','show','subtle','show',
  'warning','subtle','aggressive','aggressive','aggressive','show','aggressive','warning','subtle','aggressive'
];

function setPreviewBusy(busy) {
  g_previewBusy = !!busy;
  document.querySelectorAll('.preview-trigger').forEach(function(el) { el.disabled = g_previewBusy; });
}

function setPreviewStatus(text, side) {
  document.getElementById('preview-status-text').textContent = text || 'Idle';
  document.getElementById('preview-side').textContent = side || 'None';
}

function clearPreviewVisuals() {
  document.querySelectorAll('#tab-preview .preview-card').forEach(function(el) {
    el.classList.remove('preview-card--active');
  });
  g_previewAnimTiles.forEach(function(t) {
    t.classList.remove('anim-tile--active');
    t.classList.remove('anim-tile--fade');
  });
  document.getElementById('preview-progress').style.width = '0%';
}

function startPreviewTimer(durationMs) {
  clearInterval(g_previewCountdownTimer);
  var start = Date.now();
  var total = Math.max(1, durationMs || 0);
  g_previewCountdownTimer = setInterval(function() {
    var elapsed = Date.now() - start;
    var remain = Math.max(0, total - elapsed);
    var pct = Math.max(0, Math.min(1, remain / total));
    document.getElementById('preview-progress').style.width = String(pct * 100) + '%';
    var status = 'Preview running \u2014 ' + (remain / 1000).toFixed(1) + 's left';
    setPreviewStatus(status, document.getElementById('preview-side').textContent || 'Both');
    if (remain <= 0) {
      clearInterval(g_previewCountdownTimer);
      clearPreviewVisuals();
      setPreviewStatus('Idle', 'None');
    }
  }, 100);
}

function getPreviewDuration() {
  return +document.getElementById('preview_duration_ms').value || 3000;
}

function updatePreviewWarning(msg) {
  var el = document.getElementById('preview-warning');
  if (!msg) {
    el.className = 'preview-warning';
    el.textContent = '';
    return;
  }
  el.textContent = msg;
  el.className = 'preview-warning preview-warning--show';
}
function setAnimTile(n) {
  g_showAnim = n;
  g_showAnimTiles.forEach(function(t, i) { t.classList.toggle('anim-tile--active', i === n); });
  var row = document.getElementById('show-text-row');
  if (row) row.style.display = (n === 19) ? '' : 'none';
}
function selectAnim(n) {
  setAnimTile(n);
  postShowSettings();
}
function postShowSettings() {
  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      show_mode:  document.getElementById('show_mode').checked ? 1 : 0,
      show_anim:  g_showAnim,
      show_speed: +document.getElementById('show_speed').value,
      show_text:  document.getElementById('show_text').value
    })
  }).catch(function() {});
}

/* ── Animations tab — live auto-apply ────────────────────────────────────── */
function postAnimSettings() {
  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      brake_anim:   +document.getElementById('brake_anim').value,
      turn_anim:    +document.getElementById('turn_anim').value,
      reverse_anim: +document.getElementById('reverse_anim').value,
      run_anim:     +document.getElementById('run_anim').value
    })
  }).catch(function() {});
}

/* ── Rest mode ───────────────────────────────────────────────────────────── */
var restPulseTimer = null;
function triggerRestPulse() {
  clearTimeout(restPulseTimer);
  restPulseTimer = setTimeout(function() {
    preview('rest_pulse').catch(function(e) { console.warn('Rest pulse preview failed:', e && e.message ? e.message : e); });
  }, 120);
}
function postRestSettings(withPulse) {
  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      rest_mode: document.getElementById('rest_mode').checked ? 1 : 0
    })
  })
  .then(function(r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    if (withPulse) preview('rest_pulse').catch(function(e) { console.warn('Rest settings pulse failed:', e && e.message ? e.message : e); });
  })
  .catch(function() {});
}

/* ── Software input toggles ──────────────────────────────────────────────── */
var g_softInputs = {
  driver_brake: false, driver_running: false, driver_turn: false, driver_reverse: false,
  passenger_brake: false, passenger_running: false, passenger_turn: false, passenger_reverse: false
};
function setSoftButtonState(name) {
  var el = document.getElementById('soft-' + name);
  if (!el) return;
  el.classList.toggle('preview-card--active', !!g_softInputs[name]);
}
function toggleSoft(name) {
  g_softInputs[name] = !g_softInputs[name];
  setSoftButtonState(name);
  postSoftInputs();
}
function postSoftInputs() {
  fetch('/api/test_inputs', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      enabled: document.getElementById('soft_enable').checked ? 1 : 0,
      driver: {
        brake: g_softInputs.driver_brake ? 1 : 0,
        running: g_softInputs.driver_running ? 1 : 0,
        turn: g_softInputs.driver_turn ? 1 : 0,
        reverse: g_softInputs.driver_reverse ? 1 : 0
      },
      passenger: {
        brake: g_softInputs.passenger_brake ? 1 : 0,
        running: g_softInputs.passenger_running ? 1 : 0,
        turn: g_softInputs.passenger_turn ? 1 : 0,
        reverse: g_softInputs.passenger_reverse ? 1 : 0
      }
    })
  })
  .then(function(r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    toast('Software input test updated', 'ok');
  })
  .catch(function(e) { toast('Software input update failed: ' + e.message, 'err'); });
}

/* ── Show animation preview ─────────────────────────────────────────────── */
function postPreview(payload) {
  if (g_previewBusy) return Promise.reject(new Error('Preview request in flight'));
  setPreviewBusy(true);
  return fetch('/api/preview', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  })
  .then(function(r) {
    return r.json().catch(function(e) {
      return { ok:false, error:'bad_response', details: e && e.message ? e.message : 'Invalid JSON response' };
    }).then(function(body) {
      if (!r.ok || !body.ok) {
        var msg = body.details || body.error || ('HTTP ' + r.status);
        throw new Error(msg);
      }
      return body;
    });
  })
  .finally(function() {
    setTimeout(function() { setPreviewBusy(false); }, 180);
  });
}

function applyPreviewResponse(resp) {
  clearPreviewVisuals();
  if (!resp || !resp.preview_active) {
    setPreviewStatus('Idle', 'None');
    return;
  }
  var side = (resp.side === 'driver') ? 'Driver'
           : (resp.side === 'passenger') ? 'Passenger'
           : 'Both';
  setPreviewStatus('Preview running', side);
  if (resp.requested_state) {
    var btn = document.getElementById('preview-' + resp.requested_state);
    if (btn) btn.classList.add('preview-card--active');
  }
  if (resp.duration_ms) startPreviewTimer(resp.duration_ms);
}

function postPreviewLockout() {
  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ preview_lockout: document.getElementById('preview_lockout').checked ? 1 : 0 })
  }).catch(function() {});
}

function stopPreview() {
  postPreview({ action: 'stop' })
    .then(function() {
      clearInterval(g_previewCountdownTimer);
      clearPreviewVisuals();
      setPreviewStatus('Stopped', 'None');
      toast('Preview stopped', 'ok');
    })
    .catch(function(e) { toast('Stop failed: ' + e.message, 'err'); });
}

function runCommonSequence() {
  var seq = ['running', 'brake', 'left_turn', 'right_turn', 'hazard', 'reverse'];
  var i = 0;
  var runNext = function() {
    if (i >= seq.length) return;
    var st = seq[i++];
    preview(st).then(function() {
      setTimeout(runNext, getPreviewDuration() + 120);
    }).catch(function() {});
  };
  runNext();
}

function addRecentAnim(n) {
  var idx = g_previewRecent.indexOf(n);
  if (idx >= 0) g_previewRecent.splice(idx, 1);
  g_previewRecent.unshift(n);
  g_previewRecent = g_previewRecent.slice(0, MAX_RECENT_ANIMS);
  try { localStorage.setItem('previewRecentAnims', JSON.stringify(g_previewRecent)); } catch (_) {}
  renderQuickAnims();
}

function renderQuickAnims() {
  function renderRow(id, arr, title) {
    var root = document.getElementById(id);
    if (!root) return;
    root.innerHTML = '';
    if (!arr.length) return;
    var lbl = document.createElement('span');
    lbl.className = 'quick-chip';
    lbl.textContent = title;
    root.appendChild(lbl);
    arr.forEach(function(n) {
      var tile = g_previewAnimTiles[n];
      if (!tile) return;
      var nameEl = tile.querySelector('.anim-tile__name');
      var nm = nameEl ? nameEl.textContent : ('Anim ' + n);
      var b = document.createElement('button');
      b.className = 'quick-chip preview-trigger';
      b.textContent = nm;
      if (g_previewBusy) b.disabled = true;
      b.onclick = function() { previewShow(n); };
      root.appendChild(b);
    });
  }
  renderRow('preview-favorites', PREVIEW_FAVORITES, 'Favorites');
  renderRow('preview-recents', g_previewRecent, 'Recent');
}

function previewShow(n) {
  return postPreview({ state: 'show', anim: n, duration_ms: getPreviewDuration() })
  .then(function(resp) {
    applyPreviewResponse(resp);
    g_previewAnimTiles.forEach(function(t, i) { t.classList.toggle('anim-tile--active', i === n); });
    setTimeout(function() {
      g_previewAnimTiles.forEach(function(t) {
        if (t.classList.contains('anim-tile--active')) t.classList.add('anim-tile--fade');
      });
    }, Math.max(200, getPreviewDuration() - 500));
    addRecentAnim(n);
    toast('Previewing show effect ' + n, 'ok');
  })
  .catch(function(e) { toast('Preview failed: ' + e.message, 'err'); });
}

/* ── WiFi block toggle ───────────────────────────────────────────────────── */
function updateWifiBlocks() {
  var m = document.getElementById('wifi_mode').value;
  document.getElementById('ap-block').className  = 'wifi-block' + (m === '0' ? ' wifi-block--active' : '');
  document.getElementById('sta-block').className = 'wifi-block' + (m === '1' ? ' wifi-block--active' : '');
}

/* ── Animation tile filtering ────────────────────────────────────────────── */
function bindAnimSearch(inputId, tiles) {
  var input = document.getElementById(inputId);
  var catInput = (inputId === 'preview_anim_search') ? document.getElementById('preview_anim_category') : null;
  if (!input) return;
  tiles.forEach(function(tile) {
    var name = tile.querySelector('.anim-tile__name');
    var desc = tile.querySelector('.anim-tile__desc');
    var text = ((name ? name.textContent : '') + ' ' + (desc ? desc.textContent : '')).toLowerCase();
    tile.dataset.search = text;
    if (inputId === 'preview_anim_search') {
      var idx = tiles.indexOf(tile);
      tile.dataset.category = PREVIEW_ANIM_CATS[idx] || 'show';
    }
  });
  var apply = function() {
    var q = (input.value || '').trim().toLowerCase();
    var cat = catInput ? (catInput.value || '') : '';
    tiles.forEach(function(tile) {
      var matchText = !q || tile.dataset.search.indexOf(q) !== -1;
      var matchCat = !cat || tile.dataset.category === cat;
      var show = matchText && matchCat;
      tile.classList.toggle('anim-tile--hidden', !show);
    });
  };
  input.addEventListener('input', apply);
  if (catInput) catInput.addEventListener('change', apply);
}

/* ── Lens preset description ─────────────────────────────────────────────── */
var LENS_DESCS = [
  'All LEDs active \u2014 for custom or universal builds.',
  '3-section chrome trim: outer \u2022 center \u2022 inner. Classic 87\u201393 Mustang GT look.',
  '2-section clean-lens: wide outer and inner panels. Matches 87\u201393 Mustang LX / notchback.',
  'Center 4-row horizontal bar. Great for Cobra or custom bar-style builds.'
];
function updateLensDesc() {
  document.getElementById('lens-desc').textContent = LENS_DESCS[+document.getElementById('lens_preset').value] || '';
}
var lensPresetEl = document.getElementById('lens_preset');
if (lensPresetEl) {
  lensPresetEl.addEventListener('change', function() {
    updateLensDesc();
    postDisplaySettings();
  });
}
updateLensDesc();

/* ── Color presets ───────────────────────────────────────────────────────── */
var PRESETS = {
  stock:    { brake:[255,0,0],   turn:[255,100,0], reverse:[255,255,255], run:[30,0,0]  },
  cool:     { brake:[0,80,255],  turn:[0,180,255], reverse:[200,220,255], run:[0,0,30]  },
  purple:   { brake:[180,0,255], turn:[130,0,200], reverse:[200,200,255], run:[20,0,30] },
  sport:    { brake:[255,30,0],  turn:[255,200,0], reverse:[255,255,180], run:[30,5,0]  },
  murdered: { brake:[120,0,0],   turn:[140,50,0],  reverse:[70,70,70],   run:[10,0,0]  }
};
function applyPreset(name) {
  var p = PRESETS[name]; if (!p) return;
  setColor('brake_color',   'brake_hex',   p.brake[0],   p.brake[1],   p.brake[2]);
  setColor('turn_color',    'turn_hex',    p.turn[0],    p.turn[1],    p.turn[2]);
  setColor('reverse_color', 'reverse_hex', p.reverse[0], p.reverse[1], p.reverse[2]);
  setColor('run_color',     'run_hex',     p.run[0],     p.run[1],     p.run[2]);
  toast('Preset applied \u2014 tap Save to keep it');
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */
function rgb2hex(r, g, b) {
  return '#' + [r,g,b].map(function(v) { return ('0' + v.toString(16)).slice(-2); }).join('');
}
function hex2rgb(hex) {
  return { r: parseInt(hex.slice(1,3),16), g: parseInt(hex.slice(3,5),16), b: parseInt(hex.slice(5,7),16) };
}
function setSlider(id, val, suffix) {
  if (val == null) return;
  document.getElementById(id).value = val;
  var vl = document.getElementById(id + '-v');
  if (vl) vl.textContent = val + (suffix || '');
}
function setColor(pid, hid, r, g, b) {
  if (r == null) return;
  var hex = rgb2hex(r, g, b);
  document.getElementById(pid).value = hex;
  document.getElementById(hid).value = hex;
}
function fmtUptime(s) {
  var h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
  if (h > 0) return h + 'h ' + m + 'm';
  if (m > 0) return m + 'm ' + sec + 's';
  return sec + 's';
}
function fmtMaskHex(v) {
  return '0x' + ('0' + (v & 0xFF).toString(16)).slice(-2).toUpperCase();
}

/* ── Load settings ───────────────────────────────────────────────────────── */
function loadSettings() {
  if (g_loadInFlight || g_saveInFlight || document.hidden) return;
  g_loadInFlight = true;
  setSyncStatus('Syncing\u2026');
  fetch('/api/settings')
    .then(function(r) { return r.json(); })
    .then(function(s) {
      setSlider('brightness',     s.brightness);
      setSlider('brightness_dim', s.brightness_dim, '%');
      setSlider('turn_blink_ms',  s.turn_blink_ms, ' ms');
      setSlider('frame_ms',       s.frame_ms,       ' ms');

      setColor('brake_color',   'brake_hex',   s.brake_r,   s.brake_g,   s.brake_b);
      setColor('turn_color',    'turn_hex',    s.turn_r,    s.turn_g,    s.turn_b);
      setColor('reverse_color', 'reverse_hex', s.reverse_r, s.reverse_g, s.reverse_b);
      setColor('run_color',     'run_hex',     s.run_r,     s.run_g,     s.run_b);

      if (s.brake_anim   != null) document.getElementById('brake_anim').value   = s.brake_anim;
      if (s.turn_anim    != null) document.getElementById('turn_anim').value    = s.turn_anim;
      if (s.reverse_anim != null) document.getElementById('reverse_anim').value = s.reverse_anim;
      if (s.run_anim     != null) document.getElementById('run_anim').value     = s.run_anim;
      if (s.lens_preset  != null) {
        document.getElementById('lens_preset').value = s.lens_preset;
        updateLensDesc();
      }
      if (s.startup_anim != null) document.getElementById('startup_anim').checked = !!s.startup_anim;
      if (s.rest_mode   != null) document.getElementById('rest_mode').checked = !!s.rest_mode;

      if (s.show_mode  != null) document.getElementById('show_mode').checked = !!s.show_mode;
      if (s.show_anim  != null) setAnimTile(s.show_anim);
      if (s.show_speed != null) { setSlider('show_speed', s.show_speed, '%'); }
      if (s.show_text  != null) document.getElementById('show_text').value = s.show_text;

      document.getElementById('wifi_mode').value = s.wifi_mode;
      document.getElementById('ap_ssid').value   = s.ap_ssid  || '';
      document.getElementById('ap_pass').value   = '';
      document.getElementById('sta_ssid').value  = s.sta_ssid || '';
      document.getElementById('sta_pass').value  = '';
      updateWifiBlocks();

      document.getElementById('soft_enable').checked = !!s.soft_inputs_enabled;
      var dm = (+s.soft_driver_mask) || 0;
      var pm = (+s.soft_passenger_mask) || 0;
      g_softInputs.driver_brake      = !!(dm & 0x01);
      g_softInputs.driver_running    = !!(dm & 0x02);
      g_softInputs.driver_turn       = !!(dm & 0x04);
      g_softInputs.driver_reverse    = !!(dm & 0x08);
      g_softInputs.passenger_brake   = !!(pm & 0x01);
      g_softInputs.passenger_running = !!(pm & 0x02);
      g_softInputs.passenger_turn    = !!(pm & 0x04);
      g_softInputs.passenger_reverse = !!(pm & 0x08);
      Object.keys(g_softInputs).forEach(setSoftButtonState);

      if (s.preview_lockout_enabled != null) document.getElementById('preview_lockout').checked = !!s.preview_lockout_enabled;
      if (s.preview_last_action != null) document.getElementById('preview-last-action').textContent = s.preview_last_action;
      if (s.preview_last_action_ms_ago != null) {
        document.getElementById('preview-last-age').textContent =
          (s.preview_last_action_ms_ago > 0) ? Math.round(s.preview_last_action_ms_ago / 1000) + 's ago' : '--';
      }
      var liveDm = (+s.live_driver_mask) || 0;
      var livePm = (+s.live_passenger_mask) || 0;
      document.getElementById('preview-driver-live').textContent = fmtMaskHex(liveDm);
      document.getElementById('preview-passenger-live').textContent = fmtMaskHex(livePm);
      if (s.preview_hard_override) {
        updatePreviewWarning('Safety override: physical brake/reverse is active and preview cannot suppress it.');
      } else {
        updatePreviewWarning('');
      }
      if (s.preview_active) {
        applyPreviewResponse({
          preview_active: 1,
          requested_state: s.preview_driver_state,
          side: (s.preview_driver_state !== 'off' && s.preview_passenger_state === 'off') ? 'driver'
               : (s.preview_passenger_state !== 'off' && s.preview_driver_state === 'off') ? 'passenger' : 'both',
          duration_ms: (+s.preview_remaining_ms) || getPreviewDuration()
        });
      } else {
        clearInterval(g_previewCountdownTimer);
        clearPreviewVisuals();
        setPreviewStatus('Idle', 'None');
      }

      var ip = s.ip || '--';
      var activeAp = (s.wifi_ap_active != null) ? !!s.wifi_ap_active : (s.wifi_mode === 0);
      var mode = activeAp ? 'Access Point' : 'Station';
      document.getElementById('hd-ip').textContent       = ip;
      document.getElementById('hd-mode').textContent     = activeAp ? 'AP' : 'STA';
      document.getElementById('info-ip').textContent     = ip;
      document.getElementById('info-mode').textContent   = mode;
      document.getElementById('info-uptime').textContent = fmtUptime(s.uptime_s || 0);
      g_lastSyncMs = Date.now();
      updateSyncAge();
    })
    .catch(function() {
      setSyncStatus('Sync failed');
      toast('Could not reach device', 'err');
    })
    .finally(function() { g_loadInFlight = false; });
}

/* ── Collect settings ────────────────────────────────────────────────────── */
function collectSettings() {
  var br = hex2rgb(document.getElementById('brake_color').value);
  var tu = hex2rgb(document.getElementById('turn_color').value);
  var rv = hex2rgb(document.getElementById('reverse_color').value);
  var rn = hex2rgb(document.getElementById('run_color').value);
  return {
    brightness:     +document.getElementById('brightness').value,
    brightness_dim: +document.getElementById('brightness_dim').value,
    turn_blink_ms:  +document.getElementById('turn_blink_ms').value,
    frame_ms:       +document.getElementById('frame_ms').value,
    brake_r: br.r, brake_g: br.g, brake_b: br.b,
    turn_r:  tu.r, turn_g:  tu.g, turn_b:  tu.b,
    reverse_r: rv.r, reverse_g: rv.g, reverse_b: rv.b,
    run_r: rn.r, run_g: rn.g, run_b: rn.b,
    brake_anim:   +document.getElementById('brake_anim').value,
    turn_anim:    +document.getElementById('turn_anim').value,
    reverse_anim: +document.getElementById('reverse_anim').value,
    run_anim:     +document.getElementById('run_anim').value,
    lens_preset:  +document.getElementById('lens_preset').value,
    startup_anim:  document.getElementById('startup_anim').checked ? 1 : 0,
    rest_mode:     document.getElementById('rest_mode').checked ? 1 : 0,
    preview_lockout: document.getElementById('preview_lockout').checked ? 1 : 0,
    show_mode:  document.getElementById('show_mode').checked ? 1 : 0,
    show_anim:  g_showAnim,
    show_speed: +document.getElementById('show_speed').value,
    show_text:  document.getElementById('show_text').value,
    wifi_mode:    +document.getElementById('wifi_mode').value,
    ap_ssid:  document.getElementById('ap_ssid').value,
    ap_pass:  document.getElementById('ap_pass').value,
    sta_ssid: document.getElementById('sta_ssid').value,
    sta_pass: document.getElementById('sta_pass').value
  };
}

/* ── Save settings ───────────────────────────────────────────────────────── */
function saveSettings() {
  var btn = document.getElementById('save-btn');
  var saved = false;
  g_saveInFlight = true;
  btn.textContent = 'Saving\u2026';
  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(collectSettings())
  })
  .then(function(r) {
    if (!r.ok) throw new Error('HTTP ' + r.status);
    toast('Settings saved \u2713', 'ok');
    g_lastSyncMs = Date.now();
    updateSyncAge();
    saved = true;
  })
  .catch(function(e) { toast('Save failed: ' + e.message, 'err'); })
  .finally(function() {
    g_saveInFlight = false;
    btn.textContent = 'Save';
    if (saved) loadSettings();
  });
}

/* ── Reset defaults ──────────────────────────────────────────────────────── */
function resetDefaults() {
  if (!confirm('Reset all settings to factory defaults?')) return;
  fetch('/api/reset', { method: 'POST' })
    .then(function(r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      toast('Reset to defaults \u2713', 'ok');
      loadSettings();
    })
    .catch(function() { toast('Reset failed', 'err'); });
}

/* ── Reboot ──────────────────────────────────────────────────────────────── */
function rebootDevice() {
  if (!confirm('Reboot the device now?')) return;
  fetch('/api/reboot', { method: 'POST' })
    .then(function() { toast('Rebooting\u2026'); })
    .catch(function() { toast('Rebooting\u2026'); });
}

/* ── Preview ─────────────────────────────────────────────────────────────── */
function preview(state, btn) {
  if (state === 'off') return stopPreview();
  var payload = { state: state, duration_ms: getPreviewDuration() };
  return postPreview(payload)
    .then(function(resp) {
      applyPreviewResponse(resp);
      if (btn) btn.classList.add('preview-card--active');
      toast('Previewing: ' + state.replace('_', ' '), 'ok');
    })
    .catch(function(e) {
      toast('Preview failed: ' + e.message, 'err');
      throw e;
    });
}

/* ── Boot ────────────────────────────────────────────────────────────────── */
g_showAnimTiles = Array.prototype.slice.call(document.querySelectorAll('#anim-grid .anim-tile'));
g_previewAnimTiles = Array.prototype.slice.call(document.querySelectorAll('#preview-anim-grid .anim-tile'));
bindAnimSearch('show_anim_search', g_showAnimTiles);
bindAnimSearch('preview_anim_search', g_previewAnimTiles);
try {
  g_previewRecent = JSON.parse(localStorage.getItem('previewRecentAnims') || '[]');
  if (!Array.isArray(g_previewRecent)) g_previewRecent = [];
} catch (_) { g_previewRecent = []; }
var normalizedRecent = g_previewRecent.map(function(v) { return +v; });
var validRecent = normalizedRecent.filter(function(v) { return v >= 0 && v < g_previewAnimTiles.length; });
g_previewRecent = validRecent.slice(0, MAX_RECENT_ANIMS);
renderQuickAnims();
setPreviewStatus('Idle', 'None');

loadSettings();
setInterval(loadSettings, 15000);
setInterval(updateSyncAge, 1000);
document.addEventListener('visibilitychange', function() {
  if (!document.hidden) loadSettings();
});
</script>
</body>
</html>
)rawhtml";

// ---------------------------------------------------------------------------
// Helper: set CORS headers so the page works when accessed from any origin
// during development.
// ---------------------------------------------------------------------------
static void addCorsHeaders() {
    _server.sendHeader("Access-Control-Allow-Origin",  "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static const char* lightStateName(LightState s) {
    switch (s) {
        case LightState::OFF:        return "off";
        case LightState::RUNNING:    return "running";
        case LightState::BRAKE:      return "brake";
        case LightState::TURN:       return "turn";
        case LightState::REVERSE:    return "reverse";
        case LightState::BRAKE_TURN: return "brake_turn";
        case LightState::HAZARD:     return "hazard";
        case LightState::SHOW:       return "show";
        case LightState::CUSTOM:     return "custom";
        default:                     return "off";
    }
}

static void sendJsonError(int code, const char* err, const char* details = nullptr) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = err ? err : "error";
    if (details && details[0] != '\0') doc["details"] = details;
    String out;
    serializeJson(doc, out);
    addCorsHeaders();
    _server.send(code, "application/json", out);
}

// ---------------------------------------------------------------------------
// GET /
// ---------------------------------------------------------------------------
static void handleRoot() {
    _server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    _server.sendHeader("Pragma", "no-cache");
    _server.sendHeader("Expires", "0");
    _server.send_P(200, "text/html", INDEX_HTML);
}

// ---------------------------------------------------------------------------
// GET /api/settings
// Returns all current settings + live status as JSON.
// Passwords are intentionally omitted from the response for security.
// ---------------------------------------------------------------------------
static void handleGetSettings() {
    JsonDocument doc;

    doc["brightness"]     = g_settings.brightness;
    doc["brightness_dim"] = g_settings.brightness_dim;
    doc["turn_blink_ms"]  = g_settings.turn_blink_ms;
    doc["frame_ms"]       = g_settings.frame_ms;

    doc["brake_r"]   = g_settings.brake_r;
    doc["brake_g"]   = g_settings.brake_g;
    doc["brake_b"]   = g_settings.brake_b;

    doc["turn_r"]    = g_settings.turn_r;
    doc["turn_g"]    = g_settings.turn_g;
    doc["turn_b"]    = g_settings.turn_b;

    doc["reverse_r"] = g_settings.reverse_r;
    doc["reverse_g"] = g_settings.reverse_g;
    doc["reverse_b"] = g_settings.reverse_b;

    doc["run_r"]     = g_settings.run_r;
    doc["run_g"]     = g_settings.run_g;
    doc["run_b"]     = g_settings.run_b;

    doc["brake_anim"]   = g_settings.brake_anim;
    doc["turn_anim"]    = g_settings.turn_anim;
    doc["reverse_anim"] = g_settings.reverse_anim;
    doc["run_anim"]     = g_settings.run_anim;
    doc["lens_preset"]  = g_settings.lens_preset;
    doc["startup_anim"] = g_settings.startup_anim;
    doc["rest_mode"]    = g_settings.rest_mode;

    doc["show_mode"]  = g_settings.show_mode;
    doc["show_anim"]  = g_settings.show_anim;
    doc["show_speed"] = g_settings.show_speed;
    doc["show_text"]  = g_settings.show_text;

    doc["wifi_mode"] = g_settings.wifi_mode;
    doc["wifi_ap_active"] = g_ap_mode_active ? 1 : 0;
    doc["ap_ssid"]   = g_settings.ap_ssid;
    doc["ap_pass"]   = "";          // never echo passwords
    doc["sta_ssid"]  = g_settings.sta_ssid;
    doc["sta_pass"]  = "";

    doc["soft_inputs_enabled"] = g_soft_inputs_enabled ? 1 : 0;
    doc["soft_driver_mask"]    = g_soft_driver_mask;
    doc["soft_passenger_mask"] = g_soft_passenger_mask;
    doc["live_driver_mask"]    = g_live_driver_inputs;
    doc["live_passenger_mask"] = g_live_passenger_inputs;

    const unsigned long nowMs = millis();
    const bool previewActive = nowMs < g_preview_until_ms;
    const uint8_t dIn = g_live_driver_inputs;
    const uint8_t pIn = g_live_passenger_inputs;
    const bool hardOverride = ((dIn | pIn) & (0x01 | 0x08)) != 0;
    doc["preview_active"] = previewActive ? 1 : 0;
    doc["preview_remaining_ms"] = previewActive ? (g_preview_until_ms - nowMs) : 0;
    doc["preview_driver_state"] = lightStateName(g_preview_driver);
    doc["preview_passenger_state"] = lightStateName(g_preview_passenger);
    doc["preview_lockout_enabled"] = g_preview_lockout_enabled ? 1 : 0;
    doc["preview_hard_override"] = hardOverride ? 1 : 0;
    doc["preview_last_action"] = g_preview_last_action;
    doc["preview_last_action_ms_ago"] = g_preview_last_action_ms ? (nowMs - g_preview_last_action_ms) : 0;

    doc["uptime_s"]  = millis() / 1000UL;
    doc["ip"]        = g_ap_mode_active
                       ? WiFi.softAPIP().toString()
                       : WiFi.localIP().toString();

    String out;
    serializeJson(doc, out);
    addCorsHeaders();
    _server.send(200, "application/json", out);
}

// ---------------------------------------------------------------------------
// POST /api/settings
// Accepts a JSON body, updates g_settings, persists to NVS, applies
// brightness immediately.  Only fields present in the body are changed.
// ---------------------------------------------------------------------------
static void handlePostSettings() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"error\":\"Empty body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, _server.arg("plain"));
    if (err) {
        _server.send(400, "application/json", "{\"error\":\"Bad JSON\"}");
        return;
    }

    // ── Numeric settings ────────────────────────────────────────────────────
    if (doc["brightness"].is<int>())
        g_settings.brightness     = (uint8_t)constrain(doc["brightness"].as<int>(), 10, 255);
    if (doc["brightness_dim"].is<int>())
        g_settings.brightness_dim = (uint8_t)constrain(doc["brightness_dim"].as<int>(), 5, 100);
    if (doc["turn_blink_ms"].is<int>())
        g_settings.turn_blink_ms  = (uint16_t)constrain(doc["turn_blink_ms"].as<int>(), 200, 1500);
    if (doc["frame_ms"].is<int>())
        g_settings.frame_ms       = (uint8_t)constrain(doc["frame_ms"].as<int>(), 10, 100);

    if (doc["brake_r"].is<int>()) g_settings.brake_r = (uint8_t)constrain(doc["brake_r"].as<int>(), 0, 255);
    if (doc["brake_g"].is<int>()) g_settings.brake_g = (uint8_t)constrain(doc["brake_g"].as<int>(), 0, 255);
    if (doc["brake_b"].is<int>()) g_settings.brake_b = (uint8_t)constrain(doc["brake_b"].as<int>(), 0, 255);

    if (doc["turn_r"].is<int>())  g_settings.turn_r  = (uint8_t)constrain(doc["turn_r"].as<int>(),  0, 255);
    if (doc["turn_g"].is<int>())  g_settings.turn_g  = (uint8_t)constrain(doc["turn_g"].as<int>(),  0, 255);
    if (doc["turn_b"].is<int>())  g_settings.turn_b  = (uint8_t)constrain(doc["turn_b"].as<int>(),  0, 255);

    if (doc["reverse_r"].is<int>()) g_settings.reverse_r = (uint8_t)constrain(doc["reverse_r"].as<int>(), 0, 255);
    if (doc["reverse_g"].is<int>()) g_settings.reverse_g = (uint8_t)constrain(doc["reverse_g"].as<int>(), 0, 255);
    if (doc["reverse_b"].is<int>()) g_settings.reverse_b = (uint8_t)constrain(doc["reverse_b"].as<int>(), 0, 255);

    if (doc["brake_anim"].is<int>())
        g_settings.brake_anim   = (uint8_t)constrain(doc["brake_anim"].as<int>(),   0, 5);
    if (doc["turn_anim"].is<int>())
        g_settings.turn_anim    = (uint8_t)constrain(doc["turn_anim"].as<int>(),    0, 5);
    if (doc["reverse_anim"].is<int>())
        g_settings.reverse_anim = (uint8_t)constrain(doc["reverse_anim"].as<int>(), 0, 3);
    if (doc["run_anim"].is<int>())
        g_settings.run_anim     = (uint8_t)constrain(doc["run_anim"].as<int>(),     0, 3);
    if (doc["lens_preset"].is<int>())
        g_settings.lens_preset  = (uint8_t)constrain(doc["lens_preset"].as<int>(),  0, 3);

    if (doc["run_r"].is<int>()) g_settings.run_r = (uint8_t)constrain(doc["run_r"].as<int>(), 0, 255);
    if (doc["run_g"].is<int>()) g_settings.run_g = (uint8_t)constrain(doc["run_g"].as<int>(), 0, 255);
    if (doc["run_b"].is<int>()) g_settings.run_b = (uint8_t)constrain(doc["run_b"].as<int>(), 0, 255);
    if (doc["startup_anim"].is<int>())
        g_settings.startup_anim = (uint8_t)constrain(doc["startup_anim"].as<int>(), 0, 1);
    if (doc["rest_mode"].is<int>())
        g_settings.rest_mode    = (uint8_t)constrain(doc["rest_mode"].as<int>(), 0, 1);
    if (doc["preview_lockout"].is<int>())
        g_preview_lockout_enabled = (uint8_t)constrain(doc["preview_lockout"].as<int>(), 0, 1);

    if (doc["show_mode"].is<int>())
        g_settings.show_mode  = (uint8_t)constrain(doc["show_mode"].as<int>(), 0, 1);
    if (doc["show_anim"].is<int>())
        g_settings.show_anim  = (uint8_t)constrain(doc["show_anim"].as<int>(), 0, 32);
    if (doc["show_speed"].is<int>())
        g_settings.show_speed = (uint8_t)constrain(doc["show_speed"].as<int>(), 50, 200);
    if (doc["show_text"].is<const char*>()) {
        const char* txt = doc["show_text"].as<const char*>();
        if (txt) {
            strncpy(g_settings.show_text, txt, sizeof(g_settings.show_text) - 1);
            g_settings.show_text[sizeof(g_settings.show_text) - 1] = '\0';
        }
    }

    if (doc["wifi_mode"].is<int>())
        g_settings.wifi_mode = (uint8_t)constrain(doc["wifi_mode"].as<int>(), 0, 1);

    // ── String settings (guarded against buffer overrun) ────────────────────
    auto copyStr = [](const char* src, char* dst, size_t dstSize) {
        if (src && src[0] != '\0') {
            strncpy(dst, src, dstSize - 1);
            dst[dstSize - 1] = '\0';
        }
    };

    const char* ap_ssid = doc["ap_ssid"].as<const char*>();
    copyStr(ap_ssid, g_settings.ap_ssid, sizeof(g_settings.ap_ssid));

    // Only update password if non-empty (empty means "leave unchanged").
    // AP passwords require a WPA2 minimum of 8 characters.
    const char* ap_pass = doc["ap_pass"].as<const char*>();
    if (ap_pass && strlen(ap_pass) >= 8)
        copyStr(ap_pass, g_settings.ap_pass, sizeof(g_settings.ap_pass));

    const char* sta_ssid = doc["sta_ssid"].as<const char*>();
    copyStr(sta_ssid, g_settings.sta_ssid, sizeof(g_settings.sta_ssid));

    // Station passwords have no enforced minimum (external networks vary);
    // an empty string means "leave unchanged".
    const char* sta_pass = doc["sta_pass"].as<const char*>();
    if (sta_pass && strlen(sta_pass) > 0)
        copyStr(sta_pass, g_settings.sta_pass, sizeof(g_settings.sta_pass));

    // Apply brightness immediately without waiting for a reboot
    FastLED.setBrightness(g_settings.brightness);

    settings_save();
    addCorsHeaders();
    _server.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// POST /api/reset
// Restores compiled-in defaults, persists, re-applies brightness.
// ---------------------------------------------------------------------------
static void handleReset() {
    settings_reset();
    FastLED.setBrightness(g_settings.brightness);
    addCorsHeaders();
    _server.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// POST /api/reboot
// Sends the response first, then restarts the ESP32.
// ---------------------------------------------------------------------------
static void handleReboot() {
    addCorsHeaders();
    _server.send(200, "application/json", "{\"ok\":true}");
    _server.client().stop();
    delay(200);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// POST /api/preview
// Body: { "state": "...", "duration_ms": 1000|3000|5000, "anim": 0..32 }
//   state supports: brake/left_turn/right_turn/reverse/hazard/running/show,
//   per-side driver_* and passenger_* states, rest_pulse, and off (stop).
// Also accepts { "action":"stop" } for explicit cancellation.
// Overrides light state briefly so the user can preview animations with current
// color settings without triggering physical inputs.
// ---------------------------------------------------------------------------
static void handlePreview() {
    if (!_server.hasArg("plain")) {
        sendJsonError(400, "empty_body", "Request body is required");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
        sendJsonError(400, "bad_json", "JSON payload could not be parsed");
        return;
    }

    const unsigned long nowMs = millis();
    const uint8_t dIn = g_live_driver_inputs;
    const uint8_t pIn = g_live_passenger_inputs;
    const uint8_t activeDriveMask = (dIn | pIn) & 0x0F;
    if (activeDriveMask) {
        if (g_preview_drive_active_since_ms == 0) {
            g_preview_drive_active_since_ms = nowMs;
        }
    } else {
        g_preview_drive_active_since_ms = 0;
    }
    const bool lockoutActive = g_preview_lockout_enabled
                            && g_preview_drive_active_since_ms != 0
                            && (nowMs - g_preview_drive_active_since_ms) >= PREVIEW_LOCKOUT_AFTER_MS;

    const char* action = doc["action"].as<const char*>();
    const char* s = doc["state"].as<const char*>();
    const bool stopReq = (action && strcmp(action, "stop") == 0) || (s && strcmp(s, "off") == 0);
    if (!stopReq && !s) {
        sendJsonError(400, "missing_state", "Provide state or action=stop");
        return;
    }

    if (stopReq) {
        g_preview_driver = LightState::OFF;
        g_preview_passenger = LightState::OFF;
        g_preview_until_ms = 0;
        g_rest_pulse_until_ms = 0;
        strncpy(g_preview_last_action, "stop", sizeof(g_preview_last_action) - 1);
        g_preview_last_action[sizeof(g_preview_last_action) - 1] = '\0';
        g_preview_last_action_ms = nowMs;

        JsonDocument resp;
        resp["ok"] = true;
        resp["action"] = "stop";
        resp["preview_active"] = 0;
        resp["applied_driver"] = "off";
        resp["applied_passenger"] = "off";
        String out;
        serializeJson(resp, out);
        addCorsHeaders();
        _server.send(200, "application/json", out);
        return;
    }

    if (lockoutActive) {
        sendJsonError(423, "lockout_active", "Preview lockout is active while live driving signals are present");
        return;
    }

    LightState ld = LightState::OFF;
    LightState lp = LightState::OFF;
    const int reqDuration = doc["duration_ms"].is<int>() ? doc["duration_ms"].as<int>() : 3000;
    unsigned long durationMs = (unsigned long)constrain(reqDuration, 1000, 5000);
    const char* side = "both";
    g_rest_pulse_until_ms = 0;
    if      (strcmp(s, "brake")       == 0) { ld = LightState::BRAKE;      lp = LightState::BRAKE; }
    else if (strcmp(s, "left_turn")   == 0) { ld = LightState::TURN;       lp = LightState::OFF; side = "driver"; }
    else if (strcmp(s, "right_turn")  == 0) { ld = LightState::OFF;        lp = LightState::TURN; side = "passenger"; }
    else if (strcmp(s, "reverse")     == 0) { ld = LightState::REVERSE;    lp = LightState::REVERSE; }
    else if (strcmp(s, "hazard")      == 0) { ld = LightState::HAZARD;     lp = LightState::HAZARD; }
    else if (strcmp(s, "brake_left")  == 0) { ld = LightState::BRAKE_TURN; lp = LightState::BRAKE; side = "driver"; }
    else if (strcmp(s, "brake_right") == 0) { ld = LightState::BRAKE;      lp = LightState::BRAKE_TURN; side = "passenger"; }
    else if (strcmp(s, "running")     == 0) { ld = LightState::RUNNING;    lp = LightState::RUNNING; }
    else if (strcmp(s, "driver_brake")    == 0) { ld = LightState::BRAKE;   lp = LightState::OFF; side = "driver"; }
    else if (strcmp(s, "driver_running")  == 0) { ld = LightState::RUNNING; lp = LightState::OFF; side = "driver"; }
    else if (strcmp(s, "driver_reverse")  == 0) { ld = LightState::REVERSE; lp = LightState::OFF; side = "driver"; }
    else if (strcmp(s, "passenger_brake")   == 0) { ld = LightState::OFF; lp = LightState::BRAKE;   side = "passenger"; }
    else if (strcmp(s, "passenger_running") == 0) { ld = LightState::OFF; lp = LightState::RUNNING; side = "passenger"; }
    else if (strcmp(s, "passenger_reverse") == 0) { ld = LightState::OFF; lp = LightState::REVERSE; side = "passenger"; }
    else if (strcmp(s, "rest_pulse")  == 0) {
        ld = LightState::RUNNING;
        lp = LightState::RUNNING;
        durationMs = REST_PULSE_DURATION_MS;
        g_rest_pulse_until_ms = nowMs + durationMs;
    }
    else if (strcmp(s, "show")        == 0) {
        // Optional anim index — update show_anim so the correct effect plays
        if (doc["anim"].is<int>()) {
            g_settings.show_anim = (uint8_t)constrain(doc["anim"].as<int>(), 0, 32);
        }
        ld = LightState::SHOW;
        lp = LightState::SHOW;
        durationMs = max(durationMs, 1000UL);
    }
    else {
        sendJsonError(400, "invalid_state", "Unsupported preview state");
        return;
    }

    g_preview_driver    = ld;
    g_preview_passenger = lp;
    g_preview_until_ms  = nowMs + durationMs;
    strncpy(g_preview_last_action, s, sizeof(g_preview_last_action) - 1);
    g_preview_last_action[sizeof(g_preview_last_action) - 1] = '\0';
    g_preview_last_action_ms = nowMs;

    const bool hardOverride = ((dIn | pIn) & (0x01 | 0x08)) != 0;

    JsonDocument resp;
    resp["ok"] = true;
    resp["requested_state"] = s;
    resp["applied_driver"] = lightStateName(ld);
    resp["applied_passenger"] = lightStateName(lp);
    resp["side"] = side;
    resp["duration_ms"] = durationMs;
    resp["preview_until_ms"] = g_preview_until_ms;
    resp["preview_active"] = 1;
    resp["hard_override_active"] = hardOverride ? 1 : 0;
    resp["lockout_active"] = lockoutActive ? 1 : 0;
    if (strcmp(s, "show") == 0) {
        resp["anim"] = g_settings.show_anim;
    }

    String out;
    serializeJson(resp, out);
    addCorsHeaders();
    _server.send(200, "application/json", out);
}

// ---------------------------------------------------------------------------
// POST /api/test_inputs
// Body:
// {
//   "enabled": 0|1,
//   "driver":    {"brake":0|1,"running":0|1,"turn":0|1,"reverse":0|1},
//   "passenger": {"brake":0|1,"running":0|1,"turn":0|1,"reverse":0|1}
// }
// Lets the UI force software input bits ON for bench debugging.
// ---------------------------------------------------------------------------
static void handleTestInputs() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"error\":\"Empty body\"}");
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
        _server.send(400, "application/json", "{\"error\":\"Bad JSON\"}");
        return;
    }

    auto asBool = [](JsonVariantConst v) -> bool {
        if (v.is<bool>()) return v.as<bool>();
        if (v.is<int>())  return v.as<int>() != 0;
        return false;
    };

    if (doc["enabled"].is<bool>() || doc["enabled"].is<int>()) {
        g_soft_inputs_enabled = asBool(doc["enabled"]) ? 1 : 0;
    }

    if (doc["driver"].is<JsonObjectConst>()) {
        JsonObjectConst d = doc["driver"].as<JsonObjectConst>();
        uint8_t mask = 0;
        if (asBool(d["brake"]))   mask |= 0x01;
        if (asBool(d["running"])) mask |= 0x02;
        if (asBool(d["turn"]))    mask |= 0x04;
        if (asBool(d["reverse"])) mask |= 0x08;
        g_soft_driver_mask = mask;
    }

    if (doc["passenger"].is<JsonObjectConst>()) {
        JsonObjectConst p = doc["passenger"].as<JsonObjectConst>();
        uint8_t mask = 0;
        if (asBool(p["brake"]))   mask |= 0x01;
        if (asBool(p["running"])) mask |= 0x02;
        if (asBool(p["turn"]))    mask |= 0x04;
        if (asBool(p["reverse"])) mask |= 0x08;
        g_soft_passenger_mask = mask;
    }

    addCorsHeaders();
    _server.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
// Global WifiServer instance
// ---------------------------------------------------------------------------
WifiServer wifiServer;

// ---------------------------------------------------------------------------
void WifiServer::begin() {
    Serial.println(F("[wifi] starting..."));
    g_ap_mode_active = false;

    if (g_settings.wifi_mode == 0) {
        // ── Access Point mode ────────────────────────────────────────────────
        WiFi.mode(WIFI_AP);
        WiFi.softAP(g_settings.ap_ssid, g_settings.ap_pass);
        g_ap_mode_active = true;
        Serial.print(F("[wifi] AP \""));
        Serial.print(g_settings.ap_ssid);
        Serial.print(F("\"  IP: "));
        Serial.println(WiFi.softAPIP());
    } else {
        // ── Station mode — connect with 10-second timeout ────────────────────
        WiFi.mode(WIFI_STA);
        WiFi.begin(g_settings.sta_ssid, g_settings.sta_pass);
        Serial.print(F("[wifi] connecting to \""));
        Serial.print(g_settings.sta_ssid);
        Serial.print('"');

        uint8_t tries = 0;
        while (WiFi.status() != WL_CONNECTED && tries < 20) {
            delay(500);
            Serial.print('.');
            tries++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(F("\n[wifi] IP: "));
            Serial.println(WiFi.localIP());
        } else {
            // Fall back to AP so the device is always reachable
            Serial.println(F("\n[wifi] connection failed — falling back to AP"));
            WiFi.mode(WIFI_AP);
            WiFi.softAP(g_settings.ap_ssid, g_settings.ap_pass);
            g_ap_mode_active = true;
            Serial.print(F("[wifi] AP fallback IP: "));
            Serial.println(WiFi.softAPIP());
        }
    }

    // ── Register HTTP routes ─────────────────────────────────────────────────
    _server.on("/",               HTTP_GET,  handleRoot);
    _server.on("/index.html",     HTTP_GET,  handleRoot);
    _server.on("/api/settings",   HTTP_GET,  handleGetSettings);
    _server.on("/api/settings",   HTTP_POST, handlePostSettings);
    _server.on("/api/reset",      HTTP_POST, handleReset);
    _server.on("/api/reboot",     HTTP_POST, handleReboot);
    _server.on("/api/preview",    HTTP_POST, handlePreview);
    _server.on("/api/test_inputs", HTTP_POST, handleTestInputs);

    // Common captive-portal probe URLs used by Android / iOS / Windows.
    auto captive = []() { handleRoot(); };
    _server.on("/generate_204",             HTTP_GET, captive);
    _server.on("/gen_204",                  HTTP_GET, captive);
    _server.on("/hotspot-detect.html",      HTTP_GET, captive);
    _server.on("/library/test/success.html",HTTP_GET, captive);
    _server.on("/ncsi.txt",                 HTTP_GET, captive);
    _server.on("/connecttest.txt",          HTTP_GET, captive);
    _server.on("/fwlink",                   HTTP_GET, captive);

    // Captive-portal + CORS fallback
    _server.onNotFound([]() {
        if (_server.method() == HTTP_OPTIONS) {
            addCorsHeaders();
            _server.send(204);
        } else {
            // In AP mode, serve the settings page for unknown URLs so captive
            // assistants stay on-device. In STA mode, keep a normal redirect.
            if (g_ap_mode_active) {
                handleRoot();
            } else {
                _server.sendHeader("Location", "http://" + WiFi.localIP().toString(), true);
                _server.send(302, "text/plain", "");
            }
        }
    });

    // ── Captive portal DNS (AP mode only) ────────────────────────────────────
    // Resolves all DNS queries to our own IP so the phone's captive-portal
    // detection triggers automatically and the user sees the settings page
    // without having to type an IP address.
    if (g_ap_mode_active) {
        _dns.start(53, "*", WiFi.softAPIP());
        Serial.println(F("[wifi] DNS captive portal started"));
    }

    _server.begin(80);
    Serial.println(F("[wifi] HTTP server ready on port 80"));
}

// ---------------------------------------------------------------------------
void WifiServer::handle() {
    if (g_ap_mode_active) _dns.processNextRequest();
    _server.handleClient();
}