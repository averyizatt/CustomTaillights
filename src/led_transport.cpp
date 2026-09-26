#include "led_transport.h"
#include "config.h"
#include "ws2812_encode.h"
#include <esp_idf_version.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <esp_rom_gpio.h>
#include <soc/spi_periph.h>
#include <esp_heap_caps.h>
#include <cstring>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "The LED transport requires ESP32-S3 (SPI3 is reserved for LEDs)."
#endif

namespace {
// Arduino's global SPI (CAN) uses SPI2 on S3. Never share that bus with LEDs.
constexpr spi_host_device_t LED_HOST = SPI3_HOST;
constexpr size_t MAX_BYTES = (LEDS_PER_SIDE * 9 + 2 * ws2812::RESET_BYTES + 3) & ~size_t(3);
constexpr int TX_TIMEOUT_MS = 30;
spi_device_handle_t device = nullptr;
spi_transaction_t transaction = {};
uint8_t* encoded = nullptr;
int currentPin = -1;
int initError = ESP_OK;
bool pending = false;
LedChannelStatus channels[3];

void recordResult(LedChannelStatus& stats, esp_err_t error) {
    if (error == ESP_OK) ++stats.completed;
    else ++stats.failures;
    if (error != stats.lastError) {
        Serial.printf("[LED] GPIO%d output %s (error %d)\n", stats.pin,
                      error == ESP_OK ? "recovered" : "failed", error);
    }
    stats.lastError = error;
}

void holdLow(int pin) {
    // No gpio_reset_pin(): it enables a pull-up and briefly disables output.
    gpio_pullup_dis(static_cast<gpio_num_t>(pin));
    gpio_pulldown_en(static_cast<gpio_num_t>(pin));
    gpio_set_level(static_cast<gpio_num_t>(pin), 0);
    // This also detaches the peripheral matrix route without disabling output.
    gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_OUTPUT);
}

esp_err_t initTransport() {
    encoded = static_cast<uint8_t*>(heap_caps_malloc(MAX_BYTES,
                                  MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    if (!encoded) return ESP_ERR_NO_MEM;
    spi_bus_config_t bus = {};
    bus.mosi_io_num = PIN_LED_DRIVER;
    bus.miso_io_num = bus.sclk_io_num = -1;
    bus.quadwp_io_num = bus.quadhd_io_num = -1;
    bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
    bus.max_transfer_sz = MAX_BYTES;
    esp_err_t error = spi_bus_initialize(LED_HOST, &bus, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) return error;
    spi_device_interface_config_t config = {};
    config.clock_speed_hz = ws2812::SPI_HZ;
    config.mode = 0;
    config.spics_io_num = -1;
    config.queue_size = 1;
    error = spi_bus_add_device(LED_HOST, &config, &device);
    if (error == ESP_OK) {
        int actualKhz = 0;
        error = spi_device_get_actual_freq(device, &actualKhz);
        if (error == ESP_OK && actualKhz != ws2812::SPI_HZ / 1000) error = ESP_ERR_NOT_SUPPORTED;
    }
    if (error != ESP_OK) {
        if (device) { spi_bus_remove_device(device); device = nullptr; }
        spi_bus_free(LED_HOST);
    }
    // The SPI driver initially attaches MOSI; idle all pins until a packet starts.
    holdLow(PIN_LED_DRIVER);
    return error;
}

// A timed-out SPI transaction may still own both the descriptor and buffer.
// Reclaim it only after the driver confirms completion; never overwrite it.
esp_err_t finishPending(unsigned ticks) {
    if (!pending) return ESP_OK;
    spi_transaction_t* completed = nullptr;
    const esp_err_t error = spi_device_get_trans_result(device, &completed, ticks);
    if (error != ESP_OK) return error;
    if (completed != &transaction) return ESP_ERR_INVALID_STATE;
    pending = false;
    holdLow(currentPin);
    currentPin = -1;
    return ESP_OK;
}

esp_err_t transmit(int pin, size_t bytes) {
    currentPin = pin;
    esp_rom_gpio_connect_out_signal(pin, spi_periph_signal[LED_HOST].spid_out, false, false);
    transaction = {};
    transaction.length = bytes * 8;
    transaction.tx_buffer = encoded;
    esp_err_t error = spi_device_queue_trans(device, &transaction, 0);
    if (error != ESP_OK) {
        holdLow(pin);
        currentPin = -1;
        return error;
    }
    pending = true;
    return finishPending(pdMS_TO_TICKS(TX_TIMEOUT_MS));
}

class DmaController : public CPixelLEDController<LED_COLOR_ORDER> {
public:
    explicit DmaController(unsigned index) : _index(index) {}
    void init() override {}
    void showPixels(PixelController<LED_COLOR_ORDER>& pixels) override {
        LedChannelStatus& stats = channels[_index];
        esp_err_t error = initError;
        if (error == ESP_OK) error = finishPending(0);
        if (error == ESP_OK && (pixels.size() < 0 || pixels.size() > LEDS_PER_SIDE)) {
            error = ESP_ERR_INVALID_SIZE;
        }
        if (error == ESP_OK) {
            std::memset(encoded, 0, MAX_BYTES);
            size_t count = ws2812::RESET_BYTES;
            while (pixels.has(1)) {
                ws2812::encodeByte(pixels.loadAndScale0(), encoded + count); count += 3;
                ws2812::encodeByte(pixels.loadAndScale1(), encoded + count); count += 3;
                ws2812::encodeByte(pixels.loadAndScale2(), encoded + count); count += 3;
                pixels.advanceData();
                pixels.stepDithering();
            }
            count = (count + ws2812::RESET_BYTES + 3) & ~size_t(3);
            error = transmit(stats.pin, count);
        }
        recordResult(stats, error);
    }
private:
    unsigned _index;
};
DmaController driverController(0), passengerController(1), statusController(2);
} // namespace

void ledTransportBegin(CRGB* driver, CRGB* passenger, CRGB* status) {
    channels[0].pin = PIN_LED_DRIVER;
    channels[1].pin = PIN_LED_PASSENGER;
    channels[2].pin = PIN_STATUS_LED;
    for (const auto& output : channels) holdLow(output.pin);
    initError = initTransport();
    if (initError != ESP_OK && encoded) { heap_caps_free(encoded); encoded = nullptr; }
    FastLED.addLeds(&driverController, driver, LEDS_PER_SIDE);
    FastLED.addLeds(&passengerController, passenger, LEDS_PER_SIDE);
    FastLED.addLeds(&statusController, status, 1);
    Serial.printf("[LED] spi3-dma-full-frame: %s (error %d)\n",
                  initError == ESP_OK ? "ready" : "unavailable", initError);
}

const LedChannelStatus& ledTransportStatus(unsigned index) { return channels[index < 3 ? index : 0]; }
const char* ledTransportName() { return "spi3-dma-full-frame"; }
