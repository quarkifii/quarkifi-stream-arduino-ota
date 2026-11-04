// Mock of mbedtls/base64.h (ESP32) with the real buffer-size semantics:
// when dst is too small, *olen receives the required size INCLUDING the NUL
// terminator and MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL is returned.
#pragma once
#include <cstring>
#include "../Base64Impl.h"

#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL -0x002A

inline int mbedtls_base64_encode(unsigned char *dst, size_t dlen, size_t *olen,
                                 const unsigned char *src, size_t slen) {
    std::string enc = mock::base64Encode(src, slen);
    if (dst == nullptr || dlen < enc.size() + 1) {
        *olen = enc.size() + 1;
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }
    memcpy(dst, enc.c_str(), enc.size() + 1);
    *olen = enc.size();
    return 0;
}
