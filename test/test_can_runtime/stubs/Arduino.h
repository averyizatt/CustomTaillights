#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
using std::min;
#define IRAM_ATTR
#define HIGH 1
#define LOW 0
#define HEX 16
#define F(s) s
extern unsigned long testMillis;
inline unsigned long millis() { return testMillis; }
inline void digitalWrite(int, int) {}
template<class T, class L, class H> T constrain(T v, L lo, H hi) {
    return v < lo ? static_cast<T>(lo) : (v > hi ? static_cast<T>(hi) : v);
}
struct TestSerial {
    std::vector<std::string> lines;
    void println(const char* s) { lines.emplace_back(s); }
    template<class... T> void println(T...) {}
    template<class... T> void print(T...) {}
    template<class... T> void printf(T...) {}
};
inline TestSerial Serial;
