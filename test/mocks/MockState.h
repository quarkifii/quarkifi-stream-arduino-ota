// Scriptable state + call recordings for all Arduino mocks.
// mock::reset() runs automatically before every TEST; tests then configure
// mock::state before exercising library code.
#pragma once

#include "MockNet.h"
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace mock {

// One scripted HTTP response, consumed by each HTTPClient::begin()
// (the last queued response is reused once the queue runs dry).
struct HttpResponse {
    bool beginOk = true;
    int code = HTTP_CODE_OK;
    int sizeOverride = -2;             // -2 = body.size(); otherwise returned by getSize()
    std::vector<uint8_t> body;
    std::map<std::string, std::string> headers;  // response headers (see collectHeaders)
    size_t maxAvailable = 4096;        // bytes reported by available() per call
    size_t stallAfter = SIZE_MAX;      // available() returns 0 forever after this many bytes
    size_t disconnectAfter = SIZE_MAX; // connected() returns false after this many bytes
    size_t readZeroAt = SIZE_MAX;      // readBytes() returns 0 at this offset
    unsigned long msPerRead = 0;       // virtual time consumed by each readBytes()
};

// What the library did with one HTTPClient::begin().
struct HttpRecord {
    std::string url;
    std::string method;                // "GET", "HEAD", ...
    bool secureClient = false;
    bool insecure = false;
    std::string caCert;                // ESP32 setCACert()
    const void *trustAnchors = nullptr;// ESP8266 setTrustAnchors()
    long x509Time = 0;                 // ESP8266 setX509Time()
    int timeout = -1;
    bool followRedirects = false;
    bool http10 = false;
    std::vector<std::string> collected;
    std::map<std::string, std::string> headers;  // request headers
    bool ended = false;
};

struct State {
    // ---- configuration ----
    wl_status_t wifiStatus = WL_CONNECTED;
    std::deque<HttpResponse> responses;
    bool updateBeginOk = true;
    size_t updateWriteShortAt = SIZE_MAX; // write() returns len-1 past this many bytes
    bool updateEndOk = true;
    std::string imageMd5;                 // MD5 the mock "computes" for the written image
    uint32_t epoch = 1793000000UL;        // system clock (ESP8266 tests); 0 = not set

    // ---- recordings ----
    unsigned long nowMs = 0;
    std::vector<unsigned long> delays;
    int yieldCount = 0;
    int restartCount = 0;
    int resetCount = 0;
    int wifiDisconnectCount = 0;

    std::vector<HttpRecord> http;         // one per begin()
    int httpLive = 0;                     // live HTTPClient objects
    int x509Live = 0;                     // live BearSSL::X509List objects

    int updateBeginCount = 0;
    size_t updateBeginSize = 0;
    std::string updateExpectedMd5;
    std::vector<uint8_t> updateData;
    std::vector<size_t> updateWriteSizes;
    int updateEndCount = 0;
    bool lastEndEvenIfRemaining = false;
    bool updateFinalized = false;
};

extern State state;

void reset();

// Helpers
std::vector<uint8_t> makeBody(size_t n);
HttpResponse okResponse(size_t bodySize);

}  // namespace mock
