#pragma once

#include <Arduino.h>

// ================== IOtaPlatform ==================
// Abstract interface for platform-specific OTA implementations.
// Concrete classes: ESP32OtaPlatform, ESP8266OtaPlatform
// ===================================================

class IOtaPlatform {
public:
    virtual ~IOtaPlatform() {}

    virtual bool begin() = 0;

    virtual void setCallback(void (*callback)(const String &status)) = 0;

    // Returns true if OTA was successfully initiated (not necessarily completed)
    virtual bool startOTA(const String &url, const String &auth) = 0;

};
