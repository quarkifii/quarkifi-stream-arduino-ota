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

#if defined(ESP8266)

#include "../common/CommonOtaPlatform.h"

// ================== ESP8266OtaPlatform ==================
// ESP8266 concrete OTA implementation.
// TLS: BearSSL via WiFiClientSecure. With a root CA set, the server chain and
// hostname are verified. BearSSL also checks certificate dates, so a time is
// required — no NTP dependency is added:
//   - if the system clock is already set (e.g. the sketch ran configTime()),
//     it is used;
//   - otherwise a HEAD request is made to the OTA host and its HTTP `Date`
//     header is used. A forged Date can only defeat the expiry check; the
//     chain and hostname are still verified against the root CA.
// ============================================================

class ESP8266OtaPlatform : public CommonOtaPlatform {
public:
    // Any epoch below this (2024-01-01) is treated as "clock not set".
    static const uint32_t MIN_VALID_EPOCH = 1704067200UL;

    ESP8266OtaPlatform();
    ~ESP8266OtaPlatform();

protected:
    Attempt configureTls(WiFiClientSecure &client, const String &url) override;
    bool    finalizeUpdate() override;
    void    restartDevice() override;
    void    onRootCAChanged() override;

    // Current system time (UTC epoch). Virtual for testing.
    virtual uint32_t systemEpoch();

    // HEAD the URL over an unverified connection and parse its Date header.
    // Returns 0 when unavailable.
    uint32_t fetchServerTime(const String &url);

private:
    BearSSL::X509List *_trustAnchors;
};

#endif // ESP8266
