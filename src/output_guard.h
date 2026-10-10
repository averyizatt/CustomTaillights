#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// output_guard.h
// Watches that frames really reach both lamps. The LED transport can stop
// delivering while the rest of the controller keeps running (a transfer that
// never completes blocks every later frame), which leaves the lamps frozen on
// their last picture with every software watchdog still satisfied.
//
//   delivered  both lamps have had a frame since the last time this was true:
//              the only thing that feeds the hardware watchdog.
//   stalled    a lamp has had frames attempted and refused for long enough that
//              it is not a passing hiccup: reset now instead of waiting.
//
// Counting refusals (not time alone) keeps a slow flash write or CAN start,
// during which no frame is attempted, from looking like a stall.
// ---------------------------------------------------------------------------
struct OutputGuard {
    struct Lamp {
        uint32_t      completed = 0;
        uint32_t      failures = 0;
        uint32_t      failedSinceOk = 0;
        unsigned long lastOkMs = 0;
        bool          fresh = false;
    };
    Lamp lamps[2];
    bool started = false;

    bool update(const uint32_t completed[2], const uint32_t failures[2], unsigned long nowMs) {
        if (!started) {
            for (int i = 0; i < 2; ++i) {
                lamps[i].completed = completed[i];
                lamps[i].failures = failures[i];
                lamps[i].lastOkMs = nowMs;
            }
            started = true;
            return false;
        }
        for (int i = 0; i < 2; ++i) {
            Lamp& lamp = lamps[i];
            lamp.failedSinceOk += failures[i] - lamp.failures;
            lamp.failures = failures[i];
            if (completed[i] != lamp.completed) {
                lamp.completed = completed[i];
                lamp.lastOkMs = nowMs;
                lamp.failedSinceOk = 0;
                lamp.fresh = true;
            }
        }
        if (!(lamps[0].fresh && lamps[1].fresh)) return false;
        lamps[0].fresh = lamps[1].fresh = false;
        return true;
    }

    bool stalled(unsigned long nowMs, unsigned long stallMs, uint32_t minFailures) const {
        if (!started) return false;
        for (const Lamp& lamp : lamps) {
            if (lamp.failedSinceOk >= minFailures && nowMs - lamp.lastOkMs >= stallMs) return true;
        }
        return false;
    }
};
