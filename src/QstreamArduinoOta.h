#pragma once

#include <Arduino.h>
#include "interfaces/IOtaPlatform.h"

// ================== QstreamArduinoOta ==================
//
// Public facade for OTA firmware updates on ESP32 / ESP8266.
//
// Usage:
//   QstreamArduinoOta ota;
//   ota.begin();
//   ota.setCallback([](const String &status) { /* handle */ });
//   ota.startOTA("https://example.com/firmware.bin", "Basic xxxx");
// =======================================================

class QstreamArduinoOta {
public:
    QstreamArduinoOta();
    ~QstreamArduinoOta();

    // Initialize the OTA platform. Call once in setup().
    void begin();

    // Register a status callback. Receives strings like:
    //   "starting", "downloading: 45%", "success", "failed: http 404"
    void setCallback(void (*callback)(const String &status));

    // Trigger an OTA update from the given URL with optional Basic Auth header.
    // auth should be the full header value, e.g. "Basic dXNlcjpwYXNz"
    // Use QstreamArduinoOta::buildAuth(user, pass) to generate it.
    bool startOTA(const String &url, const String &auth = "");

    // Convenience: encode username + password into a Basic Auth header value.
    static String buildAuth(const String &username, const String &password);

private:
    IOtaPlatform *_platform;
};
