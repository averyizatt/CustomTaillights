#pragma once

// ---------------------------------------------------------------------------
// states.h
// Per-side light state enum and resolver.
//
// With two independent 4-channel optocouplers (one per physical taillight),
// each side is resolved separately from its own four bulb inputs.
// Hazard is detected when BOTH sides' turn-signal bulbs are active at once.
// ---------------------------------------------------------------------------

// States that apply to one individual side.
enum class LightState : uint8_t {
    OFF        = 0,  // nothing active
    RUNNING    = 1,  // dim red — parking / running light
    BRAKE      = 2,  // bright red — brake
    TURN       = 3,  // amber blink — this side's turn signal
    REVERSE    = 4,  // white — reverse
    BRAKE_TURN = 5,  // brake + turn active on the same side
    HAZARD     = 6,  // both sides turning simultaneously (amber blink)
    CUSTOM     = 7,  // CAN-commanded custom animation (see can_protocol.h Cmd 0x04)
};

// ---------------------------------------------------------------------------
// resolveSideState()
// Derive the LightState for ONE side from that side's four raw boolean inputs
// plus the OTHER side's turn signal so hazard can be detected.
//
// Priority (highest → lowest):
//   HAZARD > BRAKE_TURN > BRAKE > TURN > REVERSE > RUNNING > OFF
// ---------------------------------------------------------------------------
inline LightState resolveSideState(bool brake, bool running,
                                   bool turn,  bool reverse,
                                   bool otherTurn)
{
    if (turn && otherTurn) return LightState::HAZARD;
    if (brake && turn)     return LightState::BRAKE_TURN;
    if (brake)             return LightState::BRAKE;
    if (turn)              return LightState::TURN;
    if (reverse)           return LightState::REVERSE;
    if (running)           return LightState::RUNNING;
    return LightState::OFF;
}
