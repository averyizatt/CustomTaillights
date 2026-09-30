// ---------------------------------------------------------------------------
// headless_stubs.cpp
// Replaces wifi_server.cpp and firmware_update.cpp in the headless build
// (-DTAILLIGHT_HEADLESS). No WiFi stack is linked or started; the lights are
// driven only by the GPIO inputs and the CAN bus.
// ---------------------------------------------------------------------------
#ifdef TAILLIGHT_HEADLESS

#include "wifi_server.h"
#include "firmware_update.h"
#include "states.h"

// Web UI preview / soft-input state consumed by main.cpp. Without the web UI
// these stay at their idle values, so loop() always uses the real inputs.
volatile LightState    g_preview_driver        = LightState::OFF;
volatile LightState    g_preview_passenger     = LightState::OFF;
volatile unsigned long g_preview_until_ms      = 0;
volatile unsigned long g_rest_pulse_until_ms   = 0;
volatile uint8_t       g_soft_driver_mask      = 0;
volatile uint8_t       g_soft_passenger_mask   = 0;
volatile uint8_t       g_soft_inputs_enabled   = 0;
volatile uint8_t       g_live_driver_inputs    = 0;
volatile uint8_t       g_live_passenger_inputs = 0;

void WifiServer::begin() {}
void WifiServer::handle() {}
WifiServer wifiServer;

void firmwareUpdateBegin(WebServer&) {}
void firmwareUpdatePoll() {}
void firmwareUpdateAfterHttp() {}
bool firmwareUpdateBusy() { return false; }

#endif
