#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// An application header and a build-specific marker reject bootloader/factory
// images and firmware for the other wiring target before selecting a boot slot.
#ifdef CUSTOM_TAILLIGHTS_PCB
static constexpr char FIRMWARE_TARGET[] = "FOXBODY-ESP32S3-PCB-OTA-V1";
#else
static constexpr char FIRMWARE_TARGET[] = "FOXBODY-ESP32S3-DEV-OTA-V1";
#endif

class FirmwareImageCheck {
public:
    void add(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            if (bytes_ < sizeof(prefix_)) prefix_[bytes_] = data[i];
            ++bytes_;
            // The marker's first character does not recur in its prefix.
            if (data[i] == static_cast<uint8_t>(FIRMWARE_TARGET[matched_])) ++matched_;
            else matched_ = data[i] == FIRMWARE_TARGET[0] ? 1 : 0;
            if (matched_ == sizeof(FIRMWARE_TARGET) - 1) { found_ = true; matched_ = 0; }
        }
    }
    bool valid(bool stagedFlash = false) const {
        // Update deliberately withholds the first 16 bytes until activation.
        const bool header = stagedFlash || (prefix_[0] == 0xe9 && prefix_[12] == 9 && prefix_[13] == 0);
        const bool app = prefix_[32] == 0x32 && prefix_[33] == 0x54 && prefix_[34] == 0xcd && prefix_[35] == 0xab;
        return bytes_ >= 288 && header && app && found_;
    }
private:
    uint8_t prefix_[36] = {};
    size_t bytes_ = 0;
    size_t matched_ = 0;
    bool found_ = false;
};

inline bool firmwareSize(const char* text, size_t maximum, size_t& result) {
    if (!text || !*text) return false;
    size_t value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        const size_t digit = *text - '0';
        if (digit > maximum || value > (maximum - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (value < 288) return false;
    result = value;
    return true;
}
