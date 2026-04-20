// The MIT License (MIT)
//
// Copyright (c) 2024 Quarkifi Technologies Pvt Ltd
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <Arduino.h>
#include "interfaces/IOtaPlatform.h"
#include "interfaces/OtaStatus.h"

// ================== QstreamArduinoOta ==================
//
// Public facade for OTA firmware updates on ESP32 / ESP8266.
//
// Usage:
//   QstreamArduinoOta ota;
//
//   // In your OTA trigger handler — register callback then fire:
//   ota.setCallback([](const byte status, const byte progress) {
//       // status  : one of the OtaStatus:: constants
//       // progress: 0–100, only valid when status == OtaStatus::DOWNLOADING
//   });
//   ota.startOTA("https://example.com/firmware.bin", auth);
//
// See interfaces/OtaStatus.h for the full list of status codes.
// =======================================================

class QstreamArduinoOta {
public:
    QstreamArduinoOta();
    ~QstreamArduinoOta();

    // Register a status callback.
    //   status   — OtaStatus:: lifecycle or error code
    //   progress — download progress 0–100 (only valid during DOWNLOADING)
    // Call this before startOTA(). Safe to call multiple times.
    void setCallback(void (*callback)(const byte status, const byte progress));

    // Trigger an OTA update from the given URL with an optional Basic Auth header.
    // auth should be the full header value, e.g. "Basic dXNlcjpwYXNz"
    // Use ota.buildAuth(user, pass) to generate it.
    bool startOTA(const String &url, const String &auth = "");

    // Encode username + password into a Basic Auth header value.
    // Returns empty string if either argument is empty.
    static String buildAuth(const String &username, const String &password);

private:
    IOtaPlatform *_platform;
};
