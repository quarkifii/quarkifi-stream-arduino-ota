#include "CommonOtaPlatform.h"

#if defined(ESP32)
    #include "mbedtls/base64.h"
#elif defined(ESP8266)
    #include "base64.h"
#endif

// ================== CommonOtaPlatform ==================

CommonOtaPlatform::CommonOtaPlatform()
    : _callback(nullptr),
      _state(OtaState::IDLE),
      _maxRetries(DEFAULT_MAX_RETRIES),
      _timeoutMs(DEFAULT_TIMEOUT_MS),
      _lastProgressMs(0)
{}

void CommonOtaPlatform::setCallback(void (*callback)(const String &status)) {
    _callback = callback;
}

OtaState CommonOtaPlatform::getState() const {
    return _state;
}

bool CommonOtaPlatform::isBusy() const {
    return _state == OtaState::DOWNLOADING || _state == OtaState::UPDATING;
}

void CommonOtaPlatform::publishStatus(const String &status) {
    if (_callback) {
        _callback(status);
    }
}

String CommonOtaPlatform::buildBasicAuth(const String &username, const String &password) {
    if (username.isEmpty() || password.isEmpty()) return "";

    String credentials = username + ":" + password;

#if defined(ESP32)
    // ESP32: use mbedTLS
    size_t outputLen = 0;
    mbedtls_base64_encode(nullptr, 0, &outputLen,
                          (const unsigned char *)credentials.c_str(),
                          credentials.length());

    unsigned char *encoded = new unsigned char[outputLen + 1];
    mbedtls_base64_encode(encoded, outputLen + 1, &outputLen,
                          (const unsigned char *)credentials.c_str(),
                          credentials.length());
    encoded[outputLen] = '\0';

    String result = "Basic " + String((char *)encoded);
    delete[] encoded;
    return result;

#elif defined(ESP8266)
    // ESP8266: use built-in base64 helper
    return "Basic " + base64::encode(credentials);

#else
    return "";
#endif
}

void CommonOtaPlatform::reportProgress(size_t written, size_t total) {
    if (total == 0) return;

    unsigned long now = millis();
    if (now - _lastProgressMs < 1000) return;

    _lastProgressMs = now;

    int percent = (int)((written * 100UL) / total);
    publishStatus("downloading: " + String(percent) + "%");
}
