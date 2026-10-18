#pragma once

#include <stdint.h>

// ---------------------------------------------------------------------------
// faults.h  —  Diagnostic fault codes broadcast on CAN_ID_FAULT (0x102)
//
// Frame layout (4 bytes, DLC = 4):
//   Byte 0 : fault code   (FAULT_* constant below)
//   Byte 1 : severity     (FAULT_SEV_INFO / WARNING / CRITICAL)
//   Byte 2 : data byte 0  (fault-specific; 0x00 if unused)
//   Byte 3 : data byte 1  (fault-specific; 0x00 if unused)
//
// To listen for faults from a CAN analyser or another ECU:
//   Filter on ID 0x102, DLC >= 1, read byte 0 as the fault code.
// ---------------------------------------------------------------------------

// ── Fault codes ──────────────────────────────────────────────────────────────
static constexpr uint8_t FAULT_NONE             = 0x00;

// Thermal — data0 = die temperature in °C
static constexpr uint8_t FAULT_THERMAL_WARN     = 0x01;  // temp >= TEMP_DERATE_START_C
static constexpr uint8_t FAULT_THERMAL_CRITICAL = 0x02;  // temp >= TEMP_SHUTDOWN_C

// CAN bus
static constexpr uint8_t FAULT_CAN_BUS_OFF      = 0x03;  // MCP2515 TXBO — Serial only (bus is dead)

// Input signals active at power-on (possible wiring short or pre-existing condition)
// data0 = driver-side    stuck pin mask  (bit0=brake bit1=running bit2=turn bit3=reverse)
// data1 = passenger-side stuck pin mask (same bit order)
static constexpr uint8_t FAULT_INPUT_STUCK_BOOT = 0x04;

// Abnormal resets detected in NVS on this boot — data0 = accumulated count (low byte)
static constexpr uint8_t FAULT_WDT_RESET        = 0x05;  // watchdog reset on previous boot
static constexpr uint8_t FAULT_PANIC_RESET      = 0x06;  // panic / exception reset
static constexpr uint8_t FAULT_BROWNOUT_RESET   = 0x07;  // brownout / power-loss reset

// Each start, for the dash's record (severity INFO): sent when CAN comes up and again
// when the dash is first heard, since the dash starts later than the lights.
//   data0 = ESP-IDF reset reason (esp_reset_reason_t: 1 power-on, 3 software, 4 panic,
//           5/6/7 watchdogs, 9 brownout, 14 power glitch, 15 CPU lock-up)
//   data1 = BOOT_INFO_* flags
static constexpr uint8_t FAULT_BOOT_INFO        = 0x08;
static constexpr uint8_t BOOT_INFO_SETTLED_POWER_ON = 0x01;  // This start followed the restart after a power-on
static constexpr uint8_t BOOT_INFO_SETTLED_BROWNOUT = 0x02;  // ... after a brownout
static constexpr uint8_t BOOT_INFO_SAFE_MODE        = 0x04;  // Reduced LED current (fault restarts in a row)
static constexpr uint8_t BOOT_INFO_ON_TRIAL         = 0x08;  // New firmware, not yet confirmed by the dash
static constexpr uint8_t BOOT_INFO_SAFETY_INPUT     = 0x10;  // Brake or reverse was on: the settle restart was skipped

// ── Severity levels ──────────────────────────────────────────────────────────
static constexpr uint8_t FAULT_SEV_INFO     = 0x00;
static constexpr uint8_t FAULT_SEV_WARNING  = 0x01;
static constexpr uint8_t FAULT_SEV_CRITICAL = 0x02;
