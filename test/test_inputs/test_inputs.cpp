#include <cassert>
#include <iostream>
#include "inputs.h"
#include "lighting_runtime.h"

unsigned long testMillis = 100;
int testLevels[64] = {};
int testModes[64] = {};

void settle(Inputs& inputs) {
    inputs.update();
    testMillis += 40;
    inputs.update();
}
int main() {
    for (int& level : testLevels) level = HIGH;
    Inputs inputs;
    inputs.begin();
    for (int pin : PCB_OPTO_PINS) assert(testModes[pin] == INPUT_PULLUP);
    settle(inputs);
    assert(inputs.rawPcbLevels() == 0x3f);
    assert(inputs.driverSnapshot() == 0 && inputs.passengerSnapshot() == 0);
    assert(!physicalPreviewBlocked(inputs.driverSnapshot(), inputs.passengerSnapshot()));

    struct Case { int pin; uint8_t driver, passenger; bool blocksPreview; };
    const Case cases[] = {
        {7, 2, 2, false}, {15, 1, 1, true}, {16, 4, 0, false},
        {17, 0, 4, false}, {18, 8, 8, true}, {8, 0, 0, false}
    };
    for (const auto& c : cases) {
        testLevels[c.pin] = LOW;
        settle(inputs);
        assert(inputs.driverSnapshot() == c.driver);
        assert(inputs.passengerSnapshot() == c.passenger);
        assert(physicalPreviewBlocked(c.driver, c.passenger) == c.blocksPreview);
        testLevels[c.pin] = HIGH;
        settle(inputs);
        if (c.pin == 7) {  // Running is held through dropouts before it goes off.
            assert(inputs.driverSnapshot() == 2 && inputs.passengerSnapshot() == 2);
            testMillis += RUNNING_RELEASE_MS; inputs.update();
        }
        assert(inputs.driverSnapshot() == 0 && inputs.passengerSnapshot() == 0);
    }
    // A loose running-light wire: dropouts shorter than the hold never blink it off.
    testLevels[7] = LOW; settle(inputs);
    assert(inputs.driverSnapshot() == 2 && inputs.passengerSnapshot() == 2);
    for (unsigned long gap : {10ul, 200ul, 900ul, RUNNING_RELEASE_MS - 10}) {
        testLevels[7] = HIGH; inputs.update();
        testMillis += gap; inputs.update();
        assert(inputs.driverSnapshot() == 2 && inputs.passengerSnapshot() == 2);
        testLevels[7] = LOW; inputs.update();
        testMillis += 5; inputs.update();
        assert(inputs.driverSnapshot() == 2);
    }
    // Switched off for real: off once the hold has passed, and brake still works meanwhile.
    testLevels[7] = HIGH; inputs.update();
    testLevels[15] = LOW; settle(inputs);
    assert(inputs.driverSnapshot() == 3);
    testLevels[15] = HIGH; settle(inputs);
    assert(inputs.driverSnapshot() == 2);
    testMillis += RUNNING_RELEASE_MS; inputs.update();
    assert(inputs.driverSnapshot() == 0 && inputs.passengerSnapshot() == 0);
    // A brief brake pulse must not survive the existing 5 ms debounce.
    testLevels[15] = LOW; inputs.update();
    testMillis += 4; inputs.update();
    assert(inputs.driverSnapshot() == 0);
    testLevels[15] = HIGH; settle(inputs);
    assert(inputs.driverSnapshot() == 0);
    std::cout << "Active-low PCB mapping, idle preview eligibility, release, debounce and running-light hold tests passed\n";
}
