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
// AnimationRegistry
// Maps a LightState to the correct Animation instance.
// ---------------------------------------------------------------------------
class AnimationRegistry {
public:
    // Called once in setup() to create all animation instances.
    static void init();

    // Return the animation that should play for `state` on the given side.
    // `isLeft` lets directional animations mirror for the right side.
    static Animation* get(LightState state, bool isLeft);

private:
    static AnimOff        _off;
    static AnimRunning    _running;
    static AnimBrake      _brake;
    static AnimTurnSignal _turnLeft;
    static AnimTurnSignal _turnRight;
    static AnimReverse    _reverse;
    static AnimHazard     _hazard;
};
