#pragma once

#if defined(ESP32)

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "../common/CommonOtaPlatform.h"

// ================== ESP32OtaPlatform ==================
// ESP32 concrete OTA implementation.
// Uses: WiFiClient, WiFiClientSecure, HTTPClient, Update
// ======================================================

class ESP32OtaPlatform : public CommonOtaPlatform {
public:
    ESP32OtaPlatform();
    ~ESP32OtaPlatform() {}

    bool begin()   override;

    bool startOTA(const String &url, const String &auth) override;

private:
    // Execute one OTA attempt. Returns true on success.
    bool performOTA(const String &url, const String &auth);
};

#endif // ESP32
