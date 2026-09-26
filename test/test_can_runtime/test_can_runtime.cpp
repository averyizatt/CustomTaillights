#include <cassert>
#include <iostream>
#include "canbus.h"
#include "settings.h"

Settings g_settings;
unsigned long testMillis = 0;
// CAN command dependencies only; transport and command parsing are real code.
AnimScrollText AnimationRegistry::_scrollText;
AnimFlash AnimationRegistry::_flash;
AnimationRegistry::CustomSlot AnimationRegistry::_customSlot = AnimationRegistry::CustomSlot::NONE;
void AnimScrollText::set(const char*, CRGB, CRGB, int) {}
void AnimScrollText::begin(TailLight&, LightState) {}
void AnimScrollText::update(TailLight&, LightState, unsigned long) {}
void AnimScrollText::end(TailLight&) {}
void AnimFlash::set(uint8_t, uint16_t, CRGB) {}
void AnimFlash::begin(TailLight&, LightState) {}
void AnimFlash::update(TailLight&, LightState, unsigned long) {}
void AnimFlash::end(TailLight&) {}

void resetFake() {
    testMillis = 0;
    std::fill(std::begin(registers), std::end(registers), 0);
    rejectOneShot = false;
    MCP2515::resets = 0;
    MCP2515::resetResult = MCP2515::sendResult = MCP2515::ERROR_OK;
    MCP2515::errors = 0;
    MCP2515::sent.clear(); MCP2515::incoming.clear(); Serial.lines.clear();
}
void tick(CANBus& bus, unsigned long now) {
    Inputs inputs;
    ThermalManager thermal;
    testMillis = now;
    bus.tick(LightState::RUNNING, LightState::RUNNING, inputs, thermal, 128);
    assert(testMillis == now); // the runtime path never advances the clock
}
void test_missing_controller_stays_quiet() {
    resetFake(); MCP2515::resetResult = MCP2515::ERROR_FAIL;
    CANBus bus;
    assert(!bus.begin());
    const auto logs = Serial.lines.size();
    for (unsigned long t = 0; t < 60000; t += 2) tick(bus, t);
    assert(MCP2515::resets == 1 && MCP2515::sent.empty());
    assert(Serial.lines.size() == logs);
}
void test_disconnected_bus_and_reconnect() {
    resetFake(); CANBus bus; assert(bus.begin());
    assert((registers[0x0f] & 0xe8) == 0x08); // normal, one-shot
    for (unsigned long t = 0; t < 60000; t += 2) {
        tick(bus, t);
        if (registers[0x30] & 0x08) registers[0x30] = 0x50; // missing ACK
    }
    assert(MCP2515::sent.size() >= 5 && MCP2515::sent.size() <= 12);
    assert(MCP2515::resets == 1);
    assert(Serial.lines.size() == 2); // ready + one backoff message
    can_frame command;
    command.can_id = CAN_ID_COMMAND; command.can_dlc = 2;
    command.data[0] = 0x01; command.data[1] = 73;
    MCP2515::incoming.push_back(command);
    tick(bus, 60000);
    assert(bus.requestedBrightness(128) == 73); // RX alive during backoff
    for (unsigned long t = 60002; t < 72000; t += 2) {
        tick(bus, t);
        registers[0x30] = 0; // ACK/success
    }
    const auto count = MCP2515::sent.size();
    for (unsigned long t = 72000; t < 73000; t += 2) {
        tick(bus, t); registers[0x30] = 0;
    }
    assert(MCP2515::sent.size() == count + 10); // back to 100 ms telemetry
    assert(MCP2515::resets == 1);
}
void test_stuck_tx_faults_and_bus_off() {
    resetFake(); CANBus bus; assert(bus.begin());
    tick(bus, 100); assert(registers[0x30] & 0x08);
    bus.reportFault(1, 1); assert(MCP2515::sent.size() == 1); // pending slot
    tick(bus, 122); assert(!(registers[0x30] & 0x08)); // timeout abort
    bus.reportFault(1, 1); assert(MCP2515::sent.size() == 1); // shared backoff
    MCP2515::errors = 0x20;
    for (unsigned long t = 200; t < 20000; t += 100) tick(bus, t);
    assert(MCP2515::sent.size() == 1 && MCP2515::resets == 1);
    MCP2515::errors = 0;
    tick(bus, 20000); assert(MCP2515::sent.size() == 2);
}
void test_rx_budget_and_failed_mode_readback() {
    resetFake(); rejectOneShot = true; CANBus absent;
    assert(!absent.begin()); tick(absent, 1000); assert(MCP2515::sent.empty());
    resetFake(); CANBus bus; assert(bus.begin());
    for (int i = 0; i < 10; ++i) MCP2515::incoming.push_back({});
    tick(bus, 100); assert(MCP2515::incoming.size() == 8);
    tick(bus, 101); assert(MCP2515::incoming.size() == 8); // poll limit
    tick(bus, 102); assert(MCP2515::incoming.size() == 6);
}
void test_integrated_modes_and_expiry() {
    resetFake(); CANBus bus; assert(bus.begin());
    can_frame command{};
    command.can_id = CAN_ID_COMMAND; command.can_dlc = 3;
    command.data[0] = 0x05; command.data[1] = 3; command.data[2] = 2;
    MCP2515::incoming.push_back(command);
    tick(bus, 2);
    assert(g_settings.show_mode && g_settings.show_anim == 2);
    tick(bus, 5002);
    assert(g_settings.show_anim == 3);
    command.data[0] = 0x03; command.can_dlc = 1;
    MCP2515::incoming.push_back(command); tick(bus, 5004);
    assert(!g_settings.show_mode);
    command.can_dlc = 6; command.data[0] = 0x04; command.data[1] = 2;
    command.data[2] = 0; command.data[3] = 100;
    MCP2515::incoming.push_back(command); tick(bus, 5006);
    assert(bus.hasCustomAnim());
    tick(bus, 5106); assert(!bus.hasCustomAnim());
}
int main() {
    test_integrated_modes_and_expiry();
    test_missing_controller_stays_quiet();
    test_disconnected_bus_and_reconnect();
    test_stuck_tx_faults_and_bus_off();
    test_rx_budget_and_failed_mode_readback();
    std::cout << "CAN runtime tests passed\n";
}
