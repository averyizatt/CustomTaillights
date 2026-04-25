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
    static AnimBrake      _brake;
    static AnimTurnSignal _turnDriver;
    static AnimTurnSignal _turnPassenger;
    static AnimReverse    _reverse;
    static AnimHazard     _hazard;
    static AnimScrollText _scrollText;
    static AnimFlash      _flash;
    static CustomSlot     _customSlot;
};
