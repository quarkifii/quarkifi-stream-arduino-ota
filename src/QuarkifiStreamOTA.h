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

// ================== QuarkifiStreamArduinoOTA ==================
//
// Public facade for OTA firmware updates on ESP32 / ESP8266.
//
// Usage:
//   QuarkifiStreamArduinoOTA ota;
//
//   ota.setRootCA(ROOT_CA_PEM);          // recommended: verify the server
//   ota.setCallback([](const byte status, const byte progress) {
//       // status  : one of the OtaStatus:: constants
//       // progress: 0–100, only valid when status == OtaStatus::DOWNLOADING
//   });
//   ota.startOTA("https://example.com/firmware.bin", auth, md5);
//
// startOTA() is BLOCKING: it returns false on failure; on success the
// device restarts and it never returns.
//
// See interfaces/OtaStatus.h for the full list of status codes.
// ===============================================================

class QuarkifiStreamArduinoOTA {
public:
    QuarkifiStreamArduinoOTA();
    ~QuarkifiStreamArduinoOTA();

    // Owns a platform object — copying would double-free it.
    QuarkifiStreamArduinoOTA(const QuarkifiStreamArduinoOTA &) = delete;
    QuarkifiStreamArduinoOTA &operator=(const QuarkifiStreamArduinoOTA &) = delete;

    // Register a status callback.
    //   status   — OtaStatus:: lifecycle or error code
    //   progress — download progress 0–100 (only valid during DOWNLOADING)
    // Call this before startOTA(). Safe to call multiple times.
    void setCallback(void (*callback)(const byte status, const byte progress));

    // Verify the HTTPS server against this root CA certificate (PEM).
    // The PEM is copied. Pass nullptr or "" to go back to unverified TLS.
    // While a root CA is set, plain http:// URLs are rejected (ERR_BAD_ARGS).
    // No NTP/clock is required (see ESP8266OtaPlatform.h for how).
    void setRootCA(const char *rootCaPem);

    // Trigger an OTA update from the given URL.
    //   auth — optional full Authorization header value, e.g. "Basic dXNlcjpwYXNz"
    //          (use buildAuth(user, pass) to generate it)
    //   md5  — optional expected MD5 of the firmware (32 hex chars). The image
    //          is not committed if it doesn't match. For tamper protection,
    //          deliver it over a trusted channel (e.g. MQTT over TLS).
    bool startOTA(const String &url, const String &auth = "", const String &md5 = "");

    // Encode username + password into a Basic Auth header value.
    // Returns empty string if username is empty. An empty password is allowed.
    static String buildAuth(const String &username, const String &password);

private:
    IOtaPlatform *_platform;
};
