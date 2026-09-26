#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>
using esp_err_t = int;
using gpio_num_t = int;
using spi_host_device_t = int;
constexpr int ESP_OK = 0, ESP_ERR_NO_MEM = 0x101, ESP_ERR_INVALID_STATE = 0x103;
constexpr int ESP_ERR_INVALID_SIZE = 0x104, ESP_ERR_NOT_SUPPORTED = 0x106, ESP_ERR_TIMEOUT = 0x107;
constexpr int GPIO_MODE_OUTPUT = 1, SPI3_HOST = 2, SPI_DMA_CH_AUTO = 3;
#define pdMS_TO_TICKS(x) (x)
struct spi_bus_config_t {
    int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num;
    int data4_io_num, data5_io_num, data6_io_num, data7_io_num, max_transfer_sz;
};
struct spi_device_interface_config_t {
    int clock_speed_hz = 0, mode = 0, spics_io_num = 0, queue_size = 0;
};
struct spi_transaction_t { size_t length = 0; const void* tx_buffer = nullptr; };
using spi_device_handle_t = int*;
struct Signal { int spid_out; };
inline Signal spi_periph_signal[3] = {{10}, {11}, {12}};
struct Packet { int pin; std::vector<uint8_t> data; };
inline std::vector<Packet> packets;
inline bool routed[64] = {}, low[64] = {}, pullup[64] = {}, pulldown[64] = {};
inline unsigned resetCalls = 0;
inline bool failInit = false, failAdd = false, stall = false;
inline int actualKhz = 2500, created = 0, freed = 0, queueFailPin = -1, fakeDevice = 0;
inline spi_transaction_t* queued = nullptr;
inline int gpio_reset_pin(int pin) {
    ++resetCalls; pullup[pin] = true; pulldown[pin] = false; routed[pin] = false; return ESP_OK;
}
inline int gpio_pullup_dis(int pin) { pullup[pin] = false; return ESP_OK; }
inline int gpio_pulldown_en(int pin) { pulldown[pin] = true; return ESP_OK; }
inline int gpio_set_level(int pin, int value) { low[pin] = value == 0; return ESP_OK; }
inline int gpio_set_direction(int pin, int) { routed[pin] = false; return ESP_OK; }
inline void esp_rom_gpio_connect_out_signal(int pin, int signal, bool invert, bool invertEnable) {
    assert(!queued && signal == spi_periph_signal[SPI3_HOST].spid_out && !invert && !invertEnable);
    assert(low[pin] && pulldown[pin] && !pullup[pin]);
    for (bool route : routed) assert(!route);
    routed[pin] = true;
}
inline int spi_bus_initialize(int host, const spi_bus_config_t* bus, int dma) {
    ++created;
    assert(host == SPI3_HOST && dma == SPI_DMA_CH_AUTO);
    assert(bus->mosi_io_num == 4 && bus->miso_io_num == -1 && bus->sclk_io_num == -1);
    assert(bus->max_transfer_sz == 3612);
    assert(bus->quadwp_io_num == -1 && bus->quadhd_io_num == -1);
    if (failInit) return ESP_ERR_NO_MEM;
    routed[bus->mosi_io_num] = true;
    return ESP_OK;
}
inline int spi_bus_add_device(int host, const spi_device_interface_config_t* cfg, spi_device_handle_t* out) {
    assert(host == SPI3_HOST && cfg->clock_speed_hz == 2500000 && cfg->mode == 0);
    assert(cfg->spics_io_num == -1 && cfg->queue_size == 1);
    if (failAdd) return ESP_ERR_NO_MEM;
    *out = &fakeDevice; return ESP_OK;
}
inline int spi_device_get_actual_freq(spi_device_handle_t, int* khz) { *khz = actualKhz; return ESP_OK; }
inline int spi_bus_remove_device(spi_device_handle_t) { return ESP_OK; }
inline int spi_bus_free(int host) { assert(host == SPI3_HOST); ++freed; return ESP_OK; }
inline int spi_device_queue_trans(spi_device_handle_t, spi_transaction_t* tx, unsigned wait) {
    assert(wait == 0 && !queued && tx->length % 32 == 0);
    assert(reinterpret_cast<uintptr_t>(tx->tx_buffer) % 4 == 0);
    int pin = -1;
    for (int i = 0; i < 64; ++i) if (routed[i]) { assert(pin == -1); pin = i; }
    assert(pin >= 0);
    if (pin == queueFailPin) return ESP_ERR_TIMEOUT;
    queued = tx;
    const auto* bytes = static_cast<const uint8_t*>(tx->tx_buffer);
    packets.push_back({pin, {bytes, bytes + tx->length / 8}});
    return ESP_OK;
}
inline int spi_device_get_trans_result(spi_device_handle_t, spi_transaction_t** tx, unsigned wait) {
    assert(queued && wait <= 30);
    if (stall) return ESP_ERR_TIMEOUT;
    *tx = queued; queued = nullptr; return ESP_OK;
}
