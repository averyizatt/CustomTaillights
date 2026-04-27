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
AnimTurnSignal AnimationRegistry::_turnDriver;
AnimTurnSignal AnimationRegistry::_turnPassenger;
AnimReverse    AnimationRegistry::_reverse;
AnimHazard     AnimationRegistry::_hazard;
AnimScrollText AnimationRegistry::_scrollText;
AnimFlash      AnimationRegistry::_flash;
AnimationRegistry::CustomSlot AnimationRegistry::_customSlot = AnimationRegistry::CustomSlot::NONE;

// ---------------------------------------------------------------------------
void AnimationRegistry::init() {
    // Nothing to allocate dynamically for the built-ins.
    // Extend here when adding animations that need heap allocation.
}

Animation* AnimationRegistry::get(LightState state, bool isDriver) {
    switch (state) {
        case LightState::RUNNING:    return &_running;
        case LightState::BRAKE:      return &_brake;
        case LightState::TURN:
        case LightState::BRAKE_TURN: return isDriver ? static_cast<Animation*>(&_turnDriver) : static_cast<Animation*>(&_turnPassenger);
        case LightState::REVERSE:    return &_reverse;
        case LightState::HAZARD:     return &_hazard;
        case LightState::CUSTOM:
            switch (_customSlot) {
                case CustomSlot::SCROLL_TEXT: return &_scrollText;
                case CustomSlot::FLASH:       return &_flash;
                default:                      return &_off;
            }
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

    // col 0 = outermost edge on both sides; TailLight::_index() mirrors the
    // passenger panel automatically so no per-side direction logic is needed.
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;

        // Scale _step to this segment's column count
        int colLimit = (_step * segCols) / STRIP_COLS;
        colLimit = constrain(colLimit, 0, segCols - 1);

        for (int col = 0; col <= colLimit; col++) {
            for (int row = 0; row < segRows; row++) {
                side.setPixel(seg, row, col, CRGB(255, 100, 0));  // amber
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

// ── AnimScrollText ────────────────────────────────────────────────────────────
void AnimScrollText::set(const char* text, CRGB fgColour, CRGB bgColour, int scrollMs) {
    strncpy(_text, text, sizeof(_text) - 1);
    _text[sizeof(_text) - 1] = '\0';
    _fg       = fgColour;
    _bg       = bgColour;
    _scrollMs = scrollMs;
    _done     = false;
}

void AnimScrollText::begin(TailLight& /*side*/, LightState /*state*/) {
    font5x_buildBuffer(_text, _colBuf, (int)sizeof(_colBuf), &_textCols);
    _offset    = -STRIP_COLS;  // start fully off the right edge
    _lastStepMs = millis();
    _done       = false;
}

void AnimScrollText::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    if (_done) { side.fill(CRGB::Black); return; }

    // Advance one column per _scrollMs
    if (nowMs - _lastStepMs >= (unsigned long)_scrollMs) {
        _lastStepMs = nowMs;
        _offset++;
        if (_offset >= _textCols) { _done = true; return; }
    }

    // ── Top strip — clear diffuser, render text glyphs ───────────────────────
    for (int col = 0; col < STRIP_COLS; col++) {
        int     srcCol = _offset + col;
        uint8_t bits   = (srcCol >= 0 && srcCol < _textCols) ? _colBuf[srcCol] : 0;
        for (int row = 0; row < STRIP_ROWS; row++) {
            CRGB px = (bits & (1 << row)) ? _fg : CRGB::Black;
            side.setPixel(SEG_TOP_STRIP, row, col, px);
        }
    }
    // ── Bottom strip + main panel — background glow through red diffusers ────
    side.fillSegment(SEG_BOT_STRIP, _bg);
    side.fillSegment(SEG_MAIN,      _bg);
}

void AnimScrollText::end(TailLight& side) {
    side.fill(CRGB::Black);
}

// ── AnimFlash ─────────────────────────────────────────────────────────────────
void AnimFlash::set(uint8_t flashCount, uint16_t halfPeriodMs, CRGB colour) {
    _flashCount   = (flashCount   == 0) ? 3   : flashCount;
    _halfPeriodMs = (halfPeriodMs == 0) ? 150 : halfPeriodMs;
    _colour       = colour;
    _done         = false;
}

void AnimFlash::begin(TailLight& /*side*/, LightState /*state*/) {
    _startMs = millis();
    _done    = false;
}

void AnimFlash::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    if (_done) { side.fill(CRGB::Black); return; }

    unsigned long elapsed      = nowMs - _startMs;
    unsigned long fullCycle    = (unsigned long)_halfPeriodMs * 2;
    unsigned long totalDuration = fullCycle * _flashCount;

    if (elapsed >= totalDuration) {
        _done = true;
        side.fill(CRGB::Black);
        return;
    }

    bool on = (elapsed % fullCycle) < _halfPeriodMs;
    side.fill(on ? _colour : CRGB::Black);
}

void AnimFlash::end(TailLight& side) {
    side.fill(CRGB::Black);
}
