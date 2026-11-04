#include "MockState.h"

#include <algorithm>
#include <cctype>
#include <cstring>

EspClass ESP;
WiFiClass WiFi;
UpdateClass Update;

namespace mock {

State state;

namespace {

// Stream returned by HTTPClient::getStreamPtr() for the active response.
class MockStream : public WiFiClient {
public:
    HttpResponse resp;
    size_t pos = 0;

    int available() override {
        if (pos >= resp.body.size() || pos >= resp.stallAfter) return 0;
        size_t left = std::min(resp.body.size(), resp.stallAfter) - pos;
        return (int)std::min(left, resp.maxAvailable);
    }

    size_t readBytes(uint8_t *buf, size_t len) override {
        state.nowMs += resp.msPerRead;
        if (pos >= resp.readZeroAt) return 0;
        size_t n = std::min(len, (size_t)available());
        if (resp.readZeroAt != SIZE_MAX) n = std::min(n, resp.readZeroAt - pos);
        std::copy(resp.body.begin() + pos, resp.body.begin() + pos + n, buf);
        pos += n;
        return n;
    }

    uint8_t connected() override { return pos < resp.disconnectAfter ? 1 : 0; }
};

MockStream g_stream;

}  // namespace

void reset() {
    state = State();
    g_stream = MockStream();
}

std::vector<uint8_t> makeBody(size_t n) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; i++) v[i] = (uint8_t)((i * 31 + 7) & 0xFF);
    return v;
}

HttpResponse okResponse(size_t bodySize) {
    HttpResponse r;
    r.body = makeBody(bodySize);
    return r;
}

}  // namespace mock

using mock::state;

// ---------------------------------------------------------------- Arduino core
unsigned long millis() { return state.nowMs; }
void delay(unsigned long ms) {
    state.nowMs += ms;
    state.delays.push_back(ms);
}
void yield() { state.yieldCount++; }
void EspClass::restart() { state.restartCount++; }
void EspClass::reset() { state.resetCount++; }

// ---------------------------------------------------------------- WiFi
wl_status_t WiFiClass::status() { return state.wifiStatus; }
bool WiFiClass::disconnect(bool) {
    state.wifiDisconnectCount++;
    return true;
}

// ---------------------------------------------------------------- TLS
#if defined(ESP8266)
BearSSL::X509List::X509List(const char *pem) : _pem(pem ? pem : ""), _count(0) {
    state.x509Live++;
    const std::string marker = "-----BEGIN CERTIFICATE-----";
    for (size_t p = _pem.find(marker); p != std::string::npos; p = _pem.find(marker, p + 1)) _count++;
}
BearSSL::X509List::~X509List() { state.x509Live--; }
#endif

// ---------------------------------------------------------------- HTTPClient
HTTPClient::HTTPClient() { state.httpLive++; }
HTTPClient::~HTTPClient() { state.httpLive--; }

bool HTTPClient::begin(WiFiClient &client, const String &url) {
    mock::HttpRecord rec;
    rec.url = url.str();
    WiFiClientSecure *sec = dynamic_cast<WiFiClientSecure *>(&client);
    rec.secureClient = sec != nullptr;
    if (sec) {
        rec.insecure = sec->insecure();
        rec.caCert = sec->caCert();
        rec.trustAnchors = sec->trustAnchors();
        rec.x509Time = (long)sec->x509Time();
    }
    state.http.push_back(rec);

    mock::HttpResponse resp;
    if (!state.responses.empty()) {
        resp = state.responses.front();
        if (state.responses.size() > 1) state.responses.pop_front();
    }
    mock::g_stream = mock::MockStream();
    mock::g_stream.resp = resp;
    return resp.beginOk;
}

void HTTPClient::setTimeout(uint16_t ms) { state.http.back().timeout = ms; }
void HTTPClient::setFollowRedirects(followRedirects_t f) {
    state.http.back().followRedirects = f != HTTPC_DISABLE_FOLLOW_REDIRECTS;
}
void HTTPClient::useHTTP10(bool v) { state.http.back().http10 = v; }
void HTTPClient::addHeader(const String &name, const String &value) {
    state.http.back().headers[name.str()] = value.str();
}
void HTTPClient::collectHeaders(const char *keys[], const size_t n) {
    _collect.assign(keys, keys + n);
    state.http.back().collected = _collect;
}
String HTTPClient::header(const char *name) {
    // Like the real client: only headers registered via collectHeaders() are kept.
    if (std::find(_collect.begin(), _collect.end(), name) == _collect.end()) return String();
    auto it = mock::g_stream.resp.headers.find(name);
    return it == mock::g_stream.resp.headers.end() ? String() : String(it->second);
}
int HTTPClient::GET() { return sendRequest("GET"); }
int HTTPClient::sendRequest(const char *type, uint8_t *, size_t) {
    state.http.back().method = type;
    return mock::g_stream.resp.code;
}
int HTTPClient::getSize() {
    const mock::HttpResponse &r = mock::g_stream.resp;
    return r.sizeOverride != -2 ? r.sizeOverride : (int)r.body.size();
}
WiFiClient *HTTPClient::getStreamPtr() { return &mock::g_stream; }
bool HTTPClient::connected() { return mock::g_stream.connected(); }
void HTTPClient::end() {
    if (!state.http.empty()) state.http.back().ended = true;
}

// ---------------------------------------------------------------- Update
bool UpdateClass::begin(size_t size) {
    state.updateBeginCount++;
    state.updateBeginSize = size;
    state.updateData.clear();
    state.updateExpectedMd5.clear();
    state.updateFinalized = false;
    return state.updateBeginOk;
}

bool UpdateClass::setMD5(const char *md5) {
    if (!md5 || strlen(md5) != 32) return false;  // same check as both cores
    state.updateExpectedMd5 = md5;
    return true;
}

size_t UpdateClass::write(uint8_t *data, size_t len) {
    state.updateWriteSizes.push_back(len);
    if (state.updateData.size() + len > state.updateWriteShortAt) return len - 1;
    state.updateData.insert(state.updateData.end(), data, data + len);
    return len;
}

bool UpdateClass::isFinished() { return state.updateData.size() == state.updateBeginSize; }

bool UpdateClass::end(bool evenIfRemaining) {
    state.updateEndCount++;
    state.lastEndEvenIfRemaining = evenIfRemaining;
    if (!state.updateEndOk) return false;
    if (!isFinished() && !evenIfRemaining) return false;  // aborts, like the real cores
    if (evenIfRemaining) state.updateBeginSize = state.updateData.size();
    if (!state.updateExpectedMd5.empty()) {
        std::string a = state.updateExpectedMd5, b = state.imageMd5;
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
        if (a != b) return false;  // MD5 mismatch: image not committed
    }
    state.updateFinalized = true;
    return true;
}
