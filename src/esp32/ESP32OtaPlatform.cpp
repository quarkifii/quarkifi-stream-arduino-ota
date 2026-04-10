#if defined(ESP32)

#include "ESP32OtaPlatform.h"

// ================== ESP32OtaPlatform ==================

ESP32OtaPlatform::ESP32OtaPlatform() {}

bool ESP32OtaPlatform::begin() {
    _state = OtaState::IDLE;
    return true;
}

bool ESP32OtaPlatform::startOTA(const String &url, const String &auth) {
    if (isBusy()) {
        publishStatus("ignored: already updating");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        publishStatus("failed: wifi not connected");
        return false;
    }

    for (int attempt = 1; attempt <= _maxRetries; attempt++) {
        publishStatus("starting (try " + String(attempt) + ")");

        if (performOTA(url, auth)) {
            publishStatus("success");
            delay(500);
            publishStatus("restarting");
            delay(1000);
            ESP.restart();
            return true; // Unreachable, but satisfies the compiler
        }

        if (attempt < _maxRetries) {
            publishStatus("retrying...");
            delay(2000);
        }
    }

    publishStatus("failed after retries");
    _state = OtaState::FAILED;
    return false;
}

bool ESP32OtaPlatform::performOTA(const String &url, const String &auth) {
    _state = OtaState::DOWNLOADING;

    bool isHttps = url.startsWith("https://");

    WiFiClient     plainClient;
    WiFiClientSecure secureClient;

    if (isHttps) {
        secureClient.setInsecure(); // Accept any certificate
    }

    WiFiClient *clientPtr = isHttps
        ? static_cast<WiFiClient *>(&secureClient)
        : &plainClient;

    HTTPClient http;
    http.begin(*clientPtr, url);
    http.setTimeout(60000);
    http.addHeader("Accept-Encoding", "identity");

    if (!auth.isEmpty()) {
        http.addHeader("Authorization", auth);
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        publishStatus("failed: http " + String(httpCode));
        http.end();
        _state = OtaState::FAILED;
        return false;
    }

    int contentLength = http.getSize();

    if (contentLength <= 0) {
        publishStatus("failed: invalid content length");
        http.end();
        _state = OtaState::FAILED;
        return false;
    }

    if (!Update.begin(contentLength)) {
        publishStatus("failed: not enough space");
        http.end();
        _state = OtaState::FAILED;
        return false;
    }

    _state = OtaState::UPDATING;

    WiFiClient *stream    = http.getStreamPtr();
    uint8_t     buff[1024];
    size_t      written       = 0;
    uint32_t    lastDataMs    = millis();

    while (http.connected() && written < (size_t)contentLength) {
        size_t available = stream->available();

        if (available > 0) {
            lastDataMs = millis();

            int readBytes = stream->readBytes(buff, min(available, sizeof(buff)));

            if (Update.write(buff, readBytes) != (size_t)readBytes) {
                publishStatus("failed: write error");
                Update.end();
                http.end();
                _state = OtaState::FAILED;
                return false;
            }

            written += readBytes;
            reportProgress(written, contentLength);
        } else {
            if (millis() - lastDataMs > (uint32_t)_timeoutMs) {
                publishStatus("failed: timeout");
                Update.end();
                http.end();
                _state = OtaState::FAILED;
                return false;
            }
            delay(1);
        }
    }

    http.end();

    if (!Update.end()) {
        publishStatus("failed: finalize error");
        _state = OtaState::FAILED;
        return false;
    }

    if (!Update.isFinished()) {
        publishStatus("failed: incomplete");
        _state = OtaState::FAILED;
        return false;
    }

    _state = OtaState::SUCCESS;
    return true;
}

#endif // ESP32
