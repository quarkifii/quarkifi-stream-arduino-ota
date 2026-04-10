#include "QstreamArduinoOta.h"

#if defined(ESP32)
    #include "mbedtls/base64.h"
#elif defined(ESP8266)
    #include "base64.h"
#endif

// ================== Platform Selection ==================
#if defined(ESP32)
    #include "esp32/ESP32OtaPlatform.h"
#elif defined(ESP8266)
    #include "esp8266/ESP8266OtaPlatform.h"
#else
    #error "QstreamArduinoOta: Unsupported platform. Only ESP32 and ESP8266 are supported."
#endif

// ================== QstreamArduinoOta ==================

QstreamArduinoOta::QstreamArduinoOta()
    : _platform(nullptr)
{}

QstreamArduinoOta::~QstreamArduinoOta() {
    delete _platform;
}

void QstreamArduinoOta::begin() {
#if defined(ESP32)
    _platform = new ESP32OtaPlatform();
#elif defined(ESP8266)
    _platform = new ESP8266OtaPlatform();
#endif

    _platform->begin();
}

void QstreamArduinoOta::setCallback(void (*callback)(const String &status)) {
    if (_platform) {
        _platform->setCallback(callback);
    }
}

bool QstreamArduinoOta::startOTA(const String &url, const String &auth) {
    if (!_platform) return false;
    return _platform->startOTA(url, auth);
}

String QstreamArduinoOta::buildAuth(const String &username, const String &password) {
    if (username.isEmpty() || password.isEmpty()) return "";

    String credentials = username + ":" + password;

#if defined(ESP32)
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
    return "Basic " + base64::encode(credentials);

#else
    return "";
#endif
}
