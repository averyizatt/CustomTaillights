#pragma once

#include <stdint.h>
#include <can_contract/can_protocol.h>

// ===========================================================================
// can_protocol.h  —  Foxbody Taillight Controller CAN Bus Protocol Reference
// ===========================================================================
//
// Wire layout follows include/can_contract/can_protocol.h from CCM unchanged.
// This file documents the taillight behavior and custom animation IDs.
// All frame IDs are 11-bit standard frames at 500 kbit/s.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  ID    Dir   Description                                                 │
// │  0x100  TX   Periodic state broadcast (every 100 ms)                    │
// │  0x101  RX   Command frame (addressed to taillight ECU)                 │
// │  0x102  TX   Diagnostic fault broadcast (on-demand)                     │
// └─────────────────────────────────────────────────────────────────────────┘
//
// ===========================================================================
// 0x100 — STATE BROADCAST  (DLC = 7, TX every 100 ms)
// ===========================================================================
//
//  Byte  Field               Values / Notes
//  ────  ─────────────────── ──────────────────────────────────────────────
//    0   left  LightState    0=OFF 1=RUNNING 2=BRAKE 3=TURN 4=REVERSE
//                            5=BRAKE_TURN 6=HAZARD 7=CUSTOM
//    1   right LightState    same encoding
//    2   driver inputs      brake/running/turn/reverse bits
//    3   passenger inputs   brake/running/turn/reverse bits
//    4   brightness         applied global brightness (0-255)
//    5   die temperature    raw Celsius, clamped to 0-255
//    6   thermal derate     raw amount (0-255)
//
// ===========================================================================
// 0x101 — COMMAND FRAME  (DLC varies, RX)
// ===========================================================================
//
//  Byte 0 = command code.  Remaining bytes are command-specific.
//
//  ── Cmd 0x01 : Set Brightness ──────────────────────────────────────────────
//  DLC = 2
//  Persists until another brightness command or restart; always thermally limited.
//  Byte 1 : target brightness  (0–255)
//
//  Example (set to 50 % brightness):
//    ID=0x101  DLC=2  Data: 01 80
//
//  ── Cmd 0x02 : Animation Override ─────────────────────────────────────────
//  Forces both sides to play a specific LightState animation, ignoring real
//  inputs.  Stays active until Cmd 0x03 clears it.
//  DLC = 3
//  Byte 1 : driver    LightState (0–7)   — US left
//  Byte 2 : passenger LightState (0–7)   — US right
//
//  Example (force both sides to BRAKE):
//    ID=0x101  DLC=3  Data: 02 02 02
//
//  ── Cmd 0x03 : Clear Override ─────────────────────────────────────────────
//  Returns both sides to normal input-driven operation.
//  DLC = 1
//
//  Example:
//    ID=0x101  DLC=1  Data: 03
//
//  ── Cmd 0x04 : Custom Animation ───────────────────────────────────────────
//  Triggers a named custom animation on both sides for a set duration.
//  The animation plays out, then the ECU automatically returns to normal
//  operation (no Cmd 0x03 needed).
//  DLC = 6
//  Byte 1 : animation ID  (CANIM_* constant below)
//  Byte 2 : duration high byte  (duration_ms >> 8)
//  Byte 3 : duration low  byte  (duration_ms & 0xFF)
//            — set to 0x00 0x00 to use the animation's built-in default
//  Byte 4 : param0  (animation-specific; 0x00 if unused)
//  Byte 5 : param1  (animation-specific; 0x00 if unused)
//
//  Example (scroll "BRAKE CHECK" text for default duration):
//    ID=0x101  DLC=6  Data: 04 01 00 00 00 00
//
//  Example (flash amber 3 times, 300 ms per flash):
//    ID=0x101  DLC=6  Data: 04 02 00 00 03 2C
//
// ===========================================================================
// 0x102 — FAULT BROADCAST  (DLC = 4, TX on-demand)
// ===========================================================================
//
//  Byte  Field       Values / Notes
//  ────  ─────────── ──────────────────────────────────────────────────────
//    0   fault code  FAULT_* constants (see faults.h)
//    1   severity    0=INFO  1=WARNING  2=CRITICAL
//    2   data byte 0 fault-specific (0 if unused)
//    3   data byte 1 fault-specific (0 if unused)
//
//  Fault codes:
//    0x01  FAULT_THERMAL_WARN      data0 = temp °C
//    0x02  FAULT_THERMAL_CRITICAL  data0 = temp °C
//    0x03  FAULT_CAN_BUS_OFF       (Serial only — bus is dead)
//    0x04  FAULT_INPUT_STUCK_BOOT  data0 = driver stuck mask, data1 = passenger
//    0x05  FAULT_WDT_RESET         data0 = accumulated count
//    0x06  FAULT_PANIC_RESET       data0 = accumulated count
//    0x07  FAULT_BROWNOUT_RESET    data0 = accumulated count
//
// ===========================================================================
// Cmd 0x05: mode selection, DLC 3: [05 mode option].
// Mode 0=stock, 1=sequential, 2=show, 3=demo; option 0..32.
// Modes change live settings only; physical brake/reverse retain priority.
// Cmd 0x03 also stops custom/show/demo output.
// Custom Animation IDs  (Byte 1 of Cmd 0x04)
// ===========================================================================

// Scroll "BRAKE CHECK" across the top strip (white text, dim-red background).
// param0 / param1 unused.
static constexpr uint8_t CANIM_SCROLL_BRAKE_CHECK = 0x01;

// Rapid colour flash across all segments.
// param0 = number of flashes (0 → default 3)
// param1 = ms per half-flash, i.e. on-time  (0 → default 150 ms)
// Colour is amber (255, 140, 0) through the red diffusers → red; clear → amber.
static constexpr uint8_t CANIM_FLASH_AMBER        = 0x02;

// Scroll arbitrary short text — text is encoded in param0 + param1 as two
// ASCII characters (e.g. param0='H', param1='I' scrolls "HI").
// For longer messages use CANIM_SCROLL_BRAKE_CHECK or add more IDs below.
static constexpr uint8_t CANIM_SCROLL_2CHAR       = 0x03;

// ---------------------------------------------------------------------------
// To add a new custom animation:
//   1. Define a new CANIM_* ID constant here.
//   2. Handle it in CANBus::_processCustomAnim() in canbus.cpp.
//   3. Create an AnimCustom subclass or reuse AnimScrollText / AnimFlash.
// ---------------------------------------------------------------------------
