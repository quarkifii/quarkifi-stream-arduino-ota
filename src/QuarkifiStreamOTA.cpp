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

#include "QuarkifiStreamOTA.h"

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
    #error "QuarkifiStreamArduinoOTA: Unsupported platform. Only ESP32 and ESP8266 are supported."
#endif

// ================== QuarkifiStreamArduinoOTA ==================

QuarkifiStreamArduinoOTA::QuarkifiStreamArduinoOTA() {
#if defined(ESP32)
    _platform = new ESP32OtaPlatform();
#elif defined(ESP8266)
    _platform = new ESP8266OtaPlatform();
#else
    _platform = nullptr;
#endif
}

QuarkifiStreamArduinoOTA::~QuarkifiStreamArduinoOTA() {
    delete _platform;
}

void QuarkifiStreamArduinoOTA::setCallback(void (*callback)(const byte status, const byte progress)) {
    if (_platform) {
        _platform->setCallback(callback);
    }
}

void QuarkifiStreamArduinoOTA::setRootCA(const char *rootCaPem) {
    if (_platform) {
        _platform->setRootCA(rootCaPem);
    }
}

bool QuarkifiStreamArduinoOTA::startOTA(const String &url, const String &auth, const String &md5) {
    if (!_platform) return false;
    return _platform->startOTA(url, auth, md5);
}

String QuarkifiStreamArduinoOTA::buildAuth(const String &username, const String &password) {
    if (username.isEmpty()) return "";

    String credentials = username + ":" + password;

#if defined(ESP32)
    size_t outputLen = 0;
    mbedtls_base64_encode(nullptr, 0, &outputLen,
                          (const unsigned char *)credentials.c_str(),
                          credentials.length());
    unsigned char *encoded = new unsigned char[outputLen + 1];
    if (mbedtls_base64_encode(encoded, outputLen + 1, &outputLen,
                              (const unsigned char *)credentials.c_str(),
                              credentials.length()) != 0) {
        delete[] encoded;
        return "";
    }
    encoded[outputLen] = '\0';
    String result = "Basic " + String((char *)encoded);
    delete[] encoded;
    return result;

#elif defined(ESP8266)
    return "Basic " + base64::encode(credentials, false);   // no line wrapping (older cores default to true)

#else
    return "";
#endif
}
