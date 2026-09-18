// ---------------------------------------------------------------------------
// taillight.cpp
// ---------------------------------------------------------------------------

#include "taillight.h"

void TailLight::begin() {
    fill(CRGB::Black);
    _currentState = LightState::OFF;
    _currentAnim  = AnimationRegistry::get(LightState::OFF, _isDriver);
    if (_currentAnim) _currentAnim->begin(*this, _currentState);
}

void TailLight::update(LightState state, unsigned long nowMs) {
    // Re-query the registry every call so live settings changes (e.g. changing
    // show_anim while already in SHOW state, or changing brake_anim while
    // braking) take effect immediately without needing a state transition.
    Animation* next = AnimationRegistry::get(state, _isDriver);

    if (state != _currentState || next != _currentAnim) {
        if (_currentAnim) _currentAnim->end(*this);

        _currentState = state;
        _currentAnim  = next;

        if (_currentAnim) _currentAnim->begin(*this, _currentState);
        // begin() records millis(). The caller's frame timestamp may predate
        // it after HTTP/CAN work; subtracting that older time would underflow.
        nowMs = millis();
    }

    if (_currentAnim) {
        _currentAnim->update(*this, _currentState, nowMs);
    }
}

void IRAM_ATTR TailLight::fill(CRGB colour) {
    // Route through fillSegment so the diffuser filter is applied per segment
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        fillSegment(seg, colour);
    }
}

void IRAM_ATTR TailLight::fillSegment(int segment, CRGB colour) {
    if (segment < 0 || segment >= NUM_SEGMENTS) return;
    CRGB filtered = applySegDiffuser(segment, colour);
    int count  = segmentSize(segment);
    int offset = SEG_OFFSET[segment];
    for (int i = 0; i < count; i++) {
        _pixels[offset + i] = filtered;
    }
}

int TailLight::segmentSize(int seg) const {
    return (seg == SEG_MAIN) ? MAIN_LEDS : STRIP_LEDS;
}

void TailLight::setPixel(int segment, int row, int col, CRGB colour) {
    if (segment < 0 || segment >= NUM_SEGMENTS) return;
    if (segment == SEG_MAIN) {
        if (row < 0 || row >= MAIN_ROWS)  return;
        if (col < 0 || col >= MAIN_COLS)  return;
    } else {
        if (row < 0 || row >= STRIP_ROWS) return;
        if (col < 0 || col >= STRIP_COLS) return;
    }
    _pixels[_index(segment, row, col)] = applySegDiffuser(segment, colour);
}

int TailLight::_index(int segment, int row, int col) const {
    int base    = SEG_OFFSET[segment];
    int segCols = (segment == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;

    // Mirror the column for the passenger side so that col 0 is always the
    // outermost edge (farthest from the car's centre-line) on both sides.
    // Animations can write col 0→N as "outermost→innermost" without any
    // per-side direction logic.
    if (!_isDriver) col = (segCols - 1) - col;

    if (segment == SEG_TOP_STRIP || segment == SEG_BOT_STRIP) {
        // ── Long strips: 21 cols × 5 rows ─────────────────────────────────
        // Row-major serpentine; row 0 = top.
        // Even rows: left → right.  Odd rows: right → left.
        int idx = (row % 2 == 0)
                  ? row * STRIP_COLS + col
                  : row * STRIP_COLS + (STRIP_COLS - 1 - col);
        return base + idx;

    } else {
        // ── Main panel: 17 cols × 10 rows ─────────────────────────────────
        // Data-in at bottom-left (physical row 9), data-out at top-left.
        // r counts rows from the bottom: r=0 → physical row 9, r=9 → physical row 0.
        // Even r: left → right.  Odd r: right → left.
        int r   = (MAIN_ROWS - 1) - row;
        int idx = (r % 2 == 0)
                  ? r * MAIN_COLS + col
                  : r * MAIN_COLS + (MAIN_COLS - 1 - col);
        return base + idx;
    }
}
