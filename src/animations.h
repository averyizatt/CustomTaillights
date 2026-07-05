#pragma once

// ---------------------------------------------------------------------------
// animations.h
// Base class for all taillight animations and a global registry.
//
// To add a new animation:
//   1. Create a class that inherits Animation.
//   2. Override begin(), update(), and (optionally) end().
//   3. Register an instance in AnimationRegistry::init() inside animations.cpp.
// ---------------------------------------------------------------------------

#include <FastLED.h>
#include "states.h"
#include "font5x.h"

// Forward declaration
class TailLight;

// ---------------------------------------------------------------------------
// Animation — abstract base
// ---------------------------------------------------------------------------
class Animation {
public:
    virtual ~Animation() = default;

    // Called once when this animation becomes active.
    // `side` is provided so an animation can adapt per-side if needed.
    virtual void begin(TailLight& side, LightState state) {}

    // Called every FRAME_INTERVAL_MS while this animation is active.
    // Write LED colours directly into the TailLight's pixel buffer.
    virtual void update(TailLight& side, LightState state, unsigned long nowMs) = 0;

    // Called once when this animation is deactivated (state changed).
    virtual void end(TailLight& side) {}
};

// ---------------------------------------------------------------------------
// Built-in animations (defined in animations.cpp)
// ---------------------------------------------------------------------------

// All LEDs off
class AnimOff : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Solid dim red — running / parking lights
class AnimRunning : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Solid bright red — brake
class AnimBrake : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Sequential amber sweep — turn signal (left or right)
class AnimTurnSignal : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
    void end(TailLight& side) override;
private:
    unsigned long _startMs = 0;
    int           _step    = 0;
};

// Solid white — reverse
class AnimReverse : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Simultaneous amber flash — hazard
class AnimHazard : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
};

// ---------------------------------------------------------------------------
// Brake variants  (brake_anim = 1 / 2 / 3)
// ---------------------------------------------------------------------------

// Smooth sin-wave breathe — brake colour fades in and out (~1.2 s period)
class AnimBrakePulse : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Center-out sweep (~400 ms) then holds solid
class AnimBrakeCenterOut : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
    bool          _done    = false;
};

// Rapid 8 Hz strobe (125 ms period, 50 % duty)
class AnimBrakeStrobe : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// Turn-signal variants  (turn_anim = 1 / 2 / 3 — one D/P pair each)
// ---------------------------------------------------------------------------

// Simple whole-panel on/off flash at turn_blink_ms period
class AnimTurnSimple : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
};

// 4-column groups that sweep outward then repeat
class AnimTurnGroupChase : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
};

// Bouncing comet during the "on" half; black during the "off" half
class AnimTurnBounce : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
};

// ---------------------------------------------------------------------------
// Reverse variant  (reverse_anim = 1)
// ---------------------------------------------------------------------------

// Smooth breathing pulse with reverse colour
class AnimReversePulse : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// Running-light variant  (run_anim = 1)
// ---------------------------------------------------------------------------

// Slow breathing pulse with running colour
class AnimRunBreathe : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// Extra brake variants  (brake_anim = 4 / 5)
// ---------------------------------------------------------------------------

// Outer-to-inner sweep (~400 ms) — fills from outer edges toward centre then holds
class AnimBrakeOuterIn : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
    bool          _done    = false;
};

// Lub-dub heartbeat double-pulse repeating at ~800 ms
class AnimBrakeHeartbeat : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// Extra turn-signal variants  (turn_anim = 4 / 5 — one D/P pair each)
// ---------------------------------------------------------------------------

// Split outward — two sweeps start from centre and race to outer edges
class AnimTurnSplitOut : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
};

// Fast chase — three rapid sweeps per blink on-half
class AnimTurnFastChase : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _startMs = 0;
};

// ---------------------------------------------------------------------------
// Extra reverse variants  (reverse_anim = 2 / 3)
// ---------------------------------------------------------------------------

// Sparkle — scattered pixel bursts with the reverse colour
class AnimReverseSparkle : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Scanner — slow-moving bright column in the reverse colour
class AnimReverseScanner : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// Extra running-light variants  (run_anim = 2 / 3)
// ---------------------------------------------------------------------------

// Shimmer — subtle per-pixel brightness variation across the panel
class AnimRunShimmer : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Slow Comet — very dim wandering comet on the running colour
class AnimRunComet : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// Show Mode animations  (LightState::SHOW, show_anim = 0-7)
//
// Hardware note: SEG_TOP_STRIP has a clear diffuser (full colour shows).
// SEG_BOT_STRIP and SEG_MAIN have red diffusers (only the red channel passes).
// Show animations are designed accordingly: full RGB on the top strip and
// red-intensity only on the bottom strip + main panel.
// ---------------------------------------------------------------------------

// Scrolling full-spectrum hue rainbow on the top strip; red pulse on the rest
class AnimShowRainbow : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Bright comet with fading tail racing across the top strip; driver forward,
// passenger mirrored (via isDriver())
class AnimShowChase : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Marching every-third-pixel pattern (top strip full colour, rest red)
class AnimShowTheater : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Flickering fire effect — bottom / main red flicker, top strip orange/yellow
class AnimShowFire : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _lastFrameMs = 0;
    uint8_t       _heat[21]    = {};   // one heat cell per top-strip column
};

// Meteor shower — white streaks across the top strip, dim red elsewhere
class AnimShowMeteor : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Alternating red / blue strobe — driver side = red, passenger side = blue
class AnimShowPolice : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// KITT scanner — bright comet bouncing left ↔ right on the top strip
class AnimShowNightRider : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// Smooth full-panel hue rotation (top strip full colour, rest red intensity)
class AnimShowColorCycle : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 8. Sparkle — random hue pixels burst on the top strip; background dark
class AnimShowSparkle : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 9. Plasma — sine-wave interference field of flowing rainbow hues
class AnimShowPlasma : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 10. Matrix Rain — green digital-rain columns falling on the top strip
class AnimShowMatrix : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 11. Juggle — five colored dots bouncing back and forth with fade halos
class AnimShowJuggle : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 12. BPM Bars — columns fill from the bottom like a spectrum analyzer bar graph
class AnimShowBPM : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 13. Confetti — bright random-colored sparks scatter over a near-black base
class AnimShowConfetti : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 14. Ocean Waves — slow blue/teal sine waves across the top strip
class AnimShowOcean : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 15. Lightning — dramatic random white flashes with dark pauses
//     Stateful: separate timing state for driver and passenger sides
class AnimShowLightning : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    unsigned long _nextFlash[2] = {};  // [0] = driver, [1] = passenger
    unsigned long _flashEnd[2]  = {};
    int8_t        _strikeCol[2] = {};
};

// 16. Heartbeat — double-pulse cardiac rhythm in red
class AnimShowHeartbeat : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 17. Ripple — concentric brightness rings expanding outward from the center
class AnimShowRipple : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 18. Sunrise — warm color gradient cycling dark-red → orange → yellow → back
class AnimShowSunrise : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 19. Text Scroll — scrolls g_settings.show_text across SEG_TOP_STRIP using
//     the built-in 5×5 pixel font.  Both sides scroll in sync from nowMs.
class AnimShowText : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    uint8_t _colBuf[512]    = {};
    int     _textCols       = 0;
    char    _cachedText[64] = {};
};

// ---------------------------------------------------------------------------
// WLED-inspired show animations (effects 20-24)
// ---------------------------------------------------------------------------

// 20. Colorwaves (WLED) — two beatsin8 waves interfere to create silky colour
//     bands that flow across the top strip; bot/main pulse with red intensity.
class AnimShowColorwaves : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 21. TwinkleFox (WLED) — each column independently twinkles in/out using a
//     stable PRNG so the pattern is repeatable yet always looks alive.
class AnimShowTwinkleFox : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 22. Bouncing Balls (WLED) — five hue-cycling balls launched under gravity,
//     bouncing with energy loss until relaunched.  Per-side state.
class AnimShowBouncingBalls : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    static constexpr int NUM_BALLS = 5;
    float         _impactVel[NUM_BALLS]    = {};
    unsigned long _lastBounceMs[NUM_BALLS] = {};
    uint8_t       _hue[NUM_BALLS]          = {};
    bool          _inited                  = false;
};

// 23. Fireworks (WLED) — a flare launches across the top strip, bursts into
//     colour sparks that spread and fade.  Per-side state.
class AnimShowFireworks : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    static constexpr int MAX_SPARKS = 20;
    struct Spark { float pos; float vel; uint8_t hue; uint8_t life; };
    Spark         _sparks[MAX_SPARKS] = {};
    float         _flarePos  = 0.0f;
    float         _flareVel  = 0.0f;
    uint8_t       _flareHue  = 0;
    uint8_t       _phase     = 2;   // 0=flare, 1=burst, 2=idle
    unsigned long _idleUntil = 0;
    unsigned long _lastMs    = 0;
};

// 24. Drip (WLED) — drops swell at the top, fall under gravity, bounce once
//     at the bottom then reset.  Per-side state.
class AnimShowDrip : public Animation {
public:
    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
private:
    static constexpr int MAX_DROPS = 2;
    struct Drop {
        float   pos;       // column position 0=left/bottom … STRIP_COLS-1=right/top
        float   vel;       // signed velocity (col/s; negative = falling)
        uint8_t col;       // brightness 0–255
        uint8_t colIndex;  // state: 0=init, 1=forming, 2=falling, 5=bouncing
        uint8_t hue;
    };
    Drop          _drops[MAX_DROPS] = {};
    unsigned long _lastMs           = 0;
};

// ---------------------------------------------------------------------------
// Car-themed / premium show animations (effects 25-32)
// ---------------------------------------------------------------------------

// 25. Cylon Dual — two opposing coloured scanners converge to centre, flash
//     on impact, and diverge again.  Hue cycles slowly.
class AnimShowCylonDual : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 26. V8 Fire — eight column groups flash in the classic V8 firing order
//     (1-8-4-3-6-5-7-2).  Bot/main pulses red on each cylinder fire.
class AnimShowV8 : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 27. Drag Launch — staging burnout glow, three amber countdown steps,
//     full green GO blast, then twin comets race outward.  ~3 s cycle.
class AnimShowDragLaunch : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 28. Neon Glow — high-saturation, slowly cycling hue wash across the top
//     strip (tuner-car underbody neon look); bot/main pulse red in sync.
class AnimShowNeon : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 29. Speed Streaks — four short bright comets shoot across the top strip at
//     different speeds and directions, like motion-blur racing speed lines.
class AnimShowSpeedStreaks : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 30. Radar Sweep — a single bright column sweeps left-to-right with a
//     decaying trail, radar-scope style.  Hue cycles slowly.
class AnimShowRadar : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 31. Aurora — three overlapping sine-wave layers in blue-green / cyan /
//     teal hues, brightest at the top row.  Near-black through red diffuser.
class AnimShowAurora : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// 32. Glitch — digital glitch: mostly dark with random full-saturation colour
//     blocks popping on and off in irregular bursts.  Cyberpunk aesthetic.
class AnimShowGlitch : public Animation {
public:
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
};

// ---------------------------------------------------------------------------
// AnimScrollText — scrolls a short message across SEG_TOP_STRIP (non-blocking)
// Configure before activating via set(), then set state to CUSTOM.
// Automatically signals isDone() when the full text has scrolled off.
// ---------------------------------------------------------------------------
class AnimScrollText : public Animation {
public:
    // Call before the state switches to CUSTOM.
    //   text      — null-terminated string (copied internally, max 63 chars)
    //   fgColour  — text pixel colour (appears true on clear diffuser)
    //   bgColour  — fill colour for red-diffuser segments
    //   scrollMs  — ms per column step (lower = faster)
    void set(const char* text,
             CRGB        fgColour  = CRGB(220, 220, 220),
             CRGB        bgColour  = CRGB(30, 0, 0),
             int         scrollMs  = 45);

    bool isDone() const { return _done; }

    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
    void end(TailLight& side) override;

private:
    char    _text[64]     = {};
    CRGB    _fg           = CRGB(220, 220, 220);
    CRGB    _bg           = CRGB(30, 0, 0);
    int     _scrollMs     = 45;

    uint8_t _colBuf[512]  = {};
    int     _textCols     = 0;
    int     _offset       = 0;      // current scroll position
    unsigned long _lastStepMs = 0;
    bool    _done         = false;
};

// ---------------------------------------------------------------------------
// AnimFlash — rapidly flashes all segments a set number of times (non-blocking)
// Configure before activating via set(), then set state to CUSTOM.
// ---------------------------------------------------------------------------
class AnimFlash : public Animation {
public:
    // flashCount : total on+off pairs  (default 3)
    // halfPeriodMs : duration of each on half and each off half (default 150 ms)
    // colour : flash colour (red diffuser segments will show only the R channel)
    void set(uint8_t flashCount   = 3,
             uint16_t halfPeriodMs = 150,
             CRGB colour          = CRGB(255, 140, 0));

    bool isDone() const { return _done; }

    void begin(TailLight& side, LightState state) override;
    void update(TailLight& side, LightState state, unsigned long nowMs) override;
    void end(TailLight& side) override;

private:
    uint8_t  _flashCount   = 3;
    uint16_t _halfPeriodMs = 150;
    CRGB     _colour       = CRGB(255, 140, 0);

    unsigned long _startMs = 0;
    bool          _done    = false;
};

// ---------------------------------------------------------------------------
// AnimationRegistry
// Maps a LightState to the correct Animation instance.
// ---------------------------------------------------------------------------
class AnimationRegistry {
public:
    // Called once in setup() to create all animation instances.
    static void init();

    // Return the animation that should play for `state` on the given side.
    // `isDriver` lets directional animations mirror for the right side.
    static Animation* get(LightState state, bool isDriver);

    // ── Custom animation access ─────────────────────────────────────────────
    // Configure the scroll-text animation then trigger it by pushing
    // LightState::CUSTOM through the override mechanism.
    static AnimScrollText& scrollText() { return _scrollText; }
    static AnimFlash&      flash()      { return _flash; }

    // Which custom animation is currently active for CUSTOM state.
    // Set by CANBus when it receives Cmd 0x04.
    enum class CustomSlot { NONE, SCROLL_TEXT, FLASH };
    static void       setCustomSlot(CustomSlot s) { _customSlot = s; }
    static CustomSlot customSlot()                 { return _customSlot; }

private:
    static AnimOff        _off;
    static AnimRunning    _running;
    static AnimRunBreathe _runBreathe;
    static AnimBrake      _brake;
    static AnimBrakePulse     _brakePulse;
    static AnimBrakeCenterOut _brakeCenterOutD, _brakeCenterOutP;
    static AnimBrakeStrobe    _brakeStrobe;
    static AnimBrakeOuterIn   _brakeOuterInD, _brakeOuterInP;
    static AnimBrakeHeartbeat _brakeHeartbeat;
    static AnimTurnSignal _turnDriver;     // driver-side   sequential sweep
    static AnimTurnSignal _turnPassenger;  // passenger-side sequential sweep
    static AnimTurnSimple    _turnSimpleD,    _turnSimpleP;
    static AnimTurnGroupChase _turnGroupD,    _turnGroupP;
    static AnimTurnBounce    _turnBounceD,   _turnBounceP;
    static AnimTurnSplitOut  _turnSplitD,    _turnSplitP;
    static AnimTurnFastChase _turnFastD,     _turnFastP;
    static AnimReverse    _reverse;
    static AnimReversePulse   _reversePulse;
    static AnimReverseSparkle _reverseSparkle;
    static AnimReverseScanner _reverseScanner;
    static AnimHazard     _hazardD, _hazardP;
    static AnimScrollText _scrollText;
    static AnimFlash      _flash;
    static AnimRunShimmer _runShimmer;
    static AnimRunComet   _runComet;
    // ── Show mode ────────────────────────────────────────────────────────────
    static AnimShowRainbow    _showRainbow;
    static AnimShowChase      _showChase;
    static AnimShowTheater    _showTheater;
    static AnimShowFire       _showFireD,   _showFireP;
    static AnimShowMeteor     _showMeteor;
    static AnimShowPolice     _showPolice;
    static AnimShowNightRider _showNightRider;
    static AnimShowColorCycle _showColorCycle;
    static AnimShowSparkle    _showSparkle;
    static AnimShowPlasma     _showPlasma;
    static AnimShowMatrix     _showMatrix;
    static AnimShowJuggle     _showJuggle;
    static AnimShowBPM        _showBPM;
    static AnimShowConfetti   _showConfetti;
    static AnimShowOcean      _showOcean;
    static AnimShowLightning  _showLightning;
    static AnimShowHeartbeat  _showHeartbeat;
    static AnimShowRipple     _showRipple;
    static AnimShowSunrise        _showSunrise;
    static AnimShowText           _showText;
    static AnimShowColorwaves     _showColorwaves;
    static AnimShowTwinkleFox     _showTwinkleFox;
    static AnimShowBouncingBalls  _showBouncingBallsD, _showBouncingBallsP;
    static AnimShowFireworks      _showFireworksD,     _showFireworksP;
    static AnimShowDrip           _showDripD,          _showDripP;
    static AnimShowCylonDual      _showCylonDual;
    static AnimShowV8             _showV8;
    static AnimShowDragLaunch     _showDragLaunch;
    static AnimShowNeon           _showNeon;
    static AnimShowSpeedStreaks   _showSpeedStreaks;
    static AnimShowRadar          _showRadar;
    static AnimShowAurora         _showAurora;
    static AnimShowGlitch         _showGlitch;
    static CustomSlot         _customSlot;
};
