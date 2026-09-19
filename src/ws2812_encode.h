#pragma once
#include <stddef.h>
#include <stdint.h>

// Same 2.5 MHz, three-SPI-bits-per-LED-bit encoding used by Espressif's
// WS2812 SPI driver: 0 = 100 (400/800 ns), 1 = 110 (800/400 ns).
namespace ws2812 {
static constexpr int SPI_HZ = 2500000;
static constexpr size_t RESET_BYTES = 96; // 307.2 us LOW
inline void encodeByte(uint8_t byte, uint8_t* out) {
    uint32_t bits = 0;
    for (int bit = 7; bit >= 0; --bit) bits = (bits << 3) | ((byte & (1u << bit)) ? 6u : 4u);
    out[0] = static_cast<uint8_t>(bits >> 16);
    out[1] = static_cast<uint8_t>(bits >> 8);
    out[2] = static_cast<uint8_t>(bits);
}
} // namespace ws2812
