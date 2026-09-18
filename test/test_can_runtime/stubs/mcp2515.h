#pragma once
#include "SPI.h"
#include <deque>
#include <vector>
struct can_frame { uint32_t can_id = 0; uint8_t can_dlc = 0; uint8_t data[8] = {}; };
constexpr int CAN_500KBPS = 0, MCP_8MHZ = 0;
class MCP2515 {
public:
    enum ERROR { ERROR_OK, ERROR_FAIL, ERROR_NOMSG };
    enum MASK { MASK0, MASK1 };
    enum RXF { RXF0, RXF1, RXF2, RXF3, RXF4, RXF5 };
    enum TXBn { TXB0 };
    inline static int resets = 0;
    inline static ERROR resetResult = ERROR_OK, sendResult = ERROR_OK;
    inline static uint8_t errors = 0;
    inline static std::vector<can_frame> sent;
    inline static std::deque<can_frame> incoming;
    explicit MCP2515(int) {}
    ERROR reset() { ++resets; return resetResult; }
    ERROR setBitrate(int, int) { return ERROR_OK; }
    ERROR setFilterMask(MASK, bool, uint32_t) { return ERROR_OK; }
    ERROR setFilter(RXF, bool, uint32_t) { return ERROR_OK; }
    ERROR setNormalMode() { registers[0x0f] = 0; return ERROR_OK; }
    ERROR sendMessage(TXBn, const can_frame* f) {
        assert(!(registers[0x30] & 0x08));
        sent.push_back(*f);
        registers[0x30] = 0x08;
        return sendResult;
    }
    uint8_t getErrorFlags() { return errors; }
    void clearRXnOVRFlags() { errors &= ~0xc0; }
    ERROR readMessage(can_frame* f) {
        if (incoming.empty()) return ERROR_NOMSG;
        *f = incoming.front(); incoming.pop_front(); return ERROR_OK;
    }
};
