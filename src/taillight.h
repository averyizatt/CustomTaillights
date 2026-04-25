#pragma once

// ---------------------------------------------------------------------------
// taillight.h
// Represents one physical taillight (two 21×5 strips + one 17×10 main panel),
// totalling 380 LEDs per side.
// Owns the pixel buffer and exposes helpers used by animations.
// ---------------------------------------------------------------------------

#include <FastLED.h>
#include "config.h"
#include "states.h"
#include "animations.h"

class TailLight {
public:
    // `pixels`  — pointer to the CRGB array owned by main.cpp (380 elements)
    // `isDriver`  — true for the driver-side (left) taillight
    TailLight(CRGB* pixels, bool isDriver)
        : _pixels(pixels), _isDriver(isDriver) {}

    // Call once in setup() after FastLED.addLeds() has been called
    void begin();

    // Call every loop iteration with the current system state and timestamp.
    void update(LightState state, unsigned long nowMs);

    // ── Helpers used by Animation subclasses ────────────────────────────────
    bool isDriver() const { return _isDriver; }

    // Fill the entire taillight (all 380 pixels) with one colour
    void fill(CRGB colour);

    // Fill one segment with one colour.
    //   segment: SEG_TOP_STRIP, SEG_BOT_STRIP, or SEG_MAIN
    void fillSegment(int segment, CRGB colour);

    // Set a single pixel by segment + (row, col) within that segment.
    //   segment : SEG_TOP_STRIP, SEG_BOT_STRIP, or SEG_MAIN
    //   row     : 0 = top row of the segment
    //   col     : 0 = leftmost column of the segment
    // Handles the different serpentine wiring of each segment automatically.
    void setPixel(int segment, int row, int col, CRGB colour);

    // Raw index accessor for animations that walk the array directly
    CRGB& operator[](int index) { return _pixels[index]; }

    int numPixels()          const { return LEDS_PER_SIDE; }
    int segmentSize(int seg) const;

private:
    CRGB*      _pixels;
    bool       _isDriver;

    LightState  _currentState = LightState::OFF;
    Animation*  _currentAnim  = nullptr;

    // Translate segment + (row, col) to an absolute linear LED index.
    //
    // SEG_TOP_STRIP / SEG_BOT_STRIP (21 cols × 5 rows):
    //   Row-major serpentine; row 0 is the top row.
    //   Even rows: left → right.   Odd rows: right → left.
    //   First pixel: (row 0, col  0) = top-left.
    //   Last pixel : (row 4, col 20) = bottom-right.
    //
    // SEG_MAIN (17 cols × 10 rows):
    //   Row-major serpentine starting at the BOTTOM row (row 9).
    //   r = (MAIN_ROWS - 1) - row  (0 at bottom, 9 at top)
    //   Even r: left → right.   Odd r: right → left.
    //   First pixel: (row 9, col  0) = bottom-left.
    //   Last pixel : (row 0, col  0) = top-left  (r=9 is odd → R→L → exits col 0).
    int _index(int segment, int row, int col) const;
};
