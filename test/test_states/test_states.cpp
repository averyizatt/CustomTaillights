#include <assert.h>
#include "states.h"

static TurnBlinkSnapshot blinkAfterTwoEdges() {
    TurnBlinkDetector d;
    d.update(false, 0);
    d.update(true, 300);
    return d.update(false, 600);
}

static void test_steady_modes() {
    assert(resolveSideState(false, true, false, false, false) == LightState::RUNNING);
    assert(resolveSideState(true, false, false, false, false) == LightState::BRAKE);
    assert(resolveSideState(true, true, false, false, false) == LightState::BRAKE);

    // Both sides share the same steady-mode resolver and brightness path.
    LightState leftRunning = resolveSideState(false, true, false, false, false);
    LightState rightRunning = resolveSideState(false, true, false, false, false);
    assert(leftRunning == LightState::RUNNING);
    assert(rightRunning == LightState::RUNNING);

    LightState leftBrake = resolveSideState(true, true, false, false, false);
    LightState rightBrake = resolveSideState(true, true, false, false, false);
    assert(leftBrake == LightState::BRAKE);
    assert(rightBrake == LightState::BRAKE);
}

static void test_steady_vehicle_signals_are_shared() {
    bool driverBrake = true;
    bool passengerBrake = false;
    bool brakeActive = driverBrake || passengerBrake;
    assert(resolveSideState(brakeActive, false, false, false, false) == LightState::BRAKE);
    assert(resolveSideState(brakeActive, false, false, false, false) == LightState::BRAKE);

    bool driverRunning = false;
    bool passengerRunning = true;
    bool runningActive = driverRunning || passengerRunning;
    assert(resolveSideState(false, runningActive, false, false, false) == LightState::RUNNING);
    assert(resolveSideState(false, runningActive, false, false, false) == LightState::RUNNING);
}

static void test_left_turn_side_only() {
    LightState left = resolveSideState(false, true, true, false, false);
    LightState right = resolveSideState(false, true, false, false, false);
    assert(left == LightState::TURN);
    assert(right == LightState::RUNNING);

    left = resolveSideState(true, false, true, false, false);
    right = resolveSideState(true, false, false, false, false);
    assert(left == LightState::BRAKE_TURN);
    assert(right == LightState::BRAKE);
}

static void test_right_turn_side_only() {
    LightState left = resolveSideState(false, true, false, false, false);
    LightState right = resolveSideState(false, true, true, false, false);
    assert(left == LightState::RUNNING);
    assert(right == LightState::TURN);

    left = resolveSideState(true, false, false, false, false);
    right = resolveSideState(true, false, true, false, false);
    assert(left == LightState::BRAKE);
    assert(right == LightState::BRAKE_TURN);
}

static void test_hazard_requires_blinking_transitions() {
    TurnBlinkDetector left;
    TurnBlinkDetector right;

    TurnBlinkSnapshot leftSteady = left.update(true, 0);
    TurnBlinkSnapshot rightSteady = right.update(true, 0);
    assert(!validHazardBlink(leftSteady, rightSteady));

    leftSteady = left.update(true, 1000);
    rightSteady = right.update(true, 1000);
    assert(!validHazardBlink(leftSteady, rightSteady));

    TurnBlinkDetector hazardLeft;
    TurnBlinkDetector hazardRight;
    hazardLeft.update(false, 0);
    hazardRight.update(false, 0);
    hazardLeft.update(true, 300);
    hazardRight.update(true, 320);
    TurnBlinkSnapshot hbLeft = hazardLeft.update(false, 600);
    TurnBlinkSnapshot hbRight = hazardRight.update(false, 620);
    assert(validHazardBlink(hbLeft, hbRight));
    assert(resolveSideState(true, true, hbLeft.blinking, false,
                            validHazardBlink(hbLeft, hbRight)) == LightState::HAZARD);
}

static void test_one_sided_blinks_are_not_hazards() {
    TurnBlinkSnapshot leftBlink = blinkAfterTwoEdges();
    TurnBlinkSnapshot rightOff;
    assert(!validHazardBlink(leftBlink, rightOff));
    assert(resolveSideState(false, true, leftBlink.blinking, false, false) == LightState::TURN);
    assert(resolveSideState(true, false, leftBlink.blinking, false, false) == LightState::BRAKE_TURN);
}

int main() {
    test_steady_modes();
    test_steady_vehicle_signals_are_shared();
    test_left_turn_side_only();
    test_right_turn_side_only();
    test_hazard_requires_blinking_transitions();
    test_one_sided_blinks_are_not_hazards();
    return 0;
}
