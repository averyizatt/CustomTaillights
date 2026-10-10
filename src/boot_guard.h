#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// boot_guard.h
// What a boot is allowed to do, decided from why the controller restarted and
// how many restarts in a row were faults. Pure logic: the record lives in RTC
// memory (kept across every reset except loss of power) and is owned by main.cpp.
//
//  - A fault restart (watchdog, crash, stalled LED output) goes straight back to
//    the lights: no crank holdoff and no startup animation, so the brake lights
//    return in a fraction of a second.
//  - A brownout keeps the holdoff (the supply is not steady) but never replays
//    the startup animation, which is the heaviest load the lights ever draw.
//  - Three fault restarts in a row select safe mode: a much lower LED current
//    limit, so a supply that cannot carry the lights gives dim lights, not a
//    reset loop. A stretch of normal running clears the count.
// ---------------------------------------------------------------------------
namespace boot_guard {

enum class Cause : uint8_t { PowerOn, Software, Brownout, Watchdog, Panic, OutputStall };

struct Record {
    uint32_t magic;
    uint8_t  streak;    // Fault restarts in a row
    uint8_t  pending;   // PENDING_OUTPUT while a reset for a stalled LED output is under way
    uint8_t  check;
    uint8_t  reserved;
};

struct Plan {
    Cause   cause;             // As given, or OutputStall when the record says so
    bool    crankHoldoff;
    bool    startupAnimation;
    bool    safeMode;
    bool    quickRestart;      // A stalled output may force a reset at once (else the 5 s hardware watchdog does)
    uint8_t streak;
};

static constexpr uint32_t MAGIC = 0x54474431;  // "TGD1"
static constexpr uint8_t  PENDING_OUTPUT = 0x5A;
static constexpr uint8_t  SAFE_MODE_STREAK = 3;

inline uint8_t checkOf(const Record& r) { return static_cast<uint8_t>(r.streak ^ r.pending ^ 0xA5); }
inline bool valid(const Record& r) { return r.magic == MAGIC && r.check == checkOf(r); }
inline void seal(Record& r) { r.magic = MAGIC; r.check = checkOf(r); }
inline void clear(Record& r) { r.streak = 0; r.pending = 0; r.reserved = 0; seal(r); }

// Called just before the reset that a stalled LED output asks for.
inline void markOutputStall(Record& r) {
    if (!valid(r)) clear(r);
    r.pending = PENDING_OUTPUT;
    seal(r);
}

inline Plan onBoot(Record& r, Cause cause, bool startupAnimationSetting) {
    // RTC memory holds noise after power is applied, and a power-on is a fresh start anyway.
    if (!valid(r) || cause == Cause::PowerOn) {
        clear(r);
        return {Cause::PowerOn, true, startupAnimationSetting, false, true, 0};
    }
    if (cause == Cause::Watchdog && r.pending == PENDING_OUTPUT) cause = Cause::OutputStall;
    r.pending = 0;
    if (cause == Cause::Software) {  // A deliberate restart is not a fault.
        seal(r);
        return {cause, true, startupAnimationSetting, r.streak >= SAFE_MODE_STREAK, r.streak < SAFE_MODE_STREAK, r.streak};
    }
    if (r.streak < 255) ++r.streak;
    seal(r);
    const bool safe = r.streak >= SAFE_MODE_STREAK;
    return {cause, cause == Cause::Brownout, false, safe, !safe, r.streak};
}

// Called once the lights have run normally for a while.
inline void onStable(Record& r) {
    if (!valid(r)) { clear(r); return; }
    r.streak = 0;
    r.pending = 0;
    seal(r);
}

} // namespace boot_guard
