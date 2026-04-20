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

// ================== ESP8266OtaPlatform ==================

ESP8266OtaPlatform::ESP8266OtaPlatform() {
    _state = OtaState::IDLE;
}

bool ESP8266OtaPlatform::startOTA(const String &url, const String &auth) {
    if (isBusy()) {
        publishStatus(OtaStatus::ERR_BUSY);
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        publishStatus(OtaStatus::ERR_NO_WIFI);
        return false;
    }

    for (int attempt = 1; attempt <= _maxRetries; attempt++) {
        publishStatus(OtaStatus::STARTING);

        if (performOTA(url, auth)) {
            publishStatus(OtaStatus::SUCCESS);
            delay(500);
            publishStatus(OtaStatus::RESTARTING);
            delay(500);

            WiFi.disconnect(true);
            delay(200);

            ESP.restart();
            delay(2000);
            ESP.reset();
            return true; // Unreachable, satisfies the compiler
        }

        // _state is FAILED here; retry unless this was the last attempt
        if (attempt < _maxRetries) {
            delay(2000);
        }
    }

    publishStatus(OtaStatus::ERR_RETRIES);
    _state = OtaState::FAILED;
    return false;
}

bool ESP8266OtaPlatform::performOTA(const String &url, const String &auth) {
    _state = OtaState::DOWNLOADING;

    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Accept any certificate

    HTTPClient http;
    if (!http.begin(secureClient, url)) {
        publishStatus(OtaStatus::ERR_HTTP);
        _state = OtaState::FAILED;
        return false;
    }

    http.setTimeout(60000);
    http.addHeader("Accept-Encoding", "identity");

    if (!auth.isEmpty()) {
        http.addHeader("Authorization", auth);
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        publishStatus(OtaStatus::ERR_HTTP);
        http.end();
        _state = OtaState::FAILED;
        return false;
    }

    int contentLength = http.getSize();

    if (contentLength <= 0) {
        publishStatus(OtaStatus::ERR_HTTP);
        http.end();
        _state = OtaState::FAILED;
        return false;
    }

    if (!Update.begin(contentLength)) {
        publishStatus(OtaStatus::ERR_NO_SPACE);
        http.end();
        _state = OtaState::FAILED;
        return false;
    }

    _state = OtaState::UPDATING;
    publishStatus(OtaStatus::INSTALLING);

    WiFiClient *stream     = http.getStreamPtr();
    uint8_t     buff[BUFFER_SIZE];
    size_t      written    = 0;
    uint32_t    lastDataMs = millis();

    while (written < (size_t)contentLength) {
        size_t available = stream->available();

        if (available > 0) {
            lastDataMs = millis();

            size_t toRead = (available < sizeof(buff)) ? available : sizeof(buff);
            int readBytes = stream->readBytes(buff, toRead);

            if (readBytes <= 0 || Update.write(buff, readBytes) != (size_t)readBytes) {
                publishStatus(OtaStatus::ERR_WRITE);
                Update.end();
                http.end();
                _state = OtaState::FAILED;
                return false;
            }

            written += readBytes;
            reportProgress(written, contentLength);
        } else {
            if (millis() - lastDataMs > (uint32_t)_timeoutMs) {
                publishStatus(OtaStatus::ERR_TIMEOUT);
                Update.end();
                http.end();
                _state = OtaState::FAILED;
                return false;
            }
            delay(1);
            yield(); // Prevent ESP8266 watchdog reset
        }
    }

    http.end();

    if (!Update.end(true) || !Update.isFinished()) {
        publishStatus(OtaStatus::ERR_FINALIZE);
        _state = OtaState::FAILED;
        return false;
    }

    _state = OtaState::SUCCESS;
    return true;
}

#endif // ESP8266
