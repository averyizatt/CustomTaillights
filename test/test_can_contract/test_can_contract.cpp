#include <cassert>
#include <cstdio>
#include <limits>

#include "can_state.h"

#ifdef NDEBUG
#error "CAN contract tests require assertions enabled"
#endif

static void test_state_matches_ccm_contract() {
    // Deliberately distinct values catch reordered bytes in the 0x100 payload.
    const auto frame = taillight_can::encodeState(1, 4, 0xAD, 203, 74.5f, 64);
    const uint8_t expected[] = {1, 4, 0xAD, 203, 115, 25, 0};
    assert(frame.id == 0x100);
    assert(frame.dlc == sizeof(expected));
    for (unsigned i = 0; i < sizeof(expected); ++i) {
        assert(frame.data[i] == expected[i]);
    }

    // Use the unchanged decoder used by CCM, not a test-specific decoder.
    can_protocol::TaillightState received;
    assert(can_protocol::unpackTaillightState(frame, received));
    assert(received.left_state == 1);
    assert(received.right_state == 4);
    assert(received.input_flags == 0xAD);
    assert(received.brightness == 203);
    assert(received.die_temp_c == 75);
    assert(received.thermal_derate == 25);
    assert(received.status_flags == 0);
}

static void test_temperature_rounding_and_range() {
    const struct {
        float measured;
        int expected;
    } cases[] = {
        {-1000.0f, -40}, {-40.0f, -40}, {-39.5f, -40}, {-39.4f, -39},
        {-1.5f, -2}, {-0.5f, -1}, {0.0f, 0}, {74.4f, 74}, {74.5f, 75},
        {126.5f, 127}, {127.0f, 127}, {1000.0f, 127},
        {std::numeric_limits<float>::quiet_NaN(), -40},
        {std::numeric_limits<float>::infinity(), -40},
        {-std::numeric_limits<float>::infinity(), -40},
    };
    for (const auto& item : cases) {
        const auto frame = taillight_can::encodeState(0, 0, 0, 0, item.measured, 0);
        assert(frame.data[4] == item.expected + 40);
        can_protocol::TaillightState received;
        assert(can_protocol::unpackTaillightState(frame, received));
        assert(received.die_temp_c == item.expected);
    }
}

static void test_derating_is_percentage() {
    const struct {
        uint8_t scale255;
        uint8_t percent;
    } cases[] = {
        {0, 0}, {1, 0}, {2, 1}, {64, 25}, {127, 50},
        {128, 50}, {191, 75}, {254, 100}, {255, 100},
    };
    for (const auto& item : cases) {
        const auto frame = taillight_can::encodeState(0, 0, 0, 255, 25.0f, item.scale255);
        can_protocol::TaillightState received;
        assert(can_protocol::unpackTaillightState(frame, received));
        assert(received.thermal_derate == item.percent);
    }
}

static void test_brightness_command_persists() {
    taillight_can::BrightnessOverride brightness;
    assert(brightness.requested(128) == 128);
    assert(brightness.requested(200) == 200);

    const auto command = can_protocol::packTaillightBrightness(73);
    assert(command.id == 0x101);
    assert(command.dlc == 2);
    assert(command.data[0] == 0x01);
    brightness.set(command.data[1]);

    // Repeated firmware loops and changes to saved brightness cannot erase CAN's value.
    for (unsigned loop = 0; loop < 1024; ++loop) {
        assert(brightness.requested(static_cast<uint8_t>(loop)) == 73);
    }
    brightness.set(0);
    assert(brightness.requested(255) == 0);
    assert(brightness.requested(128) == 0);
    brightness.set(255);
    assert(brightness.requested(0) == 255);
    brightness.set(42);
    assert(brightness.requested(128) == 42);

    taillight_can::BrightnessOverride afterReboot;
    assert(afterReboot.requested(128) == 128);
}

int main() {
    test_state_matches_ccm_contract();
    test_temperature_rounding_and_range();
    test_derating_is_percentage();
    test_brightness_command_persists();
    std::puts("CAN contract tests passed");
}
