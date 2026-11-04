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

#if defined(ESP32) || defined(ESP8266)

#include "CommonOtaPlatform.h"

// ================== CommonOtaPlatform ==================

CommonOtaPlatform::CommonOtaPlatform()
    : _callback(nullptr),
      _state(OtaState::IDLE),
      _maxRetries(DEFAULT_MAX_RETRIES),
      _timeoutMs(DEFAULT_TIMEOUT_MS),
      _running(false),
      _progressStarted(false),
      _lastPercent(0),
      _lastProgressMs(0)
{}

void CommonOtaPlatform::setCallback(void (*callback)(const byte status, const byte progress)) {
    _callback = callback;
}

void CommonOtaPlatform::setRootCA(const char *rootCaPem) {
    _rootCA = rootCaPem ? rootCaPem : "";
    onRootCAChanged();
}

OtaState CommonOtaPlatform::getState() const {
    return _state;
}

bool CommonOtaPlatform::isBusy() const {
    return _running || _state == OtaState::DOWNLOADING || _state == OtaState::UPDATING;
}

bool CommonOtaPlatform::isValidMd5(const String &md5) {
    if (md5.length() != 32) return false;
    for (unsigned int i = 0; i < md5.length(); i++) {
        char c = md5[i];
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

bool CommonOtaPlatform::isPermanentHttpError(int httpCode) {
    // Negative codes are transport errors (connect/TLS/read) — transient.
    // 408 Request Timeout and 429 Too Many Requests are worth retrying, as is 5xx.
    if (httpCode == 408 || httpCode == 429) return false;
    return httpCode >= 300 && httpCode < 500;
}

void CommonOtaPlatform::publishStatus(byte status, byte progress) {
    if (_callback) {
        _callback(status, progress);
    }
}

void CommonOtaPlatform::resetProgress() {
    _progressStarted = false;
    _lastPercent     = 0;
    _lastProgressMs  = 0;
}

void CommonOtaPlatform::reportProgress(size_t written, size_t total) {
    if (total == 0) return;
    if (written > total) written = total;

    byte          percent = (byte)(((uint64_t)written * 100U) / total);
    unsigned long now     = millis();
    bool          final   = written == total;

    if (_progressStarted) {
        if (percent == _lastPercent) return;
        if (!final && now - _lastProgressMs < 1000) return;
    }

    _progressStarted = true;
    _lastPercent     = percent;
    _lastProgressMs  = now;
    publishStatus(OtaStatus::DOWNLOADING, percent);
}

CommonOtaPlatform::Attempt CommonOtaPlatform::fail(Attempt result) {
    _state = OtaState::FAILED;
    return result;
}

bool CommonOtaPlatform::startOTA(const String &url, const String &auth, const String &md5) {
    if (isBusy()) {
        publishStatus(OtaStatus::ERR_BUSY);
        return false;
    }

    bool isHttps = url.startsWith("https://");
    bool isHttp  = url.startsWith("http://");
    bool badArgs = (!isHttps && !isHttp) ||
                   (!md5.isEmpty() && !isValidMd5(md5)) ||
                   (!_rootCA.isEmpty() && !isHttps);   // never downgrade a pinned server to plain HTTP
    if (badArgs) {
        publishStatus(OtaStatus::ERR_BAD_ARGS);
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        publishStatus(OtaStatus::ERR_NO_WIFI);
        return false;
    }

    _running = true;

    for (int attempt = 1; attempt <= _maxRetries; attempt++) {
        resetProgress();
        publishStatus(OtaStatus::STARTING);

        Attempt result = performOTA(url, auth, md5);

        if (result == Attempt::OK) {
            publishStatus(OtaStatus::SUCCESS);
            delay(500);
            publishStatus(OtaStatus::RESTARTING);
            restartDevice();
            _running = false;
            return true;   // Not reached on hardware
        }

        if (result == Attempt::ABORT) {
            _running = false;
            return false;  // Permanent error already published
        }

        if (attempt < _maxRetries) {
            delay(RETRY_DELAY_MS);
        }
    }

    publishStatus(OtaStatus::ERR_RETRIES);
    _state   = OtaState::FAILED;
    _running = false;
    return false;
}

CommonOtaPlatform::Attempt CommonOtaPlatform::performOTA(const String &url, const String &auth,
                                                         const String &md5) {
    _state = OtaState::DOWNLOADING;

    bool isHttps = url.startsWith("https://");

    WiFiClient       plainClient;
    WiFiClientSecure secureClient;

    if (isHttps) {
        Attempt tls = configureTls(secureClient, url);
        if (tls != Attempt::OK) return fail(tls);
    }

    WiFiClient *clientPtr = isHttps
        ? static_cast<WiFiClient *>(&secureClient)
        : &plainClient;

    HTTPClient http;
    if (!http.begin(*clientPtr, url)) {
        publishStatus(OtaStatus::ERR_HTTP);   // malformed URL — retrying won't help
        return fail(Attempt::ABORT);
    }

    http.setTimeout(60000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.useHTTP10(true);                      // forces Content-Length, no chunked encoding
    http.addHeader("Accept-Encoding", "identity");

    if (!auth.isEmpty()) {
        http.addHeader("Authorization", auth);
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        publishStatus(OtaStatus::ERR_HTTP);
        http.end();
        return fail(isPermanentHttpError(httpCode) ? Attempt::ABORT : Attempt::RETRY);
    }

    int contentLength = http.getSize();

    if (contentLength <= 0) {
        publishStatus(OtaStatus::ERR_HTTP);   // server must send Content-Length
        http.end();
        return fail(Attempt::ABORT);
    }

    if (!Update.begin(contentLength)) {
        publishStatus(OtaStatus::ERR_NO_SPACE);
        http.end();
        return fail(Attempt::ABORT);
    }

    if (!md5.isEmpty() && !Update.setMD5(md5.c_str())) {
        publishStatus(OtaStatus::ERR_BAD_ARGS);
        Update.end();
        http.end();
        return fail(Attempt::ABORT);
    }

    WiFiClient *stream     = http.getStreamPtr();
    uint8_t     buff[BUFFER_SIZE];
    size_t      written    = 0;
    uint32_t    lastDataMs = millis();

    reportProgress(0, contentLength);

    while (written < (size_t)contentLength) {
        size_t available = stream->available();

        if (available > 0) {
            lastDataMs = millis();

            size_t remaining = (size_t)contentLength - written;
            size_t toRead    = available < sizeof(buff) ? available : sizeof(buff);
            if (toRead > remaining) toRead = remaining;

            int readBytes = stream->readBytes(buff, toRead);

            if (readBytes <= 0 || Update.write(buff, readBytes) != (size_t)readBytes) {
                publishStatus(OtaStatus::ERR_WRITE);
                Update.end();
                http.end();
                return fail(Attempt::RETRY);
            }

            written += readBytes;
            reportProgress(written, contentLength);
        } else if (!http.connected()) {
            publishStatus(OtaStatus::ERR_HTTP);   // server closed before sending everything
            Update.end();
            http.end();
            return fail(Attempt::RETRY);
        } else if (millis() - lastDataMs > (uint32_t)_timeoutMs) {
            publishStatus(OtaStatus::ERR_TIMEOUT);
            Update.end();
            http.end();
            return fail(Attempt::RETRY);
        } else {
            delay(1);
            yield();   // Keep the watchdog fed (ESP8266)
        }
    }

    http.end();

    _state = OtaState::UPDATING;
    publishStatus(OtaStatus::INSTALLING);

    if (!finalizeUpdate() || !Update.isFinished()) {
        publishStatus(OtaStatus::ERR_FINALIZE);
        return fail(Attempt::RETRY);
    }

    _state = OtaState::SUCCESS;
    return Attempt::OK;
}

#endif  // ESP32 || ESP8266
