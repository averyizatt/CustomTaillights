#pragma once
#include <cstdint>
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define IRAM_ATTR
extern unsigned long testMillis;
extern int testLevels[64];
extern int testModes[64];
inline unsigned long millis() { return testMillis; }
inline void pinMode(int pin, int mode) { testModes[pin] = mode; }
inline int digitalRead(int pin) { return testLevels[pin]; }
