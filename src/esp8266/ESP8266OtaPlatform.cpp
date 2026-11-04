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

#if defined(ESP8266)

#include "ESP8266OtaPlatform.h"
#include "../common/HttpDate.h"

#include <time.h>

// ================== ESP8266OtaPlatform ==================

ESP8266OtaPlatform::ESP8266OtaPlatform() : _trustAnchors(nullptr) {}

ESP8266OtaPlatform::~ESP8266OtaPlatform() {
    delete _trustAnchors;
}

void ESP8266OtaPlatform::onRootCAChanged() {
    delete _trustAnchors;
    _trustAnchors = _rootCA.isEmpty() ? nullptr : new BearSSL::X509List(_rootCA.c_str());
}

uint32_t ESP8266OtaPlatform::systemEpoch() {
    return (uint32_t)time(nullptr);
}

uint32_t ESP8266OtaPlatform::fetchServerTime(const String &url) {
    WiFiClientSecure probe;
    probe.setInsecure();   // only the Date header is trusted, and only for the expiry check

    HTTPClient http;
    if (!http.begin(probe, url)) return 0;

    const char *keys[] = {"Date"};
    http.collectHeaders(keys, 1);
    http.setTimeout(10000);

    uint32_t epoch = 0;
    int code = http.sendRequest("HEAD");
    if (code > 0) {                       // any status: 4xx responses carry Date too
        HttpDate::parse(http.header("Date"), epoch);
    }
    http.end();
    return epoch;
}

CommonOtaPlatform::Attempt ESP8266OtaPlatform::configureTls(WiFiClientSecure &client, const String &url) {
    if (_rootCA.isEmpty()) {
        client.setInsecure();             // Legacy: accept any certificate
        return Attempt::OK;
    }

    if (!_trustAnchors || _trustAnchors->getCount() == 0) {
        publishStatus(OtaStatus::ERR_BAD_ARGS);   // PEM could not be parsed
        return Attempt::ABORT;
    }

    uint32_t now = systemEpoch();
    if (now < MIN_VALID_EPOCH) {
        now = fetchServerTime(url);
        if (now < MIN_VALID_EPOCH) {
            publishStatus(OtaStatus::ERR_NO_TIME);
            return Attempt::RETRY;
        }
    }

    client.setTrustAnchors(_trustAnchors);
    client.setX509Time((time_t)now);
    return Attempt::OK;
}

bool ESP8266OtaPlatform::finalizeUpdate() {
    // Only called once the full Content-Length has been written.
    return Update.end(true);
}

void ESP8266OtaPlatform::restartDevice() {
    delay(500);
    WiFi.disconnect(true);
    delay(200);
    ESP.restart();
    delay(2000);
    ESP.reset();
}

#endif // ESP8266
