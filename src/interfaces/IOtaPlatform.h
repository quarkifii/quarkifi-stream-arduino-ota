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

// ================== IOtaPlatform ==================
// Abstract interface for platform-specific OTA implementations.
// Concrete classes: ESP32OtaPlatform, ESP8266OtaPlatform
// ===================================================

class IOtaPlatform {
public:
    virtual ~IOtaPlatform() {}

    // Register a status callback.
    //   status   — one of the OtaStatus:: constants
    //   progress — download progress 0–100 (only meaningful when
    //              status == OtaStatus::DOWNLOADING; 0 otherwise)
    virtual void setCallback(void (*callback)(const byte status, const byte progress)) = 0;

    // Trigger an OTA update from the given URL with optional Basic Auth header.
    // Returns false immediately if OTA cannot be started.
    virtual bool startOTA(const String &url, const String &auth) = 0;
};
