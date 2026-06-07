#pragma once

// ---------------------------------------------------------------------------
// font5x.h / font5x.cpp
// 5-row pixel font for the top strip (clear diffuser).
//
// Bitmask layout per column:
//   bit 0 = row 0 (top row)
//   bit 4 = row 4 (bottom row)
// Columns are ordered left → right within each glyph.
//
// Usage:
//   // Look up a single character (case-insensitive; falls back to space)
//   const Glyph* g = font5x_lookup('A');
//
//   // Build a flat column buffer for a whole string
//   uint8_t buf[512]; int len;
//   font5x_buildBuffer("HELLO", buf, sizeof(buf), &len);
//
//   // Scroll text across the top strip of both LED panels
//   font5x_scroll(ledsDriver, ledsPassenger,
//                 "MADE BY AVERY IZATT",   // text
//                 CRGB(220, 220, 220),     // foreground colour
//                 CRGB(30, 0, 0),          // background for red-diffuser segments
//                 45);                     // ms per column step
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Glyph descriptor
// ---------------------------------------------------------------------------
struct Glyph {
    char    ch;     // ASCII character (upper-case stored; lookup is case-insensitive)
    uint8_t w;      // number of columns (1–6)
    uint8_t c[6];   // column bitmasks (bit 0 = top row, bit 4 = bottom row)
};

// Full font table — defined in font5x.cpp
extern const Glyph FONT_5X[];
extern const int   FONT_5X_COUNT;

// ---------------------------------------------------------------------------
// Look up ch (case-insensitive). Returns the matching Glyph*, or the space
// glyph if ch is not in the table.
// ---------------------------------------------------------------------------
const Glyph* font5x_lookup(char ch);

// ---------------------------------------------------------------------------
// Render text into a flat column-bitmask buffer.
//   colBuf  — caller-supplied output buffer
//   bufMax  — capacity of colBuf in bytes
//   outLen  — receives the number of columns written
// A 1-column gap (0x00) is automatically inserted between characters.
// ---------------------------------------------------------------------------
void font5x_buildBuffer(const char* text,
                        uint8_t*    colBuf,
                        int         bufMax,
                        int*        outLen);

// ---------------------------------------------------------------------------
// Scroll text across SEG_TOP_STRIP on both panels.
//   ledsDriver / ledsPassenger — raw pixel buffers (LEDS_PER_SIDE elements each)
//   text     — null-terminated string to scroll
//   fgColour — pixel colour for lit text columns (true through clear diffuser)
//   bgColour — fill colour for SEG_BOT_STRIP and SEG_MAIN while scrolling
//              (only the red channel passes through those red diffusers)
//   scrollMs — milliseconds per column step
// Calls centralized LED output + delay() — only suitable for setup() / demo context.
// ---------------------------------------------------------------------------
void font5x_scroll(CRGB*       ledsDriver,
                   CRGB*       ledsPassenger,
                   const char* text,
                   CRGB        fgColour,
                   CRGB        bgColour,
                   int         scrollMs);
