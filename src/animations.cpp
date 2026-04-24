// ---------------------------------------------------------------------------
// animations.cpp
// Concrete animation implementations.
// ---------------------------------------------------------------------------

#include "animations.h"
#include "taillight.h"
#include "config.h"

// ── Static instance definitions ─────────────────────────────────────────────
AnimOff        AnimationRegistry::_off;
AnimRunning    AnimationRegistry::_running;
AnimBrake      AnimationRegistry::_brake;
AnimTurnSignal AnimationRegistry::_turnLeft;
AnimTurnSignal AnimationRegistry::_turnRight;
AnimReverse    AnimationRegistry::_reverse;
AnimHazard     AnimationRegistry::_hazard;

// ---------------------------------------------------------------------------
void AnimationRegistry::init() {
    // Nothing to allocate dynamically for the built-ins.
    // Extend here when adding animations that need heap allocation.
}

Animation* AnimationRegistry::get(LightState state, bool isLeft) {
    switch (state) {
        case LightState::RUNNING:    return &_running;
        case LightState::BRAKE:      return &_brake;
        case LightState::TURN:
        case LightState::BRAKE_TURN: return isLeft ? static_cast<Animation*>(&_turnLeft) : static_cast<Animation*>(&_turnRight);
        case LightState::REVERSE:    return &_reverse;
        case LightState::HAZARD:     return &_hazard;
        case LightState::OFF:
        default:                     return &_off;
    }
}

// ── AnimOff ──────────────────────────────────────────────────────────────────
void AnimOff::update(TailLight& side, LightState /*state*/, unsigned long /*nowMs*/) {
    side.fill(CRGB::Black);
}

// ── AnimRunning ──────────────────────────────────────────────────────────────
void AnimRunning::update(TailLight& side, LightState /*state*/, unsigned long /*nowMs*/) {
    // Dim red parking-light glow
    side.fill(CRGB(BRIGHTNESS_DIM, 0, 0));
}

// ── AnimBrake ────────────────────────────────────────────────────────────────
void AnimBrake::update(TailLight& side, LightState /*state*/, unsigned long /*nowMs*/) {
    side.fill(CRGB(255, 0, 0));
}

// ── AnimTurnSignal ───────────────────────────────────────────────────────────
// Sequential column sweep across all three segments simultaneously.
// Each cycle sweeps columns 0→(STRIP_COLS-1) inward then blanks and repeats.
// The left side sweeps left→right (outward→inward); right side is mirrored.
// MAIN panel columns are mapped proportionally so the sweep finishes together.

void AnimTurnSignal::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
    _step    = 0;
}

void AnimTurnSignal::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long elapsed    = nowMs - _startMs;
    unsigned long halfPeriod = TURN_BLINK_PERIOD_MS / 2;
    unsigned long phase      = elapsed % TURN_BLINK_PERIOD_MS;

    if (phase >= halfPeriod) {
        // Blank / off half
        side.fill(CRGB::Black);
        _step = 0;
        return;
    }

    // _step advances from 0 to (STRIP_COLS - 1) across the sweep half-period.
    // STRIP_COLS is the reference width (21); MAIN_PANEL is narrower (17) and
    // its columns are scaled so the sweep still completes at the same time.
    _step = static_cast<int>((phase * STRIP_COLS) / halfPeriod);
    _step = constrain(_step, 0, STRIP_COLS - 1);

    side.fill(CRGB::Black);

    bool sweepForward = side.isLeft();  // left panel: col 0 → col 20 (outward→inward)

    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;

        // Scale _step to this segment's column count
        int colLimit = (_step * segCols) / STRIP_COLS;
        colLimit = constrain(colLimit, 0, segCols - 1);

        for (int col = 0; col <= colLimit; col++) {
            int c = sweepForward ? col : (segCols - 1 - col);
            for (int row = 0; row < segRows; row++) {
                side.setPixel(seg, row, c, CRGB(255, 100, 0));  // amber
            }
        }
    }
}

void AnimTurnSignal::end(TailLight& side) {
    side.fill(CRGB::Black);
}

// ── AnimReverse ──────────────────────────────────────────────────────────────
void AnimReverse::update(TailLight& side, LightState /*state*/, unsigned long /*nowMs*/) {
    side.fill(CRGB(255, 255, 255));
}

// ── AnimHazard ───────────────────────────────────────────────────────────────
void AnimHazard::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimHazard::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long phase = (nowMs - _startMs) % TURN_BLINK_PERIOD_MS;
    bool on = phase < (TURN_BLINK_PERIOD_MS / 2);
    side.fill(on ? CRGB(255, 100, 0) : CRGB::Black);
}
