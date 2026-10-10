#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// hw_watchdog.h
// The chip's RTC watchdog, used as an automatic reset button. It runs from its
// own clock and resets the whole chip, so it still works when the processor,
// the scheduler or the task watchdog do not. The firmware feeds it only when
// frames have actually reached both lamps (output_guard.h).
//
// Two stages: a system reset first; if that does not bring the firmware back
// to feeding it, a reset that also restarts the RTC, as the reset button does.
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void hwWatchdogBegin(uint32_t timeoutMs);
void hwWatchdogFeed(void);
// Full reset within a few tens of milliseconds. Does not return.
void hwWatchdogResetNow(void);

#ifdef __cplusplus
}
#endif
