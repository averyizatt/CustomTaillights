// ---------------------------------------------------------------------------
// animations.cpp
// Concrete animation implementations.
// ---------------------------------------------------------------------------

#include "animations.h"
#include "taillight.h"
#include "config.h"
#include "settings.h"

// ── Static instance definitions ─────────────────────────────────────────────
AnimOff        AnimationRegistry::_off;
AnimRunning    AnimationRegistry::_running;
AnimRunBreathe AnimationRegistry::_runBreathe;
AnimBrake      AnimationRegistry::_brake;
AnimBrakePulse     AnimationRegistry::_brakePulse;
AnimBrakeCenterOut AnimationRegistry::_brakeCenterOutD;
AnimBrakeCenterOut AnimationRegistry::_brakeCenterOutP;
AnimBrakeStrobe    AnimationRegistry::_brakeStrobe;
AnimBrakeOuterIn   AnimationRegistry::_brakeOuterInD;
AnimBrakeOuterIn   AnimationRegistry::_brakeOuterInP;
AnimBrakeHeartbeat AnimationRegistry::_brakeHeartbeat;
AnimTurnSignal AnimationRegistry::_turnDriver;
AnimTurnSignal AnimationRegistry::_turnPassenger;
AnimTurnSimple    AnimationRegistry::_turnSimpleD;
AnimTurnSimple    AnimationRegistry::_turnSimpleP;
AnimTurnGroupChase AnimationRegistry::_turnGroupD;
AnimTurnGroupChase AnimationRegistry::_turnGroupP;
AnimTurnBounce    AnimationRegistry::_turnBounceD;
AnimTurnBounce    AnimationRegistry::_turnBounceP;
AnimTurnSplitOut  AnimationRegistry::_turnSplitD;
AnimTurnSplitOut  AnimationRegistry::_turnSplitP;
AnimTurnFastChase AnimationRegistry::_turnFastD;
AnimTurnFastChase AnimationRegistry::_turnFastP;
AnimReverse    AnimationRegistry::_reverse;
AnimReversePulse   AnimationRegistry::_reversePulse;
AnimReverseSparkle AnimationRegistry::_reverseSparkle;
AnimReverseScanner AnimationRegistry::_reverseScanner;
AnimRunShimmer     AnimationRegistry::_runShimmer;
AnimRunComet       AnimationRegistry::_runComet;
AnimHazard     AnimationRegistry::_hazardD;
AnimHazard     AnimationRegistry::_hazardP;
AnimScrollText AnimationRegistry::_scrollText;
AnimFlash      AnimationRegistry::_flash;
AnimShowRainbow    AnimationRegistry::_showRainbow;
AnimShowChase      AnimationRegistry::_showChase;
AnimShowTheater    AnimationRegistry::_showTheater;
AnimShowFire       AnimationRegistry::_showFireD;
AnimShowFire       AnimationRegistry::_showFireP;
AnimShowMeteor     AnimationRegistry::_showMeteor;
AnimShowPolice     AnimationRegistry::_showPolice;
AnimShowNightRider AnimationRegistry::_showNightRider;
AnimShowColorCycle AnimationRegistry::_showColorCycle;
AnimShowSparkle    AnimationRegistry::_showSparkle;
AnimShowPlasma     AnimationRegistry::_showPlasma;
AnimShowMatrix     AnimationRegistry::_showMatrix;
AnimShowJuggle     AnimationRegistry::_showJuggle;
AnimShowBPM        AnimationRegistry::_showBPM;
AnimShowConfetti   AnimationRegistry::_showConfetti;
AnimShowOcean      AnimationRegistry::_showOcean;
AnimShowLightning  AnimationRegistry::_showLightning;
AnimShowHeartbeat  AnimationRegistry::_showHeartbeat;
AnimShowRipple     AnimationRegistry::_showRipple;
AnimShowSunrise        AnimationRegistry::_showSunrise;
AnimShowText           AnimationRegistry::_showText;
AnimShowColorwaves     AnimationRegistry::_showColorwaves;
AnimShowTwinkleFox     AnimationRegistry::_showTwinkleFox;
AnimShowBouncingBalls  AnimationRegistry::_showBouncingBallsD;
AnimShowBouncingBalls  AnimationRegistry::_showBouncingBallsP;
AnimShowFireworks      AnimationRegistry::_showFireworksD;
AnimShowFireworks      AnimationRegistry::_showFireworksP;
AnimShowDrip           AnimationRegistry::_showDripD;
AnimShowDrip           AnimationRegistry::_showDripP;
AnimShowCylonDual      AnimationRegistry::_showCylonDual;
AnimShowV8             AnimationRegistry::_showV8;
AnimShowDragLaunch     AnimationRegistry::_showDragLaunch;
AnimShowNeon           AnimationRegistry::_showNeon;
AnimShowSpeedStreaks   AnimationRegistry::_showSpeedStreaks;
AnimShowRadar          AnimationRegistry::_showRadar;
AnimShowAurora         AnimationRegistry::_showAurora;
AnimShowGlitch         AnimationRegistry::_showGlitch;
AnimationRegistry::CustomSlot AnimationRegistry::_customSlot = AnimationRegistry::CustomSlot::NONE;

// ---------------------------------------------------------------------------
void AnimationRegistry::init() {
    // Nothing to allocate dynamically for the built-ins.
    // Extend here when adding animations that need heap allocation.
}

Animation* AnimationRegistry::get(LightState state, bool isDriver) {
    switch (state) {
        case LightState::RUNNING:
            switch (g_settings.run_anim) {
                case 1: return &_runBreathe;
                case 2: return &_runShimmer;
                case 3: return &_runComet;
                default: return &_running;
            }

        case LightState::BRAKE:
            switch (g_settings.brake_anim) {
                case 1: return &_brakePulse;
                case 2: return isDriver ? static_cast<Animation*>(&_brakeCenterOutD) : static_cast<Animation*>(&_brakeCenterOutP);
                case 3: return &_brakeStrobe;
                case 4: return isDriver ? static_cast<Animation*>(&_brakeOuterInD) : static_cast<Animation*>(&_brakeOuterInP);
                case 5: return &_brakeHeartbeat;
                default: return &_brake;
            }

        case LightState::TURN:
        case LightState::BRAKE_TURN:
            switch (g_settings.turn_anim) {
                case 1: return isDriver ? static_cast<Animation*>(&_turnSimpleD)  : static_cast<Animation*>(&_turnSimpleP);
                case 2: return isDriver ? static_cast<Animation*>(&_turnGroupD)   : static_cast<Animation*>(&_turnGroupP);
                case 3: return isDriver ? static_cast<Animation*>(&_turnBounceD)  : static_cast<Animation*>(&_turnBounceP);
                case 4: return isDriver ? static_cast<Animation*>(&_turnSplitD)   : static_cast<Animation*>(&_turnSplitP);
                case 5: return isDriver ? static_cast<Animation*>(&_turnFastD)    : static_cast<Animation*>(&_turnFastP);
                default: return isDriver ? static_cast<Animation*>(&_turnDriver)  : static_cast<Animation*>(&_turnPassenger);
            }

        case LightState::REVERSE:
            switch (g_settings.reverse_anim) {
                case 1: return &_reversePulse;
                case 2: return &_reverseSparkle;
                case 3: return &_reverseScanner;
                default: return &_reverse;
            }

        case LightState::HAZARD:    return isDriver ? static_cast<Animation*>(&_hazardD) : static_cast<Animation*>(&_hazardP);

        case LightState::SHOW:
            switch (g_settings.show_anim) {
                case 1:  return &_showChase;
                case 2:  return &_showTheater;
                case 3:  return isDriver ? static_cast<Animation*>(&_showFireD) : static_cast<Animation*>(&_showFireP);
                case 4:  return &_showMeteor;
                case 5:  return &_showPolice;
                case 6:  return &_showNightRider;
                case 7:  return &_showColorCycle;
                case 8:  return &_showSparkle;
                case 9:  return &_showPlasma;
                case 10: return &_showMatrix;
                case 11: return &_showJuggle;
                case 12: return &_showBPM;
                case 13: return &_showConfetti;
                case 14: return &_showOcean;
                case 15: return &_showLightning;
                case 16: return &_showHeartbeat;
                case 17: return &_showRipple;
                case 18: return &_showSunrise;
                case 19: return &_showText;
                case 20: return &_showColorwaves;
                case 21: return &_showTwinkleFox;
                case 22: return isDriver ? static_cast<Animation*>(&_showBouncingBallsD) : static_cast<Animation*>(&_showBouncingBallsP);
                case 23: return isDriver ? static_cast<Animation*>(&_showFireworksD)      : static_cast<Animation*>(&_showFireworksP);
                case 24: return isDriver ? static_cast<Animation*>(&_showDripD)           : static_cast<Animation*>(&_showDripP);
                case 25: return &_showCylonDual;
                case 26: return &_showV8;
                case 27: return &_showDragLaunch;
                case 28: return &_showNeon;
                case 29: return &_showSpeedStreaks;
                case 30: return &_showRadar;
                case 31: return &_showAurora;
                case 32: return &_showGlitch;
                default: return &_showRainbow;
            }

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
    // Scale the user-chosen running color by brightness_dim (5–100 → 5–100%)
    uint8_t dim = g_settings.brightness_dim;
    uint8_t r = (uint8_t)((g_settings.run_r * dim) / 100);
    uint8_t g = (uint8_t)((g_settings.run_g * dim) / 100);
    uint8_t b = (uint8_t)((g_settings.run_b * dim) / 100);
    side.fill(CRGB(r, g, b));
}

// ── AnimBrake ────────────────────────────────────────────────────────────────
void AnimBrake::update(TailLight& side, LightState /*state*/, unsigned long /*nowMs*/) {
    side.fill(CRGB(g_settings.brake_r, g_settings.brake_g, g_settings.brake_b));
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
    unsigned long halfPeriod = g_settings.turn_blink_ms / 2;
    unsigned long phase      = elapsed % g_settings.turn_blink_ms;

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
                side.setPixel(seg, row, col,
                              CRGB(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b));
            }
        }
    }
}

void AnimTurnSignal::end(TailLight& side) {
    side.fill(CRGB::Black);
}

// ── AnimReverse ──────────────────────────────────────────────────────────────
void AnimReverse::update(TailLight& side, LightState /*state*/, unsigned long /*nowMs*/) {
    side.fill(CRGB(g_settings.reverse_r, g_settings.reverse_g, g_settings.reverse_b));
}

// ── AnimHazard ───────────────────────────────────────────────────────────────
void AnimHazard::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimHazard::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long phase = (nowMs - _startMs) % g_settings.turn_blink_ms;
    bool on = phase < (g_settings.turn_blink_ms / 2);
    side.fill(on ? CRGB(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b)
                 : CRGB::Black);
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
    // Write glyph columns in reverse display order so that the physically
    // mirrored wiring on each side produces readable (non-backwards) text
    // when viewed from behind the car.  TailLight::_index() handles the
    // per-side serpentine mapping; reversing the column here corrects the
    // glyph orientation for both driver (data-in top-right) and passenger
    // (data-in top-left, double-inverted back to correct by _index's flip).
    for (int col = 0; col < STRIP_COLS; col++) {
        int     srcCol = _offset + col;
        uint8_t bits   = (srcCol >= 0 && srcCol < _textCols) ? _colBuf[srcCol] : 0;
        for (int row = 0; row < STRIP_ROWS; row++) {
            CRGB px = (bits & (1 << row)) ? _fg : CRGB::Black;
            side.setPixel(SEG_TOP_STRIP, row, STRIP_COLS - 1 - col, px);
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

// ===========================================================================
// Brake variants
// ===========================================================================

// ── AnimBrakePulse ────────────────────────────────────────────────────────────
// Sin-wave breathe, ~1.2 s period.
void AnimBrakePulse::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    // speed factor: 100 % = 1200 ms period
    unsigned long period = (unsigned long)(1200UL * 100 / constrain(g_settings.show_speed, 50, 200));
    uint8_t s = sin8((uint8_t)((nowMs * 256UL) / period));
    side.fill(CRGB(scale8(g_settings.brake_r, s),
                   scale8(g_settings.brake_g, s),
                   scale8(g_settings.brake_b, s)));
}

// ── AnimBrakeCenterOut ────────────────────────────────────────────────────────
// Sweeps from the centre of each segment outward (~400 ms) then holds solid.
void AnimBrakeCenterOut::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
    _done    = false;
}

void AnimBrakeCenterOut::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    CRGB col(g_settings.brake_r, g_settings.brake_g, g_settings.brake_b);
    if (_done) { side.fill(col); return; }

    unsigned long elapsed = nowMs - _startMs;
    if (elapsed >= 400) { _done = true; side.fill(col); return; }

    side.fill(CRGB::Black);
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        int maxStep = segCols / 2;
        int step    = (int)((elapsed * maxStep) / 400);
        if (step > maxStep) step = maxStep;
        int center  = segCols / 2;
        for (int c = center - step; c <= center + step; c++) {
            if (c < 0 || c >= segCols) continue;
            for (int r = 0; r < segRows; r++)
                side.setPixel(seg, r, c, col);
        }
    }
}

// ── AnimBrakeStrobe ───────────────────────────────────────────────────────────
// Rapid 8 Hz strobe (125 ms period, 50 % duty cycle).
void AnimBrakeStrobe::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    bool on = (nowMs % 125UL) < 62UL;
    side.fill(on ? CRGB(g_settings.brake_r, g_settings.brake_g, g_settings.brake_b)
                 : CRGB::Black);
}

// ===========================================================================
// Turn-signal variants
// ===========================================================================

// ── AnimTurnSimple ────────────────────────────────────────────────────────────
// Whole-panel on/off flash at turn_blink_ms period.
void AnimTurnSimple::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimTurnSimple::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long phase = (nowMs - _startMs) % g_settings.turn_blink_ms;
    bool on = phase < (g_settings.turn_blink_ms / 2);
    side.fill(on ? CRGB(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b)
                 : CRGB::Black);
}

// ── AnimTurnGroupChase ────────────────────────────────────────────────────────
// 4-column groups sweep outward and restart at turn_blink_ms period.
void AnimTurnGroupChase::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimTurnGroupChase::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long halfPeriod = g_settings.turn_blink_ms / 2;
    unsigned long phase      = (nowMs - _startMs) % g_settings.turn_blink_ms;

    if (phase >= halfPeriod) {
        side.fill(CRGB::Black);
        return;
    }

    // Advance a 4-column window across the sweep half-period
    static constexpr int GRP = 4;
    int totalSteps = STRIP_COLS + GRP;
    int step = (int)((phase * totalSteps) / halfPeriod);
    step = constrain(step, 0, totalSteps - 1);

    side.fill(CRGB::Black);
    CRGB col(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b);

    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        int scaledStep = (step * segCols) / (STRIP_COLS + GRP);
        int scaledGrp  = constrain((GRP * segCols) / STRIP_COLS, 1, segCols);
        for (int c = scaledStep; c < scaledStep + scaledGrp && c < segCols; c++) {
            for (int r = 0; r < segRows; r++)
                side.setPixel(seg, r, c, col);
        }
    }
}

// ── AnimTurnBounce ────────────────────────────────────────────────────────────
// KITT-style comet bounces across the top strip during the "on" half;
// bottom strip and main fill with the turn colour at reduced brightness.
void AnimTurnBounce::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimTurnBounce::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long halfPeriod = g_settings.turn_blink_ms / 2;
    unsigned long phase      = (nowMs - _startMs) % g_settings.turn_blink_ms;

    if (phase >= halfPeriod) {
        side.fill(CRGB::Black);
        return;
    }

    // Fill bottom segments with dim turn colour
    CRGB dimCol(g_settings.turn_r >> 2, g_settings.turn_g >> 2, g_settings.turn_b >> 2);
    side.fillSegment(SEG_BOT_STRIP, dimCol);
    side.fillSegment(SEG_MAIN,      dimCol);

    // Bounce comet on top strip
    static constexpr int TAIL = 6;
    int totalPos = (STRIP_COLS - 1) * 2; // bounce period
    int pos      = (int)((phase * totalPos) / halfPeriod);
    int col      = (pos < STRIP_COLS) ? pos : totalPos - pos; // triangle wave

    for (int c = 0; c < STRIP_COLS; c++) {
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
    }
    CRGB turnCol(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b);
    for (int t = 0; t < TAIL; t++) {
        int tc = col - t;
        if (tc < 0) break;
        uint8_t bright = (uint8_t)(255 - (t * 255 / TAIL));
        CRGB px(scale8(turnCol.r, bright), scale8(turnCol.g, bright), scale8(turnCol.b, bright));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, tc, px);
    }
}

// ===========================================================================
// Reverse variant
// ===========================================================================

// ── AnimReversePulse ──────────────────────────────────────────────────────────
// Smooth breathing pulse with the reverse colour, ~1.4 s period.
void AnimReversePulse::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t s = sin8((uint8_t)((nowMs * 256UL) / 1400UL));
    side.fill(CRGB(scale8(g_settings.reverse_r, s),
                   scale8(g_settings.reverse_g, s),
                   scale8(g_settings.reverse_b, s)));
}

// ===========================================================================
// Running-light variant
// ===========================================================================

// ── AnimRunBreathe ────────────────────────────────────────────────────────────
// Slow breathing pulse with the running colour, ~3 s period.
void AnimRunBreathe::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t dim = g_settings.brightness_dim;
    uint8_t s   = sin8((uint8_t)((nowMs * 256UL) / 3000UL));
    // Scale s to the range 20–255 so it never fully goes dark
    s = 20 + scale8(s, 235);
    uint8_t r = scale8((uint8_t)((g_settings.run_r * dim) / 100), s);
    uint8_t g = scale8((uint8_t)((g_settings.run_g * dim) / 100), s);
    uint8_t b = scale8((uint8_t)((g_settings.run_b * dim) / 100), s);
    side.fill(CRGB(r, g, b));
}

// ===========================================================================
// Extra brake variants (brake_anim = 4 / 5)
// ===========================================================================

// ── AnimBrakeOuterIn ──────────────────────────────────────────────────────────
// Sweeps from the outer edges of each segment inward to the centre (~400 ms)
// then holds solid.  Mirrors AnimBrakeCenterOut direction.
void AnimBrakeOuterIn::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
    _done    = false;
}

void AnimBrakeOuterIn::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    CRGB col(g_settings.brake_r, g_settings.brake_g, g_settings.brake_b);
    if (_done) { side.fill(col); return; }

    unsigned long elapsed = nowMs - _startMs;
    if (elapsed >= 400UL) { _done = true; side.fill(col); return; }

    side.fill(CRGB::Black);
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        int maxStep = (segCols / 2) + 1;
        int step    = (int)((elapsed * (unsigned long)maxStep) / 400UL);
        step = constrain(step, 0, maxStep);
        for (int c = 0; c < segCols; c++) {
            int distFromEdge = (c <= segCols / 2) ? c : (segCols - 1 - c);
            if (distFromEdge < step) {
                for (int r = 0; r < segRows; r++)
                    side.setPixel(seg, r, c, col);
            }
        }
    }
}

// ── AnimBrakeHeartbeat ────────────────────────────────────────────────────────
// Lub-dub double-pulse cardiac rhythm in the brake colour (~800 ms cycle).
void AnimBrakeHeartbeat::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    CRGB col(g_settings.brake_r, g_settings.brake_g, g_settings.brake_b);
    unsigned long phase = nowMs % 800UL;
    uint8_t bri;
    if (phase < 80UL) {
        bri = (phase < 40UL) ? (uint8_t)((phase * 255UL) / 40UL)
                              : (uint8_t)(((80UL - phase) * 255UL) / 40UL);
    } else if (phase < 120UL) {
        bri = 0u;
    } else if (phase < 200UL) {
        unsigned long p2 = phase - 120UL;
        bri = (p2 < 40UL) ? (uint8_t)((p2 * 200UL) / 40UL)
                           : (uint8_t)(((80UL - p2) * 200UL) / 40UL);
    } else {
        bri = 0u;
    }
    side.fill(CRGB(scale8(col.r, bri), scale8(col.g, bri), scale8(col.b, bri)));
}

// ===========================================================================
// Extra turn-signal variants (turn_anim = 4 / 5)
// ===========================================================================

// ── AnimTurnSplitOut ──────────────────────────────────────────────────────────
// Two simultaneous sweeps start from the centre and race to the outer edges
// during the "on" half; black during the "off" half.
void AnimTurnSplitOut::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimTurnSplitOut::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long halfPeriod = g_settings.turn_blink_ms / 2;
    unsigned long phase      = (nowMs - _startMs) % g_settings.turn_blink_ms;

    if (phase >= halfPeriod) {
        side.fill(CRGB::Black);
        return;
    }

    CRGB col(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b);
    side.fill(CRGB::Black);

    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        int maxStep = segCols / 2 + 1;
        int progress = (int)((phase * (unsigned long)maxStep) / halfPeriod);
        progress = constrain(progress, 0, maxStep);
        int center = segCols / 2;
        for (int c = 0; c < segCols; c++) {
            if (abs(c - center) <= progress) {
                for (int r = 0; r < segRows; r++)
                    side.setPixel(seg, r, c, col);
            }
        }
    }
}

// ── AnimTurnFastChase ─────────────────────────────────────────────────────────
// Three rapid sequential sweeps fill the on-half of each blink period.
void AnimTurnFastChase::begin(TailLight& side, LightState /*state*/) {
    _startMs = millis();
}

void AnimTurnFastChase::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long halfPeriod = g_settings.turn_blink_ms / 2;
    unsigned long phase      = (nowMs - _startMs) % g_settings.turn_blink_ms;

    if (phase >= halfPeriod) {
        side.fill(CRGB::Black);
        return;
    }

    static constexpr int SWEEPS = 3;
    static constexpr int GRP    = 3;
    unsigned long subPeriod = halfPeriod / SWEEPS;
    unsigned long subPhase  = phase % subPeriod;
    int totalSteps = STRIP_COLS + GRP;
    int step = (int)((subPhase * (unsigned long)totalSteps) / subPeriod);
    step = constrain(step, 0, totalSteps - 1);

    side.fill(CRGB::Black);
    CRGB col(g_settings.turn_r, g_settings.turn_g, g_settings.turn_b);

    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        int scaledStep = (step * segCols) / totalSteps;
        int scaledGrp  = constrain((GRP * segCols) / STRIP_COLS, 1, segCols);
        for (int c = scaledStep; c < scaledStep + scaledGrp && c < segCols; c++) {
            for (int r = 0; r < segRows; r++)
                side.setPixel(seg, r, c, col);
        }
    }
}

// ===========================================================================
// Extra reverse variants (reverse_anim = 2 / 3)
// ===========================================================================

// ── AnimReverseSparkle ────────────────────────────────────────────────────────
// Scattered bright pixel bursts in the reverse colour over a dim base fill.
void AnimReverseSparkle::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t = (uint8_t)(nowMs / 30UL);
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        for (int c = 0; c < segCols; c++) {
            for (int r = 0; r < segRows; r++) {
                uint8_t h = sin8((uint8_t)((uint16_t)t * 3u + (uint8_t)((unsigned)c * 17u) + (uint8_t)((unsigned)r * 31u)));
                uint8_t bri = (h > 220u) ? 255u : scale8(h >> 2u, 80u);
                side.setPixel(seg, r, c,
                    CRGB(scale8(g_settings.reverse_r, bri),
                         scale8(g_settings.reverse_g, bri),
                         scale8(g_settings.reverse_b, bri)));
            }
        }
    }
}

// ── AnimReverseScanner ────────────────────────────────────────────────────────
// Slow KITT-style scanner in the reverse colour (~2.4 s period).
void AnimReverseScanner::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int TAIL     = 5;
    static constexpr unsigned long PERIOD = 2400UL;
    int totalPos = (STRIP_COLS - 1) * 2;
    int pos      = (int)((nowMs % PERIOD) * (unsigned long)totalPos / PERIOD);
    int head     = (pos < STRIP_COLS) ? pos : totalPos - pos;

    side.fill(CRGB::Black);
    CRGB revCol(g_settings.reverse_r, g_settings.reverse_g, g_settings.reverse_b);

    for (int t = 0; t < TAIL; t++) {
        int tc = head - t;
        if (tc < 0) tc = -tc;
        if (tc >= STRIP_COLS) continue;
        uint8_t bri = (uint8_t)(255u - (uint8_t)((unsigned)t * 255u / (unsigned)TAIL));
        CRGB px(scale8(revCol.r, bri), scale8(revCol.g, bri), scale8(revCol.b, bri));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, tc, px);
    }
    uint8_t dimR = revCol.r >> 3u;
    uint8_t dimG = revCol.g >> 3u;
    uint8_t dimB = revCol.b >> 3u;
    side.fillSegment(SEG_BOT_STRIP, CRGB(dimR, dimG, dimB));
    side.fillSegment(SEG_MAIN,      CRGB(dimR, dimG, dimB));
}

// ===========================================================================
// Extra running-light variants (run_anim = 2 / 3)
// ===========================================================================

// ── AnimRunShimmer ────────────────────────────────────────────────────────────
// Subtle per-column brightness shimmer (70–100 % of brightness_dim).
void AnimRunShimmer::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t dim = g_settings.brightness_dim;
    uint8_t t   = (uint8_t)(nowMs / 40UL);
    for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
        int segCols = (seg == SEG_MAIN) ? MAIN_COLS : STRIP_COLS;
        int segRows = (seg == SEG_MAIN) ? MAIN_ROWS : STRIP_ROWS;
        for (int c = 0; c < segCols; c++) {
            uint8_t s = sin8((uint8_t)((uint16_t)t * 3u + (uint8_t)((unsigned)c * 29u)));
            uint8_t dimC = (uint8_t)((unsigned)dim * (70u + scale8(s, 30u)) / 100u);
            dimC = (dimC < 1u) ? 1u : dimC;
            CRGB col((uint8_t)((g_settings.run_r * dimC) / 100u),
                     (uint8_t)((g_settings.run_g * dimC) / 100u),
                     (uint8_t)((g_settings.run_b * dimC) / 100u));
            for (int r = 0; r < segRows; r++)
                side.setPixel(seg, r, c, col);
        }
    }
}

// ── AnimRunComet ──────────────────────────────────────────────────────────────
// Very dim, slow comet wanders back and forth on top strip; dim base elsewhere.
void AnimRunComet::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int TAIL            = 8;
    static constexpr unsigned long PERIOD = 4000UL;
    uint8_t dim     = g_settings.brightness_dim;
    int totalPos    = (STRIP_COLS - 1) * 2;
    int pos         = (int)((nowMs % PERIOD) * (unsigned long)totalPos / PERIOD);
    int head        = (pos < STRIP_COLS) ? pos : totalPos - pos;

    uint8_t baseR = (uint8_t)((g_settings.run_r * dim) / 100u);
    uint8_t baseG = (uint8_t)((g_settings.run_g * dim) / 100u);
    uint8_t baseB = (uint8_t)((g_settings.run_b * dim) / 100u);
    side.fill(CRGB(baseR >> 2u, baseG >> 2u, baseB >> 2u));

    for (int t = 0; t < TAIL; t++) {
        int tc = head - t;
        if (tc < 0) tc = -tc;
        if (tc >= STRIP_COLS) continue;
        uint8_t bri = (uint8_t)((unsigned)(TAIL - t) * (unsigned)dim / (unsigned)TAIL);
        bri = (bri > dim) ? dim : bri;
        CRGB col((uint8_t)((g_settings.run_r * bri) / 100u),
                 (uint8_t)((g_settings.run_g * bri) / 100u),
                 (uint8_t)((g_settings.run_b * bri) / 100u));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, tc, col);
    }
}
// Hardware: SEG_TOP_STRIP = clear diffuser (full colour).
//           SEG_BOT_STRIP + SEG_MAIN = red diffuser (only red channel passes).
// Each animation drives the top strip with full colour and generates a
// matching red-intensity signal for the other two segments.
// ===========================================================================

// Helper: speed-adjusted period factor
static inline unsigned long showPeriod(unsigned long baseMs) {
    return (unsigned long)(baseMs * 100UL / constrain(g_settings.show_speed, 50, 200));
}

// ── AnimShowRainbow ───────────────────────────────────────────────────────────
// Scrolling hue rainbow on the top strip; red intensity for the rest.
void AnimShowRainbow::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t baseHue = (uint8_t)((nowMs * 256UL) / showPeriod(6000UL));

    // Top strip — full colour rainbow
    for (int col = 0; col < STRIP_COLS; col++) {
        uint8_t hue = baseHue + (uint8_t)(col * 255 / STRIP_COLS);
        CRGB px; hsv2rgb_rainbow(CHSV(hue, 240, 200), px);
        for (int row = 0; row < STRIP_ROWS; row++)
            side.setPixel(SEG_TOP_STRIP, row, col, px);
    }
    // Bottom strip + main — brightness of rainbow as red intensity
    uint8_t midHue = baseHue + 128;
    CRGB midPx; hsv2rgb_rainbow(CHSV(midHue, 240, 200), midPx);
    uint8_t redIntensity = midPx.getLuma();
    side.fillSegment(SEG_BOT_STRIP, CRGB(redIntensity >> 1, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(redIntensity >> 1, 0, 0));
}

// ── AnimShowChase ─────────────────────────────────────────────────────────────
// Bright comet with a fading tail racing across the top strip.
void AnimShowChase::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int TAIL = 8;
    unsigned long period  = showPeriod(1200UL);
    unsigned long totalPos = STRIP_COLS + TAIL;
    int pos = (int)((nowMs % period) * totalPos / period);

    // Colour cycles through hue over time
    uint8_t hue = (uint8_t)((nowMs * 256UL) / showPeriod(8000UL));

    // Top strip — comet
    for (int col = 0; col < STRIP_COLS; col++) {
        for (int row = 0; row < STRIP_ROWS; row++)
            side.setPixel(SEG_TOP_STRIP, row, col, CRGB::Black);
    }
    for (int t = 0; t < TAIL; t++) {
        int c = side.isDriver() ? (pos - t) : (STRIP_COLS - 1 - (pos - t));
        if (c < 0 || c >= STRIP_COLS) continue;
        uint8_t bright = (uint8_t)(255 - (t * 255 / TAIL));
        CRGB px; hsv2rgb_rainbow(CHSV(hue, 220, bright), px);
        for (int row = 0; row < STRIP_ROWS; row++)
            side.setPixel(SEG_TOP_STRIP, row, c, px);
    }
    // Bottom + main — dim red base
    uint8_t base = (uint8_t)(20 + scale8(50, sin8((uint8_t)((nowMs * 256UL) / showPeriod(1200UL)))));
    side.fillSegment(SEG_BOT_STRIP, CRGB(base, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(base, 0, 0));
}

// ── AnimShowTheater ───────────────────────────────────────────────────────────
// Every-third-pixel marching pattern on the top strip; red dots on the rest.
void AnimShowTheater::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t hue   = (uint8_t)((nowMs * 256UL) / showPeriod(8000UL));
    int     shift = (int)((nowMs / showPeriod(80UL)) % 3);

    for (int col = 0; col < STRIP_COLS; col++) {
        bool lit = ((col + shift) % 3 == 0);
        CRGB px = CRGB::Black;
        if (lit) { hsv2rgb_rainbow(CHSV(hue + col * 8, 220, 220), px); }
        for (int row = 0; row < STRIP_ROWS; row++)
            side.setPixel(SEG_TOP_STRIP, row, col, px);
    }
    int botShift = (int)((nowMs / showPeriod(80UL)) % 3);
    for (int col = 0; col < STRIP_COLS; col++) {
        bool lit = ((col + botShift) % 3 == 0);
        CRGB px(lit ? 60 : 8, 0, 0);
        for (int row = 0; row < STRIP_ROWS; row++)
            side.setPixel(SEG_BOT_STRIP, row, col, px);
    }
    for (int col = 0; col < MAIN_COLS; col++) {
        bool lit = ((col + botShift) % 3 == 0);
        CRGB px(lit ? 60 : 8, 0, 0);
        for (int row = 0; row < MAIN_ROWS; row++)
            side.setPixel(SEG_MAIN, row, col, px);
    }
}

// ── AnimShowFire ──────────────────────────────────────────────────────────────
// Flickering fire — heat cells per column, cooling and sparking each frame.
void AnimShowFire::begin(TailLight& side, LightState /*state*/) {
    memset(_heat, 0, sizeof(_heat));
    _lastFrameMs = millis();
}

void AnimShowFire::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long frameInterval = showPeriod(60UL);
    if (nowMs - _lastFrameMs < frameInterval) {
        // Just re-draw last state without changing heat
    } else {
        _lastFrameMs = nowMs;
        // Cool every cell
        for (int c = 0; c < STRIP_COLS; c++) {
            int cool = random8(0, 40);
            _heat[c] = (_heat[c] > cool) ? _heat[c] - cool : 0;
        }
        // Diffuse heat upward (col + 1)
        for (int c = STRIP_COLS - 1; c >= 2; c--)
            _heat[c] = (_heat[c - 1] + _heat[c - 2] + _heat[c - 2]) / 3;
        // Random sparks at left edge
        if (random8() < 120) {
            int spark = random8(0, 4);
            _heat[spark] = qadd8(_heat[spark], random8(160, 255));
        }
    }

    // Map heat → colour on top strip (orange/yellow)
    for (int col = 0; col < STRIP_COLS; col++) {
        uint8_t h = _heat[col];
        CRGB px;
        if (h < 85)       px = CRGB(h * 3, 0, 0);
        else if (h < 170) px = CRGB(255, (h - 85) * 3, 0);
        else              px = CRGB(255, 255, (h - 170) * 3);
        for (int row = 0; row < STRIP_ROWS; row++)
            side.setPixel(SEG_TOP_STRIP, row, col, px);
    }
    // Red intensity on bottom + main
    uint8_t avgHeat = 0;
    for (int c = 0; c < STRIP_COLS; c++) avgHeat = qadd8(avgHeat, _heat[c] / STRIP_COLS);
    side.fillSegment(SEG_BOT_STRIP, CRGB(avgHeat >> 1, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(avgHeat >> 2, 0, 0));
}

// ── AnimShowMeteor ────────────────────────────────────────────────────────────
// White meteor streaks on the top strip, dim red base on the rest.
void AnimShowMeteor::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int TAIL = 7;
    unsigned long period = showPeriod(1800UL);
    int totalPos = STRIP_COLS + TAIL;
    int pos1 = (int)((nowMs % period) * totalPos / period);
    int pos2 = (int)(((nowMs + period / 2) % period) * totalPos / period);

    for (int col = 0; col < STRIP_COLS; col++) {
        // Fade existing top-strip pixels
        for (int row = 0; row < STRIP_ROWS; row++) {
            // just blank each frame and overdraw
            side.setPixel(SEG_TOP_STRIP, row, col, CRGB(4, 0, 0));
        }
    }
    // Draw two meteors
    auto drawMeteor = [&](int pos, uint8_t hue) {
        for (int t = 0; t < TAIL; t++) {
            int c = side.isDriver() ? (pos - t) : (STRIP_COLS - 1 - pos + t);
            if (c < 0 || c >= STRIP_COLS) continue;
            uint8_t bright = (uint8_t)(255 - (t * 255 / TAIL));
            CRGB px; hsv2rgb_rainbow(CHSV(hue, 200, bright), px);
            for (int row = 0; row < STRIP_ROWS; row++)
                side.setPixel(SEG_TOP_STRIP, row, c, px);
        }
    };
    uint8_t hue = (uint8_t)((nowMs * 256UL) / showPeriod(12000UL));
    drawMeteor(pos1, hue);
    drawMeteor(pos2, hue + 60);

    side.fillSegment(SEG_BOT_STRIP, CRGB(15, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(10, 0, 0));
}

// ── AnimShowPolice ────────────────────────────────────────────────────────────
// Alternating red / blue strobe; driver = red side, passenger = blue side.
void AnimShowPolice::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long half = showPeriod(120UL);
    bool redPhase = ((nowMs / half) % 2) == 0;

    if (side.isDriver()) {
        // Driver: red when redPhase, off when not
        CRGB topCol = redPhase ? CRGB(255, 0, 0) : CRGB::Black;
        side.fillSegment(SEG_TOP_STRIP, topCol);
        side.fillSegment(SEG_BOT_STRIP, redPhase ? CRGB(180, 0, 0) : CRGB::Black);
        side.fillSegment(SEG_MAIN,      redPhase ? CRGB(180, 0, 0) : CRGB::Black);
    } else {
        // Passenger: blue on top strip (won't show on red-diffuser segments), red on bottom
        CRGB topCol = redPhase ? CRGB::Black : CRGB(0, 0, 255);
        side.fillSegment(SEG_TOP_STRIP, topCol);
        // Red diffuser segments just pulse red in opposite phase
        side.fillSegment(SEG_BOT_STRIP, !redPhase ? CRGB(180, 0, 0) : CRGB::Black);
        side.fillSegment(SEG_MAIN,      !redPhase ? CRGB(180, 0, 0) : CRGB::Black);
    }
}

// ── AnimShowNightRider ────────────────────────────────────────────────────────
// KITT scanner: bright comet bouncing left ↔ right on the top strip.
void AnimShowNightRider::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int TAIL = 7;
    unsigned long period = showPeriod(1600UL);
    int totalPos = (STRIP_COLS - 1) * 2;
    int pos = (int)((nowMs % period) * totalPos / period);
    int col = (pos < STRIP_COLS) ? pos : totalPos - pos; // triangle wave

    // Blank top strip then draw comet
    for (int c = 0; c < STRIP_COLS; c++) {
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
    }
    for (int t = 0; t < TAIL; t++) {
        int tc = side.isDriver() ? (col - t) : (STRIP_COLS - 1 - col + t);
        if (tc < 0 || tc >= STRIP_COLS) continue;
        uint8_t bright = (uint8_t)(255 - (t * 255 / TAIL));
        CRGB px(bright, 0, 0);
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, tc, px);
    }
    // Bottom strip + main glow dimly in sync with position
    uint8_t glow = (uint8_t)(20 + scale8(40, (uint8_t)(255 - abs(col - STRIP_COLS / 2) * 255 / (STRIP_COLS / 2))));
    side.fillSegment(SEG_BOT_STRIP, CRGB(glow, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(glow >> 1, 0, 0));
}

// ── AnimShowColorCycle ────────────────────────────────────────────────────────
// Smooth full-panel hue rotation; top strip full colour, others red intensity.
void AnimShowColorCycle::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t hue = (uint8_t)((nowMs * 256UL) / showPeriod(8000UL));
    CRGB col; hsv2rgb_rainbow(CHSV(hue, 230, 210), col);

    side.fillSegment(SEG_TOP_STRIP, col);

    // Red-diffuser segments use only the luma as red intensity
    uint8_t luma = col.getLuma();
    side.fillSegment(SEG_BOT_STRIP, CRGB(luma >> 1, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(luma >> 1, 0, 0));
}

// ── AnimShowSparkle ──────────────────────────────────────────────────────────
// Stateless pseudo-random sparkle: each pixel has a sin8-derived trigger
// so bursts appear to pop on randomly across the strip.
void AnimShowSparkle::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t = (uint8_t)(nowMs / showPeriod(18UL));
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            uint8_t phase = sin8(t + (uint8_t)(r * 73 + c * 37));
            uint8_t bright = (phase > 210) ? (uint8_t)((phase - 210) * 5) : 0;
            if (bright > 10) {
                uint8_t hue = (uint8_t)(nowMs / showPeriod(20UL) + c * 11 + r * 29);
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(hue, 200, bright)));
            } else {
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
            }
        }
    }
    uint8_t redGlow = (uint8_t)(sin8(t * 3) >> 3);
    side.fillSegment(SEG_BOT_STRIP, CRGB(redGlow, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(redGlow, 0, 0));
}

// ── AnimShowPlasma ───────────────────────────────────────────────────────────
// Sine-wave interference field — completely stateless; hue derived from
// overlapping sin8 waves of time, column, and row.
void AnimShowPlasma::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t  = (uint8_t)(nowMs / showPeriod(35UL));
    uint8_t t2 = (uint8_t)(nowMs / showPeriod(55UL));
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            uint8_t v = sin8((uint8_t)(c * 13 + t))
                      + sin8((uint8_t)(r * 21 + t2 * 2))
                      + sin8((uint8_t)(c * 7 + r * 17 + t / 2));
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(v, 255, 180)));
        }
    }
    uint8_t base = sin8(t * 3);
    side.fillSegment(SEG_BOT_STRIP, CRGB((uint8_t)(20 + (base >> 3)), 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(20 + (base >> 3)), 0, 0));
}

// ── AnimShowMatrix ───────────────────────────────────────────────────────────
// Digital rain — each column has a falling bright green "head" with a
// decaying green trail.  Column speeds vary so they desync naturally.
// (Green is invisible through the red diffuser — only the top strip shows.)
void AnimShowMatrix::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    for (int c = 0; c < STRIP_COLS; c++) {
        // Each column drops at a slightly different speed.
        unsigned long colPeriod = showPeriod(300UL + (unsigned long)c * 28UL);
        int head = (int)((nowMs / (colPeriod / (STRIP_ROWS * 2))) % (STRIP_ROWS * 2));
        for (int r = 0; r < STRIP_ROWS; r++) {
            int dist = head - r;
            CRGB col;
            if (dist == 0)                    col = CRGB(180, 255, 180); // bright white-green head
            else if (dist == 1)               col = CRGB(0, 200, 0);
            else if (dist == 2)               col = CRGB(0, 120, 0);
            else if (dist == 3)               col = CRGB(0,  50, 0);
            else                              col = CRGB::Black;
            side.setPixel(SEG_TOP_STRIP, r, c, col);
        }
    }
    // Green passes as black through red diffuser — keep it dark/off
    side.fillSegment(SEG_BOT_STRIP, CRGB::Black);
    side.fillSegment(SEG_MAIN,      CRGB::Black);
}

// ── AnimShowJuggle ───────────────────────────────────────────────────────────
// Six colored balls bounce back and forth at different speeds.
// Each ball is rendered as a bright center with a two-pixel halo.
void AnimShowJuggle::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    side.fillSegment(SEG_TOP_STRIP, CRGB::Black);
    constexpr int NUM_BALLS = 6;
    for (int b = 0; b < NUM_BALLS; b++) {
        // beatsin8(bpm, lo, hi, timebase, phase_offset)
        uint8_t pos = beatsin8((uint8_t)(7 + b * 3), 0, STRIP_COLS - 1,
                               (unsigned long)(nowMs), (uint8_t)(b * 43));
        uint8_t hue = (uint8_t)(b * 43 + nowMs / showPeriod(8000UL));
        for (int r = 0; r < STRIP_ROWS; r++) {
            // Center pixel full brightness; neighbours faded
            side.setPixel(SEG_TOP_STRIP, r, pos, CRGB(CHSV(hue, 210, 230)));
            if (pos > 0)
                side.setPixel(SEG_TOP_STRIP, r, pos - 1,
                    CRGB(CHSV(hue, 210, 80)));
            if (pos < STRIP_COLS - 1)
                side.setPixel(SEG_TOP_STRIP, r, pos + 1,
                    CRGB(CHSV(hue, 210, 80)));
        }
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB::Black);
    side.fillSegment(SEG_MAIN,      CRGB::Black);
}

// ── AnimShowBPM ──────────────────────────────────────────────────────────────
// Spectrum-analyzer bars: each column has its own BPM, so columns rise and
// fall at different rates, creating a dancing equalizer look.
void AnimShowBPM::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    for (int c = 0; c < STRIP_COLS; c++) {
        uint8_t barH = beatsin8((uint8_t)(25 + c), 0, STRIP_ROWS,
                                (unsigned long)(nowMs), (uint8_t)(c * 12));
        for (int r = 0; r < STRIP_ROWS; r++) {
            // Fill from the bottom row upward
            int rowFromBottom = STRIP_ROWS - 1 - r;
            if (rowFromBottom < barH) {
                uint8_t hue = (uint8_t)(c * 12 + nowMs / showPeriod(5000UL));
                // Gradient brightness: brighter at top of bar
                uint8_t bright = (uint8_t)(120 + scale8(120, (uint8_t)(rowFromBottom * 255 / STRIP_ROWS)));
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(hue, 230, bright)));
            } else {
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
            }
        }
    }
    uint8_t redBeat = (uint8_t)(30 + beatsin8(60, 0, 60, (unsigned long)(nowMs)));
    side.fillSegment(SEG_BOT_STRIP, CRGB(redBeat, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(redBeat >> 1, 0, 0));
}

// ── AnimShowConfetti ─────────────────────────────────────────────────────────
// Random bright-colored sparks scatter over a near-black field.
// Stateless: pixel activity gates on sin8 of (time + unique pixel offset).
void AnimShowConfetti::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t = (uint8_t)(nowMs / showPeriod(40UL));
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            uint8_t slot = sin8((uint8_t)(t * 3 + r * 61 + c * 31));
            if (slot > 218) {
                uint8_t hue = (uint8_t)(t * 7 + r * 29 + c * 17);
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(hue, 200, 255)));
            } else if (slot > 190) {
                // Fading remnant
                uint8_t dim = (uint8_t)((slot - 190) * 3);
                uint8_t hue = (uint8_t)(t * 7 + r * 29 + c * 17 - 10);
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(hue, 200, dim)));
            } else {
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
            }
        }
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB(15, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(15, 0, 0));
}

// ── AnimShowOcean ────────────────────────────────────────────────────────────
// Pacifica-inspired ocean waves: overlapping sine-wave layers in blue/teal.
void AnimShowOcean::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t1 = (uint8_t)(nowMs / showPeriod(100UL));
    uint8_t t2 = (uint8_t)(nowMs / showPeriod(160UL));
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            uint8_t w1 = sin8((uint8_t)(c * 15 + t1 * 2));
            uint8_t w2 = sin8((uint8_t)(c * 9 - t2 + r * 22));
            uint8_t w3 = sin8((uint8_t)(c * 6 + r * 11 + t1 / 2));
            uint8_t combined = (uint8_t)(((uint16_t)w1 + w2 + w3) / 3);
            // Ocean hue range: teal (128) to deep blue (160)
            uint8_t hue    = (uint8_t)(128 + (combined >> 3));
            uint8_t bright = (uint8_t)(60 + (combined >> 1));
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(hue, 230, bright)));
        }
    }
    // Blue/teal → nearly black through red diffuser; keep a faint glow
    side.fillSegment(SEG_BOT_STRIP, CRGB(8, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(8, 0, 0));
}

// ── AnimShowLightning ────────────────────────────────────────────────────────
// Random white flashes with dark pauses.  Driver and passenger sides fire
// independently (each stores its own timing in _nextFlash / _flashEnd).
void AnimShowLightning::begin(TailLight& side, LightState /*state*/) {
    int i = side.isDriver() ? 0 : 1;
    _nextFlash[i] = millis() + (unsigned long)random(300, 1200);
    _flashEnd[i]  = 0;
    _strikeCol[i] = (int8_t)(STRIP_COLS / 2);
}

void AnimShowLightning::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    int i = side.isDriver() ? 0 : 1;

    // Trigger a new flash when the timer fires
    if (nowMs >= _flashEnd[i] && nowMs >= _nextFlash[i]) {
        unsigned long dur  = (unsigned long)random(20, 90);
        _flashEnd[i]  = nowMs + dur;
        _nextFlash[i] = _flashEnd[i] + (unsigned long)random(showPeriod(400UL),
                                                              showPeriod(2000UL));
        _strikeCol[i] = (int8_t)random(2, STRIP_COLS - 2);
    }

    bool lit = (nowMs < _flashEnd[i]);
    int  sc  = _strikeCol[i];
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            CRGB px = CRGB::Black;
            if (lit) {
                int d = abs(c - sc);
                if (d == 0)      px = CRGB(220, 220, 255); // white-blue core
                else if (d == 1) px = CRGB(80,  80,  200);
                else if (d == 2) px = CRGB(20,  20,   80);
            }
            side.setPixel(SEG_TOP_STRIP, r, c, px);
        }
    }
    side.fillSegment(SEG_BOT_STRIP, lit ? CRGB(60, 0, 0) : CRGB::Black);
    side.fillSegment(SEG_MAIN,      lit ? CRGB(30, 0, 0) : CRGB::Black);
}

// ── AnimShowHeartbeat ────────────────────────────────────────────────────────
// Double-pulse "lub-dub" cardiac rhythm.  Both top and bottom strips pulse
// red — the clear diffuser shows full red on top, the red diffuser amplifies
// it on the lower segments.
void AnimShowHeartbeat::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long t = nowMs % showPeriod(900UL);
    uint8_t bright = 0;
    // Pulse 1: fast rise/fall (0–100 ms range)
    if      (t <  80UL) bright = (uint8_t)(t * 3);
    else if (t < 160UL) bright = (uint8_t)((160UL - t) * 3);
    // Brief gap then Pulse 2 (slightly smaller)
    else if (t < 240UL) bright = (uint8_t)((t - 160UL) * 2);
    else if (t < 320UL) bright = (uint8_t)((320UL - t) * 2);
    // Dark until next beat

    if (bright > 255) bright = 255;
    side.fillSegment(SEG_TOP_STRIP, CRGB(bright, 0, 0));
    side.fillSegment(SEG_BOT_STRIP, CRGB((uint8_t)(bright >> 1), 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(bright >> 1), 0, 0));
}

// ── AnimShowRipple ───────────────────────────────────────────────────────────
// Concentric rings expand outward from the horizontal center of the top strip.
// Hue rotates slowly so the rings cycle through colors.
void AnimShowRipple::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t   = (uint8_t)(nowMs / showPeriod(55UL));
    uint8_t hue = (uint8_t)(nowMs / showPeriod(5000UL));
    int center  = STRIP_COLS / 2;
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            int   dist  = abs(c - center) * 2 + r;  // pseudo radial distance
            uint8_t wave = sin8((uint8_t)(dist * 22 - t * 4));
            uint8_t bri  = (wave > 140) ? (uint8_t)((wave - 140) * 2) : 0;
            side.setPixel(SEG_TOP_STRIP, r, c,
                          CRGB(CHSV((uint8_t)(hue + dist * 8), 230, bri)));
        }
    }
    uint8_t redRipple = (uint8_t)(sin8((uint8_t)(255 - t * 4)) >> 2);
    side.fillSegment(SEG_BOT_STRIP, CRGB(redRipple, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(redRipple, 0, 0));
}

// ── AnimShowSunrise ───────────────────────────────────────────────────────────
// Warm gradient cycles from near-black through deep red, orange, and yellow
// then back down — like a slow sunrise/sunset looping forever.
void AnimShowSunrise::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long period = showPeriod(7000UL);
    uint8_t phase = (uint8_t)((nowMs % period) * 255UL / period);

    // Hue 0 (red) → 20 (orange) → 40 (yellow) and back via sin
    uint8_t hue = (uint8_t)(scale8(42, sin8(phase)));
    uint8_t val = (uint8_t)(80 + scale8(160, sin8(phase)));

    for (int r = 0; r < STRIP_ROWS; r++) {
        // Lower rows are dimmer (horizon is darker)
        uint8_t rowBright = (uint8_t)((unsigned)val * (STRIP_ROWS - r) / STRIP_ROWS);
        uint8_t rowHue    = (uint8_t)(hue > (uint8_t)(r * 3) ? hue - r * 3 : 0);
        for (int c = 0; c < STRIP_COLS; c++) {
            // Per-column shimmer for a heat-haze effect
            uint8_t shimmer = (uint8_t)(sin8((uint8_t)(c * 19 + phase * 2)) >> 4);
            side.setPixel(SEG_TOP_STRIP, r, c,
                          CRGB(CHSV(rowHue, 230, (uint8_t)(rowBright + shimmer))));
        }
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB(CHSV(hue, 230, (uint8_t)(val >> 1))));
    side.fillSegment(SEG_MAIN,      CRGB(CHSV(hue, 230, (uint8_t)(val >> 1))));
}

// ── AnimShowText ──────────────────────────────────────────────────────────────
// Scrolls g_settings.show_text across SEG_TOP_STRIP using the built-in 5×5
// pixel font.  Scrolling is derived entirely from nowMs so both sides stay
// perfectly in sync.  The buffer is rebuilt automatically whenever the text
// changes.
void AnimShowText::begin(TailLight& /*side*/, LightState /*state*/) {
    // Force buffer rebuild on next update()
    _cachedText[0] = '\0';
}

void AnimShowText::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    // Rebuild column buffer if the text string has changed
    if (strncmp(_cachedText, g_settings.show_text, sizeof(_cachedText)) != 0) {
        strncpy(_cachedText, g_settings.show_text, sizeof(_cachedText) - 1);
        _cachedText[sizeof(_cachedText) - 1] = '\0';
        font5x_buildBuffer(_cachedText, _colBuf, (int)sizeof(_colBuf), &_textCols);
    }

    // Total scroll distance: blank panel → text → blank panel
    int totalCols = STRIP_COLS + _textCols + STRIP_COLS;
    unsigned long msPerCol = showPeriod(55UL);
    int offset = (int)((nowMs / msPerCol) % (unsigned long)totalCols);

    // Reverse column so glyphs read correctly on both sides — see note in
    // AnimScrollText::update for the full explanation.
    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            int srcCol = offset - STRIP_COLS + c;
            bool lit = false;
            if (srcCol >= 0 && srcCol < _textCols) {
                lit = ((_colBuf[srcCol] >> r) & 1) != 0;
            }
            side.setPixel(SEG_TOP_STRIP, r, STRIP_COLS - 1 - c,
                          lit ? CRGB(220, 220, 220) : CRGB::Black);
        }
    }
    // Faint warm-red background on the diffuser segments while text scrolls
    side.fillSegment(SEG_BOT_STRIP, CRGB(18, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(18, 0, 0));
}

// ── AnimShowColorwaves ────────────────────────────────────────────────────────
// Ported from WLED's mode_colorwaves (Mark Kriegsman).
// Two sin8 waves at different speeds and frequencies create interference bands.
void AnimShowColorwaves::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t1 = (uint8_t)(nowMs / showPeriod(14UL));
    uint8_t t2 = (uint8_t)(nowMs / showPeriod(9UL));
    uint8_t t3 = (uint8_t)(nowMs / showPeriod(22UL));
    for (int c = 0; c < STRIP_COLS; c++) {
        uint8_t w1  = sin8((uint8_t)(c * 10 + t1));
        uint8_t w2  = sin8((uint8_t)(c *  6 + t2));
        uint8_t w3  = sin8((uint8_t)(c * 15 - t3));
        uint8_t hue = (uint8_t)(((uint16_t)w1 + (uint16_t)w3) / 2);
        uint8_t bri = qadd8(w1 >> 1, w2 >> 1);
        bri         = max(bri, (uint8_t)28);
        CRGB col    = CRGB(CHSV(hue, 240, bri));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, col);
    }
    uint8_t rb = (uint8_t)(sin8((uint8_t)(nowMs / showPeriod(20UL))) / 2 + 40);
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(rb, 0, 0));
}

// ── AnimShowTwinkleFox ────────────────────────────────────────────────────────
// Ported from WLED's TwinkleFOX by Mark Kriegsman.
// Each column uses a stable PRNG seed to independently twinkle in and out.
void AnimShowTwinkleFox::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint16_t PRNG16 = 11337u;
    for (int c = 0; c < STRIP_COLS; c++) {
        PRNG16 = (uint16_t)(PRNG16 * 2053u + 1384u);
        uint8_t salt = (uint8_t)(PRNG16 >> 8);
        uint8_t hue  = (uint8_t)(PRNG16 & 0xFF);

        // Each pixel has its own time-tick rate derived from salt
        unsigned long clockDiv = showPeriod(16UL) + (unsigned long)(salt >> 3);
        unsigned long ticks    = nowMs / clockDiv;
        uint8_t fastcycle      = (uint8_t)ticks;

        // Slow-cycle density gate (WLED algorithm)
        uint16_t slowcycle = (uint16_t)(ticks >> 8) + salt;
        slowcycle += sin8(slowcycle & 0xFF);
        slowcycle  = (uint16_t)(slowcycle * 2053u + 1384u);
        uint8_t sc8 = (uint8_t)((slowcycle & 0xFF) + (slowcycle >> 8));

        uint8_t bri = 0;
        if (((sc8 & 0x0Eu) >> 1) < 5u) {            // density ~5/8
            if      (fastcycle < 86u)  bri = fastcycle * 3u;
            else if (fastcycle < 171u) bri = 255u - (fastcycle - 86u) * 3u;
        }
        CRGB col = (bri > 0) ? CRGB(CHSV(hue, 200, bri)) : CRGB::Black;
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, col);
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB(12, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(12, 0, 0));
}

// ── AnimShowBouncingBalls ─────────────────────────────────────────────────────
// Ported from WLED's mode_bouncing_balls (Aircoookie).
// Five hue-cycling balls launch under gravity, bounce with damping, and
// relaunch when their energy is depleted.
static constexpr float kBBGravity = -9.81f;

void AnimShowBouncingBalls::begin(TailLight& /*side*/, LightState /*state*/) {
    unsigned long now = millis();
    for (int i = 0; i < NUM_BALLS; i++) {
        _lastBounceMs[i] = now;
        // Each ball gets a slightly different launch speed → different peak heights
        _impactVel[i] = sqrtf(-2.0f * kBBGravity) * (0.7f + i * 0.06f);
        _hue[i]       = (uint8_t)(i * 51u);
    }
    _inited = true;
}

void AnimShowBouncingBalls::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    if (!_inited) begin(side, LightState::SHOW);

    side.fillSegment(SEG_TOP_STRIP, CRGB::Black);

    for (int i = 0; i < NUM_BALLS; i++) {
        float tSec = (float)(nowMs - _lastBounceMs[i]) / 1000.0f;
        float pos  = (0.5f * kBBGravity * tSec + _impactVel[i]) * tSec;

        if (pos <= 0.0f) {
            // Bounce: apply damping, reset clock
            float damping     = 0.9f - (float)i / (float)(NUM_BALLS * NUM_BALLS);
            _impactVel[i]    *= damping;
            _lastBounceMs[i]  = nowMs;
            // Relaunch when energy is exhausted
            if (_impactVel[i] < 0.015f) {
                uint8_t rng = random8(5, 11);
                _impactVel[i] = sqrtf(-2.0f * kBBGravity) * (float)rng / 10.0f;
            }
            pos = 0.0f;
        } else if (pos > 1.0f) {
            continue;   // ball is above the strip – skip
        }

        int col = constrain((int)roundf(pos * (float)(STRIP_COLS - 1)), 0, STRIP_COLS - 1);
        CRGB ballCol = CRGB(CHSV(_hue[i], 255, 255));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, col, ballCol);
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB(30, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(30, 0, 0));
}

// ── AnimShowFireworks ─────────────────────────────────────────────────────────
// Inspired by WLED's mode_exploding_fireworks.
// A bright flare races across the top strip, bursts into hue-coloured sparks,
// waits a beat, then repeats.
void AnimShowFireworks::begin(TailLight& /*side*/, LightState /*state*/) {
    _phase     = 2;       // start in idle so first frame triggers a launch
    _lastMs    = millis();
    _idleUntil = millis();
    for (int i = 0; i < MAX_SPARKS; i++) _sparks[i] = {};
}

void AnimShowFireworks::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long dt = min((unsigned long)80, nowMs - _lastMs);
    _lastMs = nowMs;
    float dtSec = dt / 1000.0f;

    side.fillSegment(SEG_TOP_STRIP, CRGB::Black);

    if (_phase == 0) {
        // ── FLARE RISING ──────────────────────────────────────────────────────
        _flarePos += _flareVel * dtSec;
        _flareVel -= 22.0f * dtSec;    // decelerate (gravity)

        int fc = constrain((int)_flarePos, 0, STRIP_COLS - 1);
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, fc, CRGB(255, 220, 80));  // bright yellow-white
        if (fc > 0)
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, fc - 1, CRGB(80, 55, 15)); // dim trail

        if (_flareVel <= 0.0f || _flarePos >= (float)(STRIP_COLS - 1)) {
            // Explode
            _phase         = 1;
            int nSparks    = 8 + (int)random8(9);   // 8-16 sparks
            _flareHue     += (uint8_t)random8(40);  // slight hue drift each burst
            for (int i = 0; i < MAX_SPARKS; i++) {
                if (i < nSparks) {
                    _sparks[i].pos  = _flarePos;
                    _sparks[i].vel  = ((float)(int8_t)random8() / 10.0f);  // -12.8 … +12.7
                    _sparks[i].hue  = _flareHue + random8(50);
                    _sparks[i].life = 160 + random8(80);
                } else {
                    _sparks[i].life = 0;
                }
            }
        }
    } else if (_phase == 1) {
        // ── SPARKS FADING ─────────────────────────────────────────────────────
        bool anyAlive = false;
        for (int i = 0; i < MAX_SPARKS; i++) {
            if (_sparks[i].life == 0) continue;
            anyAlive = true;
            _sparks[i].pos  += _sparks[i].vel * dtSec * 10.0f;
            _sparks[i].vel  *= (1.0f - 1.5f * dtSec);  // friction
            if (_sparks[i].life > 4) _sparks[i].life -= 4; else _sparks[i].life = 0;

            int sc = constrain((int)roundf(_sparks[i].pos), 0, STRIP_COLS - 1);
            CRGB sc_col = CRGB(CHSV(_sparks[i].hue, 220, _sparks[i].life));
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, sc, sc_col);
        }
        if (!anyAlive) {
            _phase     = 2;
            _idleUntil = nowMs + 400UL + (unsigned long)random8() * 3UL;  // 400–1165 ms gap
        }
    } else {
        // ── IDLE – wait then launch ────────────────────────────────────────────
        if (nowMs >= _idleUntil) {
            _phase    = 0;
            _flarePos = 0.0f;
            // Launch velocity: enough to reach somewhere between col 12 and STRIP_COLS-1
            float peak = 12.0f + (float)random8(STRIP_COLS - 13);
            _flareVel  = sqrtf(2.0f * 22.0f * peak);  // v = sqrt(2*a*d)
            _flareHue  = random8();
        }
    }

    uint8_t rb = (_phase == 1) ? 60u : 15u;
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(rb, 0, 0));
}

// ── AnimShowDrip ──────────────────────────────────────────────────────────────
// Ported from WLED's mode_drip (based on a YouTube tutorial concept).
// Drops swell at the right edge (top), fall leftward under gravity, bounce
// once at the left edge (floor), then reset.
static constexpr float kDripGravity = -25.0f;  // columns / s²

void AnimShowDrip::begin(TailLight& /*side*/, LightState /*state*/) {
    for (int j = 0; j < MAX_DROPS; j++) _drops[j] = {};  // colIndex=0 → init on first update
    _lastMs = millis();
}

void AnimShowDrip::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long dt = min((unsigned long)80, nowMs - _lastMs);
    _lastMs = nowMs;
    float dtSec = dt / 1000.0f;

    side.fillSegment(SEG_TOP_STRIP, CRGB::Black);

    static const int sourceDrop = 12;

    for (int j = 0; j < MAX_DROPS; j++) {
        Drop& d = _drops[j];

        // ── Initialise drop ───────────────────────────────────────────────────
        if (d.colIndex == 0) {
            d.pos      = (float)(STRIP_COLS - 1);
            d.vel      = 0.0f;
            d.col      = (uint8_t)sourceDrop;
            d.hue      = random8();
            d.colIndex = 1;   // forming
        }

        // ── Source glow at right edge ─────────────────────────────────────────
        {
            int ec = STRIP_COLS - 1;
            CRGB sc = CRGB(CHSV(d.hue, 220, (uint8_t)sourceDrop));
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, ec, sc);
        }

        // ── Forming phase ─────────────────────────────────────────────────────
        if (d.colIndex == 1) {
            int inc = constrain((int)(5.0f * dtSec * 60.0f), 1, 18);
            d.col   = (uint8_t)min(255, (int)d.col + inc);
            int ec  = STRIP_COLS - 1;
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, ec, CRGB(CHSV(d.hue, 220, d.col)));
            // Randomly release once bright enough
            if (random8() < d.col / 10u && d.col > 80u) {
                d.colIndex = 2;
                d.col      = 255u;
            }
        }

        // ── Falling / bouncing phase ──────────────────────────────────────────
        if (d.colIndex >= 2) {
            d.vel += kDripGravity * dtSec;
            d.pos += d.vel      * dtSec;

            // Trail (pixels to the right / above the falling drop)
            int trailLen = (d.colIndex == 2) ? 4 : 1;
            int ic = constrain((int)roundf(d.pos), 0, STRIP_COLS - 1);
            for (int t = 1; t <= trailLen; t++) {
                int tc = constrain(ic + t, 0, STRIP_COLS - 1);
                uint8_t tbri = d.col / (uint8_t)(t + 1);
                for (int r = 0; r < STRIP_ROWS; r++)
                    side.setPixel(SEG_TOP_STRIP, r, tc, CRGB(CHSV(d.hue, 200, tbri)));
            }
            // Main drop
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, ic, CRGB(CHSV(d.hue, 220, d.col)));

            // Bounce splash on floor (col 0)
            if (d.colIndex > 2) {
                for (int r = 0; r < STRIP_ROWS; r++)
                    side.setPixel(SEG_TOP_STRIP, r, 0, CRGB(CHSV(d.hue, 150, d.col)));
            }

            // Hit the floor?
            if (d.pos <= 0.0f) {
                if (d.colIndex > 2) {
                    // Already bounced once → reset
                    d.colIndex = 0;
                } else {
                    // First impact: small bounce
                    if (d.colIndex == 2) {
                        d.vel = -d.vel * 0.25f;
                        d.pos = 0.0f;
                    }
                    d.col      = (uint8_t)(sourceDrop * 2);
                    d.colIndex = 5;   // bouncing state
                }
            }

            // Age the bounce splash
            if (d.colIndex == 5) {
                if (d.col > 4u) d.col -= 4u;
                else            d.colIndex = 0;
            }
        }
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB(18, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(18, 0, 0));
}

// ── AnimShowCylonDual ─────────────────────────────────────────────────────────
// Two mirrored scanners sweep from the outer edges toward the centre.
// When they converge the whole strip flashes; then they diverge and repeat.
void AnimShowCylonDual::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int TAIL = 6;
    unsigned long period  = showPeriod(1600UL);
    int totalPos          = (STRIP_COLS - 1) * 2;
    int rawPos            = (int)((nowMs % period) * (unsigned long)totalPos / period);
    int colA              = (rawPos < STRIP_COLS) ? rawPos : totalPos - rawPos;
    int colB              = (STRIP_COLS - 1) - colA;

    uint8_t hue = (uint8_t)(nowMs / showPeriod(12000UL));

    if (abs(colA - colB) <= 2) {
        CRGB flash = CRGB(CHSV(hue, 200, 255));
        for (int c = 0; c < STRIP_COLS; c++)
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, c, flash);
        side.fillSegment(SEG_BOT_STRIP, CRGB(200, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB(160, 0, 0));
        return;
    }

    for (int c = 0; c < STRIP_COLS; c++)
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);

    int dirA = (rawPos < STRIP_COLS) ? -1 : 1;
    int dirB = -dirA;

    for (int t = 0; t < TAIL; t++) {
        int tcA = colA + dirA * t;
        int tcB = colB + dirB * t;
        uint8_t bri = (uint8_t)(255u - (uint8_t)((unsigned)t * 255u / (unsigned)TAIL));
        for (int r = 0; r < STRIP_ROWS; r++) {
            if (tcA >= 0 && tcA < STRIP_COLS)
                side.setPixel(SEG_TOP_STRIP, r, tcA, CRGB(CHSV(hue, 230, bri)));
            if (tcB >= 0 && tcB < STRIP_COLS)
                side.setPixel(SEG_TOP_STRIP, r, tcB, CRGB(CHSV((uint8_t)(hue + 128u), 230, bri)));
        }
    }

    int separation = abs(colA - colB);
    int glow = 20 + ((STRIP_COLS - 3 - separation) * 80 / (STRIP_COLS - 3));
    glow = constrain(glow, 20, 100);
    side.fillSegment(SEG_BOT_STRIP, CRGB((uint8_t)glow, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(glow >> 1), 0, 0));
}

// ── AnimShowV8 ────────────────────────────────────────────────────────────────
// Eight column groups flash in the classic American V8 firing order
// 1-8-4-3-6-5-7-2 (groups 0-7, left to right).
// Bot/main pulses red on each cylinder fire — engine-exhaust heat look.
void AnimShowV8::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static const uint8_t ORDER[8] = { 0, 7, 3, 2, 5, 4, 6, 1 };
    unsigned long firePeriod = showPeriod(150UL);
    int   step = (int)((nowMs / firePeriod) % 8u);
    uint8_t t  = (uint8_t)((nowMs % firePeriod) * 255UL / firePeriod);

    int activeGroup = ORDER[step];
    uint8_t bright;
    if (t < 50u) bright = (uint8_t)((unsigned)t * 5u);
    else         bright = (uint8_t)(250u - ((unsigned)(t - 50u) * 250u / 205u));
    bright = max(bright, (uint8_t)4u);

    uint8_t hue = (uint8_t)(nowMs / showPeriod(8000UL));

    for (int c = 0; c < STRIP_COLS; c++) {
        int grp = (c * 8) / STRIP_COLS;
        bool isActive = (grp == activeGroup);
        uint8_t b = isActive ? bright : (uint8_t)(bright >> 4u);
        CRGB col = isActive ? CRGB(CHSV((uint8_t)(hue + (uint8_t)((unsigned)activeGroup * 32u)), 220, b))
                            : CRGB(CHSV(hue, 200, b));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, col);
    }
    uint8_t rb = (uint8_t)(15u + (bright >> 1u));
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(rb >> 1u), 0, 0));
}

// ── AnimShowDragLaunch ────────────────────────────────────────────────────────
// NHRA Christmas-tree staging sequence loops every ~3 s:
// idle → burnout glow → 3 amber steps → green GO → twin launch comets.
void AnimShowDragLaunch::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    unsigned long cycle = showPeriod(3000UL);
    unsigned long tc    = (nowMs % cycle) * 3000UL / cycle;   // 0-2999 in base-speed ms

    for (int c = 0; c < STRIP_COLS; c++)
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
    side.fillSegment(SEG_BOT_STRIP, CRGB::Black);
    side.fillSegment(SEG_MAIN,      CRGB::Black);

    const int center = STRIP_COLS / 2;

    if (tc < 500UL) {
        side.fillSegment(SEG_BOT_STRIP, CRGB(8, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB(6, 0, 0));

    } else if (tc < 1400UL) {
        uint8_t pct = (uint8_t)((tc - 500UL) * 255UL / 900UL);
        uint8_t rb  = (uint8_t)(10u + (pct >> 2u));
        side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(rb >> 1u), 0, 0));
        uint8_t topBri = pct >> 3u;
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, center, CRGB(topBri, (uint8_t)(topBri >> 1u), 0));

    } else if (tc < 1700UL) {
        CRGB amber(255, 100, 0);
        for (int r = 0; r < STRIP_ROWS; r++) side.setPixel(SEG_TOP_STRIP, r, center - 2, amber);
        side.fillSegment(SEG_BOT_STRIP, CRGB(35, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB(20, 0, 0));

    } else if (tc < 2000UL) {
        CRGB amber(255, 100, 0);
        for (int r = 0; r < STRIP_ROWS; r++) side.setPixel(SEG_TOP_STRIP, r, center - 2, amber);
        for (int r = 0; r < STRIP_ROWS; r++) side.setPixel(SEG_TOP_STRIP, r, center,     amber);
        side.fillSegment(SEG_BOT_STRIP, CRGB(50, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB(30, 0, 0));

    } else if (tc < 2300UL) {
        CRGB amber(255, 100, 0);
        for (int r = 0; r < STRIP_ROWS; r++) side.setPixel(SEG_TOP_STRIP, r, center - 2, amber);
        for (int r = 0; r < STRIP_ROWS; r++) side.setPixel(SEG_TOP_STRIP, r, center,     amber);
        for (int r = 0; r < STRIP_ROWS; r++) side.setPixel(SEG_TOP_STRIP, r, center + 2, amber);
        side.fillSegment(SEG_BOT_STRIP, CRGB(70, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB(40, 0, 0));

    } else if (tc < 2500UL) {
        for (int c = 0; c < STRIP_COLS; c++)
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB(0, 255, 0));
        side.fillSegment(SEG_BOT_STRIP, CRGB(80, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB(50, 0, 0));

    } else {
        static constexpr int TAIL = 5;
        int spread = (int)((tc - 2500UL) * (unsigned long)(STRIP_COLS / 2) / 500UL);
        spread = constrain(spread, 0, STRIP_COLS / 2);
        int headL = center - spread;
        int headR = center + spread;
        for (int t = 0; t < TAIL; t++) {
            int lc = headL + t;
            if (lc >= 0 && lc < STRIP_COLS) {
                uint8_t bri = (uint8_t)(220u - (uint8_t)((unsigned)t * 220u / (unsigned)TAIL));
                for (int r = 0; r < STRIP_ROWS; r++)
                    side.setPixel(SEG_TOP_STRIP, r, lc, CRGB(bri, (uint8_t)(bri >> 1u), 0));
            }
            int rc = headR - t;
            if (rc >= 0 && rc < STRIP_COLS) {
                uint8_t bri = (uint8_t)(220u - (uint8_t)((unsigned)t * 220u / (unsigned)TAIL));
                for (int r = 0; r < STRIP_ROWS; r++)
                    side.setPixel(SEG_TOP_STRIP, r, rc, CRGB(0, (uint8_t)(bri >> 1u), bri));
            }
        }
        uint8_t rb = (uint8_t)constrain(15 + spread * 3, 15, 60);
        side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
        side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(rb >> 1u), 0, 0));
    }
}

// ── AnimShowNeon ──────────────────────────────────────────────────────────────
// High-saturation hue wash on the top strip — tuner-car underbody neon look.
// Bot/main breathe red in sync with the brightness cycle.
void AnimShowNeon::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t hue = (uint8_t)(nowMs / showPeriod(10000UL));
    uint8_t bri = (uint8_t)(140u + scale8(100u, sin8((uint8_t)(nowMs / showPeriod(2200UL)))));
    for (int c = 0; c < STRIP_COLS; c++) {
        uint8_t colHue = (uint8_t)(hue + (uint8_t)((unsigned)c * 4u));
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(colHue, 255, bri)));
    }
    uint8_t rb = (uint8_t)(25u + scale8(50u, sin8((uint8_t)(nowMs / showPeriod(2200UL)))));
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(rb >> 1u), 0, 0));
}

// ── AnimShowSpeedStreaks ──────────────────────────────────────────────────────
// Four short bright comets shoot across the top strip at staggered speeds
// and in alternating directions — like motion-blur speed lines.
void AnimShowSpeedStreaks::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static const unsigned long PERIODS[4] = { 700UL, 950UL, 550UL, 820UL };
    static const int           LENGTHS[4] = { 5, 3, 7, 4 };
    static const uint8_t       HUES[4]    = { 0, 20, 40, 200 };

    for (int c = 0; c < STRIP_COLS; c++)
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);

    uint8_t hueShift = (uint8_t)(nowMs / showPeriod(6000UL));

    for (int i = 0; i < 4; i++) {
        unsigned long p   = showPeriod(PERIODS[i]);
        int           len = LENGTHS[i];
        int totalPos      = STRIP_COLS + len;
        int pos = (int)((nowMs % p) * (unsigned long)totalPos / p);
        if (i % 2 == 1) pos = totalPos - 1 - pos;

        uint8_t baseHue = (uint8_t)(HUES[i] + hueShift);
        for (int t = 0; t < len; t++) {
            int cc = side.isDriver() ? (pos - t) : (STRIP_COLS - 1 - (pos - t));
            if (cc < 0 || cc >= STRIP_COLS) continue;
            uint8_t bri = (uint8_t)(255u - (uint8_t)((unsigned)t * 255u / (unsigned)len));
            for (int r = 0; r < STRIP_ROWS; r++)
                side.setPixel(SEG_TOP_STRIP, r, cc, CRGB(CHSV(baseHue, 220, bri)));
        }
    }
    side.fillSegment(SEG_BOT_STRIP, CRGB(15, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(10, 0, 0));
}

// ── AnimShowRadar ─────────────────────────────────────────────────────────────
// Bright column sweeps left-to-right with a 14-column decaying trail.
void AnimShowRadar::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    static constexpr int DECAY = 14;
    unsigned long period = showPeriod(2200UL);
    int head = (int)((nowMs % period) * (unsigned long)STRIP_COLS / period);
    uint8_t hue = (uint8_t)(nowMs / showPeriod(10000UL));

    for (int c = 0; c < STRIP_COLS; c++) {
        int behind = (head - c + STRIP_COLS) % STRIP_COLS;
        uint8_t bri;
        if      (behind == 0)     bri = 255u;
        else if (behind < DECAY)  bri = (uint8_t)(255u - (uint8_t)((unsigned)behind * 255u / (unsigned)DECAY));
        else                      bri = 0u;
        CRGB px = (bri > 4u) ? CRGB(CHSV(hue, 220, bri)) : CRGB::Black;
        for (int r = 0; r < STRIP_ROWS; r++)
            side.setPixel(SEG_TOP_STRIP, r, c, px);
    }
    uint8_t rb = (uint8_t)(15u + (uint8_t)((unsigned)(STRIP_COLS - head) * 40u / (unsigned)STRIP_COLS));
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB((uint8_t)(rb >> 1u), 0, 0));
}

// ── AnimShowAurora ────────────────────────────────────────────────────────────
// Three overlapping sine-wave layers in blue-green / cyan / teal hues,
// brightest at the top row.  Near-black through red diffuser → dark and moody.
void AnimShowAurora::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t t1 = (uint8_t)(nowMs / showPeriod(48UL));
    uint8_t t2 = (uint8_t)(nowMs / showPeriod(77UL));
    uint8_t t3 = (uint8_t)(nowMs / showPeriod(33UL));

    for (int r = 0; r < STRIP_ROWS; r++) {
        uint8_t rowFade = (uint8_t)(255u - (uint8_t)((unsigned)r * 50u));
        for (int c = 0; c < STRIP_COLS; c++) {
            uint8_t l1 = sin8((uint8_t)((unsigned)c * 19u + t1));
            uint8_t l2 = sin8((uint8_t)((unsigned)c * 11u - (uint8_t)((unsigned)t2 * 2u) + (uint8_t)((unsigned)r * 13u)));
            uint8_t l3 = sin8((uint8_t)((unsigned)c *  7u + t3 + (uint8_t)((unsigned)r * 21u)));
            uint8_t v  = (uint8_t)(((uint16_t)l1 + l2 + l3) / 3u);
            uint8_t aHue = (uint8_t)(96u + (v >> 3u));
            uint8_t sat  = (uint8_t)(255u - (v >> 3u));
            uint8_t bri  = scale8((uint8_t)(v >> 1u), rowFade);
            side.setPixel(SEG_TOP_STRIP, r, c,
                          (bri > 8u) ? CRGB(CHSV(aHue, sat, bri)) : CRGB::Black);
        }
    }
    uint8_t rb = (uint8_t)((sin8((uint8_t)((unsigned)t1 * 2u)) >> 4u) + 5u);
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(rb, 0, 0));
}

// ── AnimShowGlitch ────────────────────────────────────────────────────────────
// Digital glitch: mostly dark with random full-saturation colour blocks
// popping on and off in irregular bursts — cyberpunk aesthetic.
void AnimShowGlitch::update(TailLight& side, LightState /*state*/, unsigned long nowMs) {
    uint8_t  t   = (uint8_t)(nowMs / showPeriod(20UL));
    uint16_t big = (uint16_t)(nowMs / showPeriod(35UL));

    for (int r = 0; r < STRIP_ROWS; r++) {
        for (int c = 0; c < STRIP_COLS; c++) {
            uint8_t gate1 = sin8((uint8_t)((uint16_t)(big * 7u)  + (uint8_t)((unsigned)c * 31u)));
            uint8_t gate2 = sin8((uint8_t)((uint16_t)(big * 13u) + (uint8_t)((unsigned)r * 43u) + (uint8_t)((unsigned)c * 11u)));
            uint16_t b1   = (gate1 > 210u) ? (uint16_t)((gate1 - 210u) * 7u) : 0u;
            uint16_t b2   = (gate2 > 220u) ? (uint16_t)((gate2 - 220u) * 9u) : 0u;
            uint16_t bmax = (b1 > b2) ? b1 : b2;
            uint8_t  bri  = (bmax > 255u) ? 255u : (uint8_t)bmax;
            if (bri > 10u) {
                uint8_t hue = (uint8_t)((uint16_t)(t * 11u) + (uint8_t)((unsigned)c * 7u) + (uint8_t)((unsigned)r * 23u));
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB(CHSV(hue, 255, bri)));
            } else {
                side.setPixel(SEG_TOP_STRIP, r, c, CRGB::Black);
            }
        }
    }
    uint8_t rb = (uint8_t)((sin8((uint8_t)((uint16_t)(big * 3u)))  >> 3u)
               + (sin8((uint8_t)((uint16_t)(big * 17u))) >> 4u));
    side.fillSegment(SEG_BOT_STRIP, CRGB(rb, 0, 0));
    side.fillSegment(SEG_MAIN,      CRGB(rb, 0, 0));
}
