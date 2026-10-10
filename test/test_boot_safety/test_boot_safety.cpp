// Recovery safeguards: what a boot may do after a fault, when the lamps count as
// stalled, and how fast the LED current may rise. Pure logic, no hardware.
#include <cassert>
#include <cstring>
#include <iostream>
#include "boot_guard.h"
#include "output_guard.h"
#include "power_ramp.h"

using boot_guard::Cause;

static void test_power_on_is_a_fresh_start() {
    boot_guard::Record record;
    std::memset(&record, 0xC3, sizeof(record));            // RTC memory after power is applied: noise.
    auto plan = boot_guard::onBoot(record, Cause::Watchdog, true);
    assert(plan.cause == Cause::PowerOn && plan.crankHoldoff && plan.startupAnimation && !plan.safeMode && plan.streak == 0);
    assert(boot_guard::valid(record));
    plan = boot_guard::onBoot(record, Cause::PowerOn, false);
    assert(plan.crankHoldoff && !plan.startupAnimation);   // The owner switched the animation off.
}

static void test_a_fault_restart_goes_straight_back_to_the_lights() {
    boot_guard::Record record{};
    boot_guard::onBoot(record, Cause::PowerOn, true);
    for (Cause cause : {Cause::Watchdog, Cause::Panic}) {
        boot_guard::Record copy = record;
        const auto plan = boot_guard::onBoot(copy, cause, true);
        assert(plan.cause == cause && !plan.crankHoldoff && !plan.startupAnimation);
        assert(!plan.safeMode && plan.quickRestart && plan.streak == 1);
    }
    // A brownout waits for the supply but never replays the heaviest load.
    const auto plan = boot_guard::onBoot(record, Cause::Brownout, true);
    assert(plan.crankHoldoff && !plan.startupAnimation && plan.streak == 1);
}

static void test_three_fault_restarts_in_a_row_select_safe_mode_until_a_stable_run() {
    boot_guard::Record record{};
    boot_guard::onBoot(record, Cause::PowerOn, true);
    assert(!boot_guard::onBoot(record, Cause::Brownout, true).safeMode);
    assert(!boot_guard::onBoot(record, Cause::Brownout, true).safeMode);
    auto plan = boot_guard::onBoot(record, Cause::Brownout, true);
    assert(plan.safeMode && plan.streak == 3);
    assert(!plan.quickRestart);                            // No more quick resets: the 5 s hardware watchdog only.
    plan = boot_guard::onBoot(record, Cause::Software, true);
    assert(plan.safeMode && plan.streak == 3);             // A plain restart neither adds to nor clears the count.
    boot_guard::onStable(record);
    plan = boot_guard::onBoot(record, Cause::Watchdog, true);
    assert(!plan.safeMode && plan.streak == 1);
    for (int i = 0; i < 400; ++i) boot_guard::onBoot(record, Cause::Panic, true);
    assert(boot_guard::onBoot(record, Cause::Panic, true).streak == 255);   // Saturates, never wraps to "healthy".
}

static void test_a_reset_for_a_stalled_output_is_named_on_the_next_boot() {
    boot_guard::Record record{};
    boot_guard::onBoot(record, Cause::PowerOn, true);
    boot_guard::markOutputStall(record);
    auto plan = boot_guard::onBoot(record, Cause::Watchdog, true);
    assert(plan.cause == Cause::OutputStall && !plan.crankHoldoff && !plan.startupAnimation);
    plan = boot_guard::onBoot(record, Cause::Watchdog, true);
    assert(plan.cause == Cause::Watchdog);                  // The mark is used once.
    record.streak ^= 0x10;                                  // A corrupted record is not trusted.
    assert(!boot_guard::valid(record));
    assert(boot_guard::onBoot(record, Cause::Panic, true).streak == 0);
}

static void test_frames_reaching_both_lamps_feed_the_watchdog() {
    OutputGuard guard;
    uint32_t done[2] = {0, 0}, failed[2] = {0, 0};
    assert(!guard.update(done, failed, 0));
    done[0] = 1;
    assert(!guard.update(done, failed, 25));                // One lamp is not enough.
    done[1] = 1;
    assert(guard.update(done, failed, 25));
    assert(!guard.update(done, failed, 30));                // Nothing new: no feed.
    done[0] = 2; done[1] = 2;
    assert(guard.update(done, failed, 50));
    done[0] = 3;                                            // Driver keeps going, passenger stops.
    for (unsigned long t = 75; t < 400; t += 25) { ++done[0]; ++failed[1]; assert(!guard.update(done, failed, t)); }
}

static void test_a_lamp_that_keeps_refusing_frames_is_a_stall_and_a_quiet_pause_is_not() {
    OutputGuard guard;
    uint32_t done[2] = {5, 5}, failed[2] = {0, 0};
    guard.update(done, failed, 0);
    assert(!guard.stalled(10000, 500, 8));                  // Before the first report nothing is judged... and after it,
    guard.update(done, failed, 4000);                       // four seconds with no frame attempted (a flash write) is not a stall.
    assert(!guard.stalled(4000, 500, 8));
    for (int i = 1; i <= 7; ++i) { ++failed[0]; ++failed[1]; guard.update(done, failed, 4000 + i * 25); }
    assert(!guard.stalled(6000, 500, 8));                   // Seven refusals: still below the count.
    ++failed[1];
    guard.update(done, failed, 4200);
    assert(guard.stalled(4600, 500, 8));                    // Eight refusals and no frame for over 500 ms.
    ++done[1];
    guard.update(done, failed, 4625);
    assert(!guard.stalled(9000, 500, 8));                   // One delivered frame clears it.
    // A short burst of refusals that recovers inside the time limit never trips it.
    OutputGuard burst;
    uint32_t d[2] = {0, 0}, f[2] = {0, 0};
    burst.update(d, f, 0);
    for (int i = 1; i <= 12; ++i) { ++f[0]; burst.update(d, f, i * 25); assert(!burst.stalled(i * 25, 500, 8)); }
    ++d[0];
    burst.update(d, f, 325);
    assert(!burst.stalled(2000, 500, 8));
}

static void test_led_current_rises_in_steps_and_falls_at_once() {
    const uint32_t fullRed = 760u * 255u;                   // Every LED red.
    assert(power_ramp::estimateMa(fullRed, 255) == 15200);
    assert(power_ramp::estimateMa(fullRed, 128) == 7629);
    power_ramp::Limiter ramp;
    // Dark to full brake at brightness 128: about 7.6 A, allowed 4 A per frame.
    const uint8_t first = ramp.apply(fullRed, 128, 4000, 12000);
    assert(first > 60 && first < 128);                      // On in the first frame, clearly lit.
    assert(ramp.lastMa <= 4000);
    assert(ramp.apply(fullRed, 128, 4000, 12000) == 128);   // Full brightness in the second.
    assert(ramp.lastMa == 7629);
    assert(ramp.apply(fullRed / 10, 128, 4000, 12000) == 128 && ramp.lastMa < 800);   // Falling is never limited.
    assert(ramp.apply(0, 128, 4000, 12000) == 128 && ramp.lastMa == 0);
    // A small rise passes untouched; a frame never goes fully dark because of the limit.
    assert(ramp.apply(fullRed / 4, 128, 4000, 12000) == 128);
    power_ramp::Limiter tiny;
    assert(tiny.apply(fullRed * 3, 255, 10, 12000) >= 1);
    // The remembered level never exceeds what the hard cap lets through.
    power_ramp::Limiter capped;
    capped.lastMa = 11000;
    capped.apply(fullRed * 3, 255, 4000, 12000);
    assert(capped.lastMa == 12000);
    // Safe mode: smaller steps.
    power_ramp::Limiter safe;
    int frames = 0;
    while (safe.apply(fullRed, 128, 1500, 4000) != 128 && frames < 20) ++frames;
    assert(frames >= 2 && frames <= 6);
}

int main() {
    test_power_on_is_a_fresh_start();
    test_a_fault_restart_goes_straight_back_to_the_lights();
    test_three_fault_restarts_in_a_row_select_safe_mode_until_a_stable_run();
    test_a_reset_for_a_stalled_output_is_named_on_the_next_boot();
    test_frames_reaching_both_lamps_feed_the_watchdog();
    test_a_lamp_that_keeps_refusing_frames_is_a_stall_and_a_quiet_pause_is_not();
    test_led_current_rises_in_steps_and_falls_at_once();
    std::cout << "Boot plan, output guard and current ramp tests passed\n";
}
