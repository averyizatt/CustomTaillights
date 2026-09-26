#include <cassert>
#include <cstdio>
#include <limits>

#include "can_state.h"

#ifdef NDEBUG
#error "CAN contract tests require assertions enabled"
#endif

static void test_state_matches_ccm_contract() {
    // Deliberately distinct values catch reordered bytes in the 0x100 payload.
    const auto frame = taillight_can::encodeState(1, 4, 0xAD, 0x02, 203, 74.5f, 64);
    const uint8_t expected[] = {1, 4, 0xAD, 0x02, 203, 75, 64};
    assert(frame.id == 0x100);
    assert(frame.dlc == sizeof(expected));
    for (unsigned i = 0; i < sizeof(expected); ++i) {
        assert(frame.data[i] == expected[i]);
    }

    // Use the schema 2 decoder used by CCM, not a test-specific decoder.
    can_protocol::TaillightState received;
    assert(can_protocol::unpackTaillightState(frame, received));
    assert(received.left_state == 1);
    assert(received.right_state == 4);
    assert(received.input_flags == 0xAF);
    assert(received.brightness == 203);
    assert(received.die_temp_c == 75);
    assert(received.thermal_derate == 64);
    assert(received.driver_input_flags == 0xAD);
    assert(received.passenger_input_flags == 0x02);
}

static void test_temperature_rounding_and_range() {
    const struct {
        float measured;
        int expected;
    } cases[] = {
        {-1000.0f, 0}, {-40.0f, 0}, {-39.5f, 0}, {-39.4f, 0},
        {-1.5f, 0}, {-0.5f, 0}, {0.0f, 0}, {74.4f, 74}, {74.5f, 75},
        {126.5f, 127}, {127.0f, 127}, {1000.0f, 255},
        {std::numeric_limits<float>::quiet_NaN(), 0},
        {std::numeric_limits<float>::infinity(), 0},
        {-std::numeric_limits<float>::infinity(), 0},
    };
    for (const auto& item : cases) {
        const auto frame = taillight_can::encodeState(0, 0, 0, 0, 0, item.measured, 0);
        assert(frame.data[5] == item.expected);
        can_protocol::TaillightState received;
        assert(can_protocol::unpackTaillightState(frame, received));
        assert(received.die_temp_c == item.expected);
    }
}

static void test_derating_is_raw() {
    const uint8_t cases[] = {0, 1, 2, 64, 127, 128, 191, 254, 255};
    for (const auto amount : cases) {
        const auto frame = taillight_can::encodeState(0, 0, 0, 0, 255, 25.0f, amount);
        can_protocol::TaillightState received;
        assert(can_protocol::unpackTaillightState(frame, received));
        assert(received.thermal_derate == amount);
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
    test_derating_is_raw();
    test_brightness_command_persists();
    std::puts("CAN contract tests passed");
}
