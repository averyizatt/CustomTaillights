#include <iostream>
#include <cstring>
#include "led_transport.h"
#include "lighting_runtime.h"
#include "ws2812_encode.h"
#include "spi_fake.h"

unsigned long testMillis = 0;
uint8_t decodeByte(const uint8_t* data) {
    const uint32_t packed = (uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8) | data[2];
    uint8_t value = 0;
    for (int bit = 7; bit >= 0; --bit) {
        const unsigned symbol = (packed >> (bit * 3)) & 7;
        assert(symbol == 4 || symbol == 6); // 100 / 110, both end LOW
        value = (value << 1) | (symbol == 6);
    }
    return value;
}
void checkPacket(const Packet& packet, int pixels) {
    const size_t payloadEnd = 96 + pixels * 9;
    assert(packet.data.size() == ((payloadEnd + 96 + 3) & ~size_t(3)));
    for (size_t i = 0; i < 96; ++i) assert(packet.data[i] == 0);
    for (size_t i = payloadEnd; i < packet.data.size(); ++i) assert(packet.data[i] == 0);
    for (size_t i = 96; i < payloadEnd; i += 3) decodeByte(&packet.data[i]);
}
int main(int argc, char** argv) {
    assert(requestedTurn(false, true, 4) && !requestedTurn(false, false, 4));
    assert(physicalPreviewBlocked(1, 0) && !physicalPreviewBlocked(2, 4));
    for (int value = 0; value < 256; ++value) {
        uint8_t bytes[3]; ws2812::encodeByte(value, bytes);
        assert(decodeByte(bytes) == value);
    }
    CRGB driver[LEDS_PER_SIDE] = {}, passenger[LEDS_PER_SIDE] = {}, status;
    driver[0] = {0x81, 0x24, 0x42};
    passenger[0] = {0x11, 0x88, 0x55};
    status = {1, 2, 3};
    const char* mode = argc > 1 ? argv[1] : "";
    failInit = std::strcmp(mode, "--init-failure") == 0;
    failAdd = std::strcmp(mode, "--device-failure") == 0;
    if (std::strcmp(mode, "--bad-clock") == 0) actualKhz = 2000;
    ledTransportBegin(driver, passenger, &status);
    FastLED.show();
    assert(created == 1 && resetCalls == 0);
    if (failInit || failAdd || actualKhz != 2500) {
        assert(packets.empty());
        assert(ledTransportStatus(0).lastError != ESP_OK && ledTransportStatus(1).lastError != ESP_OK);
        assert(freed == (failInit ? 0 : 1));
        std::cout << "Initialization failure handled without abort\n";
        return 0;
    }
    assert(packets.size() == 3);
    assert(packets[0].pin == 4 && packets[1].pin == 6 && packets[2].pin == 48);
    assert(!routed[5] && !pulldown[5]); // spare remains unused
    checkPacket(packets[0], 380); checkPacket(packets[1], 380); checkPacket(packets[2], 1);
    assert(decodeByte(&packets[0].data[96]) == 0x24 && decodeByte(&packets[0].data[99]) == 0x81);
    assert(decodeByte(&packets[1].data[96]) == 0x88 && decodeByte(&packets[1].data[102]) == 0x55);
    assert(low[4] && low[6] && !routed[4] && !routed[6]);
    queueFailPin = 4;
    FastLED.show();
    assert(ledTransportStatus(0).lastError == ESP_ERR_TIMEOUT);
    assert(ledTransportStatus(1).completed == 2); // failed enqueue doesn't strand passenger
    queueFailPin = -1;
    stall = true;
    FastLED.show();
    assert(queued && ledTransportStatus(0).lastError == ESP_ERR_TIMEOUT);
    const auto snapshot = packets.back().data;
    const void* savedBuffer = queued->tx_buffer;
    const size_t savedLength = queued->length;
    const size_t sent = packets.size();
    driver[0].g = 0xff; passenger[0].b = 0;
    FastLED.show();
    assert(packets.size() == sent && queued->tx_buffer == savedBuffer && queued->length == savedLength);
    assert(std::memcmp(savedBuffer, snapshot.data(), snapshot.size()) == 0);
    stall = false;
    FastLED.show(); // reclaim the late completion before encoding another frame
    assert(!queued && packets.size() == sent + 3);
    assert(ledTransportStatus(0).lastError == ESP_OK && ledTransportStatus(1).lastError == ESP_OK);
    assert(decodeByte(&packets[sent].data[96]) == 0xff);
    std::cout << "Full-frame SPI DMA, GPIO isolation, encoding, timeout ownership and recovery tests passed\n";
}
