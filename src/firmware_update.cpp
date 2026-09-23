#include "firmware_update.h"
#include "firmware_image.h"
#include "settings.h"
#include "inputs.h"
#include "led_output.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <esp_task_wdt.h>

extern Inputs inputs;

namespace {
WebServer* server = nullptr;
char password[64] = {};
bool active = false, restartPending = false, watchdogPaused = false;
bool httpAttempt = false, httpComplete = false, httpResponded = false;
unsigned httpFiles = 0;
size_t expected = 0, received = 0;
unsigned long restartAt = 0, transferStarted = 0;
uint32_t bootId = 0;
String lastError;
FirmwareImageCheck imageCheck;

bool signalsActive() { return (inputs.driverSnapshot() | inputs.passengerSnapshot()) != 0; }
const esp_partition_t* updatePartition() { return esp_ota_get_next_update_partition(nullptr); }

void resumeWatchdog() {
    if (watchdogPaused) {
        esp_task_wdt_add(nullptr);
        esp_task_wdt_reset();
        watchdogPaused = false;
    }
}
void startTransfer() {
    active = true;
    lastError = "";
    received = 0;
    transferStarted = millis();
    // The synchronous network receivers can wait longer than the render WDT.
    // Only this task is detached; the input task remains monitored.
    if (!watchdogPaused) watchdogPaused = esp_task_wdt_delete(nullptr) == ESP_OK;
    ledOutputClear(true, true);
}
void fail(const char* message) {
    lastError = message;
    if (Update.isRunning()) Update.abort();
    active = false;
    Serial.printf("[ota] %s\n", lastError.c_str());
    // Restore the watchdog only once the blocking receiver has returned.
}
void completed() {
    active = false;
    restartPending = true;
    restartAt = millis() + 1000UL;
    lastError = "";
}
void replyError(int code, const char* message) {
    JsonDocument doc;
    doc["error"] = message;
    String body;
    serializeJson(doc, body);
    server->sendHeader("Connection", "close");
    server->send(code, "application/json", body);
    httpResponded = true;
}
void rejectUpload(int code, const char* message) {
    fail(message);
    replyError(code, message);
    server->client().stop(); // Do not drain a bad/unauthorized multi-MB body.
}

bool stagedImageValid() {
    const auto* partition = updatePartition();
    if (!partition || expected > partition->size) return false;
    FirmwareImageCheck check;
    uint8_t buffer[1024];
    for (size_t offset = 0; offset < expected; offset += sizeof(buffer)) {
        const size_t length = min(sizeof(buffer), expected - offset);
        if (esp_partition_read(partition, offset, buffer, length) != ESP_OK) return false;
        check.add(buffer, length);
        if (signalsActive()) return false;
        yield();
    }
    return check.valid(true);
}

void uploadChunk() {
    HTTPUpload& upload = server->upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (httpResponded) return;
        if (httpFiles++ != 0) { rejectUpload(400, "Select exactly one firmware.bin file"); return; }
        httpAttempt = true;
        httpComplete = false;
        if (active || restartPending) { replyError(409, "An update is already running"); server->client().stop(); return; }
        if (!server->authenticate("admin", password)) { rejectUpload(401, "Incorrect controller WiFi password"); return; }
        if (signalsActive()) { rejectUpload(409, "Release all vehicle light inputs before updating"); return; }
        const auto* partition = updatePartition();
        if (!partition || partition == esp_ota_get_running_partition()) { rejectUpload(409, "No inactive OTA slot; install by USB first"); return; }
        if (upload.name != "firmware" || !upload.filename.endsWith(".bin") ||
            !firmwareSize(server->header("X-Firmware-Size").c_str(), partition->size, expected)) {
            rejectUpload(400, "Choose an application firmware.bin that fits the OTA slot"); return;
        }
        imageCheck = FirmwareImageCheck();
        startTransfer();
        if (!Update.begin(expected, U_FLASH)) { rejectUpload(500, Update.errorString()); return; }
    } else if (upload.status == UPLOAD_FILE_WRITE && active && httpAttempt) {
        if (millis() - transferStarted > 180000UL) { rejectUpload(408, "Firmware upload timed out"); return; }
        if (signalsActive()) { rejectUpload(409, "Vehicle input activated; update aborted"); return; }
        if (upload.currentSize > expected - received) { rejectUpload(400, "Firmware exceeds its declared size"); return; }
        imageCheck.add(upload.buf, upload.currentSize);
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { rejectUpload(400, Update.errorString()); return; }
        received += upload.currentSize;
        yield();
    } else if (upload.status == UPLOAD_FILE_END && active && httpAttempt) {
        httpComplete = received == expected && imageCheck.valid();
        if (!httpComplete) { rejectUpload(400, "Incomplete image or wrong firmware target; use this project's firmware.bin"); }
    } else if (upload.status == UPLOAD_FILE_ABORTED && httpAttempt && active) {
        fail("Upload disconnected; existing firmware retained");
    }
}
void uploadFinished() {
    if (httpResponded) return;
    if (!httpAttempt || !httpComplete || !active) { replyError(400, "No complete firmware upload received"); return; }
    if (signalsActive()) { fail("Vehicle input activated; update aborted"); replyError(409, lastError.c_str()); return; }
    // Finalize only after the complete multipart request, never per file part.
    if (!Update.end()) { fail(Update.errorString()); replyError(400, lastError.c_str()); return; }
    completed();
    server->sendHeader("Connection", "close");
    server->send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
    httpResponded = true;
}
void information() {
    const auto* partition = updatePartition();
    JsonDocument doc;
    doc["target"] = FIRMWARE_TARGET;
    doc["build"] = __DATE__ " " __TIME__;
    doc["boot_id"] = bootId;
    doc["max_size"] = partition ? partition->size : 0;
    doc["available"] = partition && partition != esp_ota_get_running_partition();
    doc["busy"] = active || restartPending;
    doc["inputs_active"] = signalsActive();
    doc["received"] = received;
    doc["expected"] = expected;
    doc["error"] = lastError;
    String body;
    serializeJson(doc, body);
    server->sendHeader("Cache-Control", "no-store");
    server->send(200, "application/json", body);
}
}

void firmwareUpdateBegin(WebServer& webServer) {
    server = &webServer;
    bootId = esp_random();
    // Use the AP password from startup, even when connected to a home network.
    strlcpy(password, g_settings.ap_pass, sizeof(password));
    if (strlen(password) < 8) strlcpy(password, "foxbody1", sizeof(password));
    static const char* headers[] = {"Authorization", "X-Firmware-Size"};
    server->collectHeaders(headers, 2);
    server->on("/api/firmware", HTTP_GET, information);
    server->on("/api/firmware", HTTP_POST, uploadFinished, uploadChunk);

    ArduinoOTA.setHostname("foxbody-taillights");
    ArduinoOTA.setPassword(password);
    ArduinoOTA.setRebootOnSuccess(false);
    ArduinoOTA.setTimeout(1500);
    ArduinoOTA.onStart([]() {
        if (!watchdogPaused) watchdogPaused = esp_task_wdt_delete(nullptr) == ESP_OK;
        lastError = "";
        if (active || restartPending || signalsActive() || ArduinoOTA.getCommand() != U_FLASH) {
            fail("OTA requires idle vehicle inputs and an application firmware image");
            return;
        }
        expected = Update.size();
        startTransfer();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        if (!active) return;
        received = progress;
        if (millis() - transferStarted > 180000UL) { fail("OTA transfer timed out"); return; }
        if (signalsActive()) { fail("Vehicle input activated; update aborted"); return; }
        if (progress == total && !stagedImageValid()) fail("Wrong firmware target or invalid application image");
    });
    ArduinoOTA.onEnd([]() { completed(); });
    ArduinoOTA.onError([](ota_error_t error) {
        if (lastError.isEmpty()) {
            if (error == OTA_AUTH_ERROR) fail("Incorrect OTA password");
            else fail("OTA transfer failed; existing firmware retained");
        } else if (Update.isRunning()) Update.abort();
        active = false;
    });
    ArduinoOTA.begin();
    Serial.printf("[ota] %s ready on UDP 3232 and /api/firmware\n", FIRMWARE_TARGET);
}

void firmwareUpdatePoll() {
    if (restartPending) {
        if (static_cast<int32_t>(millis() - restartAt) >= 0) ESP.restart();
        return;
    }
    if (!active) {
        ArduinoOTA.handle();
        resumeWatchdog();
    }
}
void firmwareUpdateAfterHttp() {
    if (httpAttempt && active) fail("Upload ended before completion; existing firmware retained");
    httpAttempt = httpComplete = httpResponded = false;
    httpFiles = 0;
    resumeWatchdog();
}
bool firmwareUpdateBusy() { return active || restartPending; }
