// RTC watchdog through the same HAL calls ESP-IDF's own restart path uses.
// Kept in C: the low-level watchdog headers are written for C.
#include "hw_watchdog.h"

#include <stdbool.h>
#include "hal/wdt_hal.h"
#include "soc/rtc.h"

static wdt_hal_context_t s_rwdt;
static bool s_started = false;
static const uint32_t SECOND_STAGE_MS = 3000;

static uint32_t ticksFor(uint32_t ms) {
    // The RTC watchdog counts ticks of the RTC slow clock.
    return (uint32_t)((uint64_t)ms * rtc_clk_slow_freq_get_hz() / 1000ULL);
}

static void configure(uint32_t firstMs, uint32_t secondMs) {
    wdt_hal_init(&s_rwdt, WDT_RWDT, 0, false);
    wdt_hal_write_protect_disable(&s_rwdt);
    wdt_hal_config_stage(&s_rwdt, WDT_STAGE0, ticksFor(firstMs), WDT_STAGE_ACTION_RESET_SYSTEM);
    wdt_hal_config_stage(&s_rwdt, WDT_STAGE1, ticksFor(secondMs), WDT_STAGE_ACTION_RESET_RTC);
    wdt_hal_enable(&s_rwdt);
    wdt_hal_feed(&s_rwdt);
    wdt_hal_write_protect_enable(&s_rwdt);
    s_started = true;
}

void hwWatchdogBegin(uint32_t timeoutMs) {
    configure(timeoutMs, SECOND_STAGE_MS);
}

void hwWatchdogFeed(void) {
    if (!s_started) return;
    wdt_hal_write_protect_disable(&s_rwdt);
    wdt_hal_feed(&s_rwdt);
    wdt_hal_write_protect_enable(&s_rwdt);
}

void hwWatchdogResetNow(void) {
    configure(40, 200);
    for (;;) { }
}
