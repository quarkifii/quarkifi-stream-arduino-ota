#pragma once

#if defined(ESP8266)

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Updater.h>
#include "../common/CommonOtaPlatform.h"

// ================== ESP8266OtaPlatform ==================
// ESP8266 concrete OTA implementation.
// Uses: ESP8266WiFi, ESP8266HTTPClient, WiFiClientSecure, Updater
// ============================================================

class ESP8266OtaPlatform : public CommonOtaPlatform {
public:
    ESP8266OtaPlatform();
    ~ESP8266OtaPlatform() {}

    bool begin()   override;

    bool startOTA(const String &url, const String &auth) override;

private:
    // Execute one OTA attempt. Returns true on success.
    bool performOTA(const String &url, const String &auth);
};

#endif // ESP8266
