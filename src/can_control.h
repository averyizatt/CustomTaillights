#pragma once

#include "settings.h"
#include "../../../shared/can_contract/include/can_contract/can_protocol.h"

// Live settings only: remote commands do not write NVS.
inline bool applyCanMode(Settings& settings, const can_protocol::CanFrame& frame) {
    if (frame.id != can_protocol::ID_TAILLIGHT_COMMAND || frame.dlc != 3 ||
        frame.data[0] != can_protocol::taillight_command::SET_MODE ||
        !can_protocol::validTaillightMode(frame.data[1], frame.data[2])) return false;
    const uint8_t mode = frame.data[1];
    settings.show_mode = mode >= can_protocol::taillight_mode::SHOW;
    if (settings.show_mode) settings.show_anim = frame.data[2];
    if (mode == can_protocol::taillight_mode::STOCK) settings.turn_anim = 1; // simple flash
    if (mode == can_protocol::taillight_mode::SEQUENTIAL) settings.turn_anim = 0;
    return true;
}
