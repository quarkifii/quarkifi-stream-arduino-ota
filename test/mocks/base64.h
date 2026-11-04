// Mock of the ESP8266 core's base64.h (core >= 3.0 signature).
#pragma once
#include "Arduino.h"
#include "Base64Impl.h"

class base64 {
public:
    static String encode(const String &text, bool doNewLines = false) {
        std::string enc = mock::base64Encode((const unsigned char *)text.c_str(), text.length());
        if (doNewLines) {
            // Legacy cores wrapped output every 72 chars.
            for (size_t i = 72; i < enc.size(); i += 73) enc.insert(i, "\n");
        }
        return String(enc);
    }
};
