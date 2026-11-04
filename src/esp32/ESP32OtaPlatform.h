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

#if defined(ESP32)

#include "../common/CommonOtaPlatform.h"

// ================== ESP32OtaPlatform ==================
// ESP32 concrete OTA implementation.
// TLS: mbedTLS via WiFiClientSecure. With a root CA set, the server chain and
// hostname are verified. Certificate dates are not checked by the default
// Arduino-ESP32 mbedTLS build, so no clock is needed.
// ======================================================

class ESP32OtaPlatform : public CommonOtaPlatform {
public:
    ESP32OtaPlatform() {}
    ~ESP32OtaPlatform() {}

protected:
    Attempt configureTls(WiFiClientSecure &client, const String &url) override;
    bool    finalizeUpdate() override;
    void    restartDevice() override;
};

#endif // ESP32
