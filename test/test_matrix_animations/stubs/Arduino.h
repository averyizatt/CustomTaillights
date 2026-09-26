#pragma once
#include <cstdint>
#define IRAM_ATTR
extern unsigned long testMillis;
inline unsigned long millis() { return testMillis; }
template<class T, class L, class H> T constrain(T value, L low, H high) {
    return value < low ? static_cast<T>(low) : (value > high ? static_cast<T>(high) : value);
}
