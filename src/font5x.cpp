// ---------------------------------------------------------------------------
// font5x.cpp
// Full A–Z, 0–9, and punctuation glyph table + rendering helpers.
//
// Bitmask layout: bit 0 = row 0 (top), bit 4 = row 4 (bottom).
// Columns listed left → right.  All characters are stored upper-case;
// font5x_lookup() is case-insensitive.
//
// Visual key used in comments below:
//   X = LED on   . = LED off
//   Rows top → bottom, columns left → right.
// ---------------------------------------------------------------------------

#include "font5x.h"

// ===========================================================================
// Glyph table
// ===========================================================================
const Glyph FONT_5X[] = {

    // ── Letters ──────────────────────────────────────────────────────────────
    // A        B        C        D        E
    // .XX.     XXX.     .XXX     XXX.     XXXX
    // X..X     X..X     X...     X..X     X...
    // XXXX     XXX.     X...     X..X     XXX.
    // X..X     X..X     X...     X..X     X...
    // X..X     XXX.     .XXX     XXX.     XXXX
    {'A', 4, {0x1E, 0x05, 0x05, 0x1E}},
    {'B', 4, {0x1F, 0x15, 0x15, 0x0A}},
    {'C', 4, {0x0E, 0x11, 0x11, 0x11}},
    {'D', 4, {0x1F, 0x11, 0x11, 0x0E}},
    {'E', 4, {0x1F, 0x15, 0x15, 0x11}},

    // F        G        H        I        J
    // XXXX     .XXX     X..X     XXX      XXXX
    // X...     X...     X..X     .X.      ...X
    // XXX.     X.XX     XXXX     .X.      ...X
    // X...     X..X     X..X     .X.      X..X
    // X...     .XXX     X..X     XXX      .XX.
    {'F', 4, {0x1F, 0x05, 0x05, 0x01}},
    {'G', 4, {0x0E, 0x11, 0x15, 0x1D}},
    {'H', 4, {0x1F, 0x04, 0x04, 0x1F}},
    {'I', 3, {0x11, 0x1F, 0x11}},
    {'J', 4, {0x09, 0x11, 0x11, 0x0F}},

    // K        L        M          N        O
    // X..X     X...     X...X      X..X     .XX.
    // X.X.     X...     XX.XX      XX.X     X..X
    // XX..     X...     X.X.X      X.XX     X..X
    // X.X.     X...     X...X      X..X     X..X
    // X..X     XXXX     X...X      X..X     .XX.
    {'K', 4, {0x1F, 0x04, 0x0A, 0x11}},
    {'L', 4, {0x1F, 0x10, 0x10, 0x10}},
    {'M', 5, {0x1F, 0x02, 0x04, 0x02, 0x1F}},
    {'N', 4, {0x1F, 0x02, 0x04, 0x1F}},
    {'O', 4, {0x0E, 0x11, 0x11, 0x0E}},

    // P        Q        R        S        T
    // XXX.     .XX.     XXX.     .XXX     XXX
    // X..X     X..X     X..X     X...     .X.
    // XXX.     X..X     XXX.     .XX.     .X.
    // X...     X.X.     X.X.     ...X     .X.
    // X...     .X.X     X..X     XXX.     .X.
    {'P', 4, {0x1F, 0x05, 0x05, 0x02}},
    {'Q', 4, {0x0E, 0x11, 0x09, 0x16}},
    {'R', 4, {0x1F, 0x05, 0x0D, 0x12}},
    {'S', 4, {0x12, 0x15, 0x15, 0x09}},
    {'T', 3, {0x01, 0x1F, 0x01}},

    // U        V          W          X        Y
    // X..X     X...X      X...X      X..X     X..X
    // X..X     X...X      X...X      X..X     X..X
    // X..X     X...X      X.X.X      .XX.     .XX.
    // X..X     .X.X.      XX.XX      X..X     .XX.
    // .XX.     ..X..      X...X      X..X     .XX.
    {'U', 4, {0x0F, 0x10, 0x10, 0x0F}},
    {'V', 5, {0x07, 0x08, 0x10, 0x08, 0x07}},
    {'W', 5, {0x1F, 0x08, 0x04, 0x08, 0x1F}},
    {'X', 4, {0x1B, 0x04, 0x04, 0x1B}},
    {'Y', 4, {0x03, 0x1C, 0x1C, 0x03}},

    // Z
    // XXXX
    // ...X
    // .XX.
    // X...
    // XXXX
    {'Z', 4, {0x19, 0x15, 0x15, 0x13}},

    // ── Digits ───────────────────────────────────────────────────────────────
    // 0        1        2        3        4
    // .XX.     .X.      .XX.     XXX.     X..X
    // X..X     XX.      X..X     ...X     X..X
    // X..X     .X.      ..X.     .XX.     XXXX
    // X..X     .X.      .X..     ...X     ...X
    // .XX.     XXX      XXXX     XXX.     ...X
    {'0', 4, {0x0E, 0x11, 0x11, 0x0E}},
    {'1', 3, {0x12, 0x1F, 0x10}},
    {'2', 4, {0x12, 0x19, 0x15, 0x12}},
    {'3', 4, {0x11, 0x15, 0x15, 0x0A}},
    {'4', 4, {0x07, 0x04, 0x04, 0x1F}},

    // 5        6        7        8        9
    // XXXX     .XXX     XXXX     .XX.     .XX.
    // X...     X...     ...X     X..X     X..X
    // XXX.     XXX.     ..X.     .XX.     .XXX
    // ...X     X..X     .X..     X..X     ...X
    // XXX.     .XX.     X...     .XX.     .XX.
    {'5', 4, {0x17, 0x15, 0x15, 0x09}},
    {'6', 4, {0x0E, 0x15, 0x15, 0x09}},
    {'7', 4, {0x11, 0x09, 0x05, 0x03}},
    {'8', 4, {0x0A, 0x15, 0x15, 0x0A}},
    {'9', 4, {0x02, 0x15, 0x15, 0x0E}},

    // ── Punctuation ──────────────────────────────────────────────────────────
    {' ', 3, {0x00, 0x00, 0x00}},   // space (3-col gap)
    {'!', 1, {0x17}},               // X / X / X / . / X
    {'?', 4, {0x02, 0x01, 0x15, 0x02}},
    {'.', 2, {0x10, 0x10}},         // bottom-row dot
    {'-', 3, {0x04, 0x04, 0x04}},   // mid-row dash
    {'/', 3, {0x18, 0x04, 0x03}},   // forward slash
    {':', 1, {0x0A}},               // colon (bits 1,3 — two dots)
};

const int FONT_5X_COUNT = (int)(sizeof(FONT_5X) / sizeof(FONT_5X[0]));

// ---------------------------------------------------------------------------
const Glyph* font5x_lookup(char ch) {
    char up = (char)toupper((unsigned char)ch);
    for (int i = 0; i < FONT_5X_COUNT; i++) {
        if (FONT_5X[i].ch == up) return &FONT_5X[i];
    }
    // Fall back to space
    for (int i = 0; i < FONT_5X_COUNT; i++) {
        if (FONT_5X[i].ch == ' ') return &FONT_5X[i];
    }
    return &FONT_5X[0];
}

// ---------------------------------------------------------------------------
void font5x_buildBuffer(const char* text,
                        uint8_t*    colBuf,
                        int         bufMax,
                        int*        outLen) {
    int len = 0;
    for (int i = 0; text[i] != '\0' && len < bufMax - 7; i++) {
        const Glyph* g = font5x_lookup(text[i]);
        for (int c = 0; c < g->w && len < bufMax - 1; c++) {
            colBuf[len++] = g->c[c];
        }
        colBuf[len++] = 0x00;  // 1-column inter-character gap
    }
    *outLen = len;
}

// ---------------------------------------------------------------------------
void font5x_scroll(CRGB*       ledsDriver,
                   CRGB*       ledsPassenger,
                   const char* text,
                   CRGB        fgColour,
                   CRGB        bgColour,
                   int         scrollMs) {
    static uint8_t colBuf[512];
    int textCols = 0;
    font5x_buildBuffer(text, colBuf, (int)sizeof(colBuf), &textCols);

    // Scroll: text enters from the right edge and exits left
    for (int offset = -STRIP_COLS; offset < textCols; offset++) {

        // ── Top strip (clear diffuser) — render the text glyphs ─────────────
        for (int col = 0; col < STRIP_COLS; col++) {
            int     srcCol = offset + col;
            uint8_t bits   = (srcCol >= 0 && srcCol < textCols) ? colBuf[srcCol] : 0;

            for (int row = 0; row < STRIP_ROWS; row++) {
                CRGB px = (bits & (1 << row)) ? fgColour : CRGB::Black;

                // Replicate the serpentine wiring of SEG_TOP_STRIP
                int ledIdx = SEG_OFFSET[SEG_TOP_STRIP] +
                             ((row % 2 == 0) ? row * STRIP_COLS + col
                                             : row * STRIP_COLS + (STRIP_COLS - 1 - col));
                ledsDriver[ledIdx]  = px;
                ledsPassenger[ledIdx] = px;
            }
        }

        // ── Bottom strip + main panel (red diffusers) — background glow ─────
        fill_solid(ledsDriver  + SEG_OFFSET[SEG_BOT_STRIP], STRIP_LEDS, bgColour);
        fill_solid(ledsPassenger + SEG_OFFSET[SEG_BOT_STRIP], STRIP_LEDS, bgColour);
        fill_solid(ledsDriver  + SEG_OFFSET[SEG_MAIN],      MAIN_LEDS,  bgColour);
        fill_solid(ledsPassenger + SEG_OFFSET[SEG_MAIN],      MAIN_LEDS,  bgColour);

        FastLED.show();
        delay(scrollMs);
    }

    FastLED.clear(true);
}
