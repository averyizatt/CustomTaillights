#pragma once
#include <cassert>
#include <cstdint>
constexpr int MSBFIRST = 1, SPI_MODE0 = 0;
inline uint8_t registers[256] = {};
inline bool rejectOneShot = false;
struct SPISettings { SPISettings(int, int, int) {} };
struct TestSPI {
    int byte = 0, transactions = 0;
    uint8_t command = 0, reg = 0, mask = 0;
    void begin(int, int, int, int) {}
    void beginTransaction(SPISettings) { byte = 0; ++transactions; }
    void endTransaction() { assert(byte == 3 || byte == 4); }
    uint8_t transfer(uint8_t value) {
        ++byte;
        if (byte == 1) { command = value; return 0; }
        if (byte == 2) { reg = value; return 0; }
        if (command == 0x03) return registers[reg];
        assert(command == 0x05);
        if (byte == 3) { mask = value; return 0; }
        if (rejectOneShot && reg == 0x0f) return 0;
        // TX error flags are read-only; cleared automatically on a new TXREQ.
        if (reg == 0x30) mask &= ~0x70;
        registers[reg] = (registers[reg] & ~mask) | (value & mask);
        return 0;
    }
};
inline TestSPI SPI;
