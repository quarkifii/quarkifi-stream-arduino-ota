// Host-side mocks for WiFi, WiFiClient(Secure), HTTPClient and Update/Updater.
// Behaviour is scripted through the `mock` namespace (see MockState.h).
// Platform-only APIs are exposed only for that platform's build so the host
// compile catches calls that wouldn't exist on the real core.
#pragma once

#include "Arduino.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------- WiFi
typedef enum {
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_DISCONNECTED = 6
} wl_status_t;

class WiFiClass {
public:
    wl_status_t status();
    bool disconnect(bool wifioff = false);
};
extern WiFiClass WiFi;

// ---------------------------------------------------------------- Clients
class WiFiClient {
public:
    virtual ~WiFiClient() {}
    virtual int available() { return 0; }
    virtual size_t readBytes(uint8_t *buf, size_t len) { (void)buf; (void)len; return 0; }
    virtual uint8_t connected() { return 0; }
};

#if defined(ESP8266)
namespace BearSSL {
// Counts "-----BEGIN CERTIFICATE-----" blocks like the real parser's getCount().
class X509List {
public:
    explicit X509List(const char *pem);
    ~X509List();
    size_t getCount() const { return _count; }
    const std::string &pem() const { return _pem; }
private:
    std::string _pem;
    size_t _count;
};
}  // namespace BearSSL
#endif

class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() { _insecure = true; }
#if defined(ESP32)
    void setCACert(const char *rootCA) { _caCert = rootCA ? rootCA : ""; _caCertPtr = rootCA; }
#elif defined(ESP8266)
    void setTrustAnchors(const BearSSL::X509List *ta) { _trustAnchors = ta; }
    void setX509Time(time_t now) { _x509Time = now; }
#endif

    // ---- test inspection ----
    bool insecure() const { return _insecure; }
    std::string caCert() const { return _caCert; }
    const char *caCertPtr() const { return _caCertPtr; }
    const void *trustAnchors() const { return _trustAnchors; }
    time_t x509Time() const { return _x509Time; }

private:
    bool _insecure = false;
    std::string _caCert;
    const char *_caCertPtr = nullptr;
    const void *_trustAnchors = nullptr;
    time_t _x509Time = 0;
};

// ---------------------------------------------------------------- HTTPClient
enum { HTTP_CODE_OK = 200, HTTP_CODE_NOT_FOUND = 404, HTTP_CODE_UNAUTHORIZED = 401 };
#define HTTPC_ERROR_CONNECTION_REFUSED (-1)

typedef enum {
    HTTPC_DISABLE_FOLLOW_REDIRECTS,
    HTTPC_STRICT_FOLLOW_REDIRECTS,
    HTTPC_FORCE_FOLLOW_REDIRECTS
} followRedirects_t;

class HTTPClient {
public:
    HTTPClient();
    ~HTTPClient();
    bool begin(WiFiClient &client, const String &url);
    void setTimeout(uint16_t ms);
    void setFollowRedirects(followRedirects_t follow);
    void useHTTP10(bool usehttp10 = true);
    void addHeader(const String &name, const String &value);
    void collectHeaders(const char *headerKeys[], const size_t headerKeysCount);
    String header(const char *name);
    int GET();
    int sendRequest(const char *type, uint8_t *payload = nullptr, size_t size = 0);
    int getSize();
    WiFiClient *getStreamPtr();
    bool connected();
    void end();
private:
    std::vector<std::string> _collect;
};

// ---------------------------------------------------------------- Update
class UpdateClass {
public:
    bool begin(size_t size);
    bool setMD5(const char *expected_md5);
    size_t write(uint8_t *data, size_t len);
    bool end(bool evenIfRemaining = false);
    bool isFinished();
};
extern UpdateClass Update;
