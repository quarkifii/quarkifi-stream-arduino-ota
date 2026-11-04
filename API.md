# Quarkifi Stream OTA Usage Details

## Include Library

```arduino
#include <QuarkifiStreamOTA.h>
```

## Setup
Make sure the Arduino IDE is set up for ESP8266 (core 3.x or later) or ESP32 (core 2.x or later).

## Start OTA
Retrieve the OTA details (URL, credentials) from your OTA platform,
e.g. Quarkifi Stream (https://qstream.quarkifi.com).

```arduino
QuarkifiStreamArduinoOTA ota;

ota.setRootCA(ROOT_CA_PEM);              // optional, recommended (see below)
ota.setCallback(onOtaStatus);
String auth = ota.buildAuth(user, pass);
ota.startOTA(url, auth);                 // or ota.startOTA(url, auth, md5)
```

`startOTA()` is **blocking**. It returns `false` if the update fails; on success
the device restarts and the call never returns.

| Method | Description |
|---|---|
| `setCallback(cb)` | Registers `void cb(const byte status, const byte progress)`. |
| `setRootCA(pem)` | Verifies the HTTPS server against this root CA (PEM, copied). `nullptr`/`""` disables verification. While set, `http://` URLs are rejected. |
| `startOTA(url, auth = "", md5 = "")` | Downloads and installs firmware. `auth` is a full `Authorization` header value. `md5` (32 hex chars) is optional; the image is not committed if it does not match. |
| `buildAuth(user, pass)` | Returns `"Basic <base64(user:pass)>"`, or `""` if `user` is empty. An empty password is allowed. |

### Server verification (root CA)

Without `setRootCA()` the TLS connection is encrypted but **not verified**, so
anyone on the network path could serve their own firmware. With a root CA set,
the certificate chain and hostname are checked.

No NTP/clock sync is required:
* **ESP32** does not check certificate dates.
* **ESP8266** (BearSSL) needs a time for the date check. If the system clock is
  already set (e.g. your sketch calls `configTime()`), it is used. Otherwise
  the library sends a `HEAD` request to the OTA host first and uses the
  server's HTTP `Date` header. A forged `Date` can only defeat the expiry
  check; the chain and hostname are still verified.

Use a long-lived root CA rather than the server's own certificate, so the
devices keep working when the server certificate is renewed.

### MD5

Passing `md5` protects against corrupted downloads. It also protects against
tampering only if the MD5 reaches the device over a trusted channel (e.g. MQTT
over TLS), not from the same HTTP server as the firmware.

### Other behaviour

* Redirects are followed (e.g. GitHub release URLs).
* The server must send a `Content-Length` (the request uses HTTP/1.0, so
  chunked encoding is not used).
* Transient errors are retried up to 3 times, 2 s apart. Permanent errors
  (invalid URL, most 3xx/4xx, no Content-Length, not enough flash) stop
  immediately.

## OTA Callback Function

The library reports OTA status on the callback. It can optionally be sent back
to the OTA platform:
 `{ "ota_status": <code>, "ota_progress": <0-100> }`

Successful update order:
`STARTING → DOWNLOADING (0 … 100) → INSTALLING → SUCCESS → RESTARTING`

`DOWNLOADING` always starts at 0 and ends at 100; in between it is reported at
most once per second.

| Code | Constant | Meaning |
|---|---|---|
| 0  | `STARTING`     | OTA attempt is beginning (one per retry) |
| 1  | `DOWNLOADING`  | Firmware downloading; `progress` = 0–100 |
| 2  | `INSTALLING`   | Download complete; verifying and committing |
| 3  | `SUCCESS`      | Firmware written and verified |
| 4  | `RESTARTING`   | Device is about to restart |
| 10 | `ERR_BUSY`     | OTA already in progress; request ignored |
| 11 | `ERR_NO_WIFI`  | Wi-Fi not connected |
| 12 | `ERR_HTTP`     | HTTP/TLS request failed, or connection lost mid-download |
| 13 | `ERR_NO_SPACE` | Not enough flash space |
| 14 | `ERR_WRITE`    | Flash write error |
| 15 | `ERR_TIMEOUT`  | No data received for 20 s |
| 16 | `ERR_FINALIZE` | Verify/commit failed (incl. MD5 mismatch) |
| 17 | `ERR_RETRIES`  | All retry attempts exhausted (final status for transient errors) |
| 18 | `ERR_NO_TIME`  | ESP8266 with root CA: clock not set and no usable server `Date` header |
| 19 | `ERR_BAD_ARGS` | Invalid URL, MD5 or root CA, or `http://` URL while a root CA is set |

For permanent errors (`ERR_BAD_ARGS`, `ERR_NO_SPACE`, `ERR_HTTP` for
404/401/…), that error is the final status and `ERR_RETRIES` is not sent.

```arduino
void onOtaStatus(const byte status, const byte progress) {

    if (status == OtaStatus::STARTING) {
        Serial.println("Starting OTA...");
    } else if (status == OtaStatus::DOWNLOADING) {
        Serial.println("Downloading: " + String(progress) + "%");
    } else if (status == OtaStatus::SUCCESS) {
        Serial.println("Download successful");
    } else if (status == OtaStatus::RESTARTING) {
        Serial.println("Restarting...");
    } else if (status >= 10) {
        Serial.println("Error on OTA: " + String(status));
    }

    // DO NOT CHANGE Below structure if using Quarkifi Stream Platform (https://qstream.quarkifi.com)
    JsonDocument doc;
    doc["ota_status__"]   = status;
    doc["ota_progress__"] = progress;      // 0–100; only meaningful when status == 1 (DOWNLOADING)

    char buffer[64];
    serializeJson(doc, buffer);
    mqttClient->publish(TOPIC_PUB, buffer);
}
```

## Unit tests

Host-side tests with Arduino mocks live in [`test/`](test). Run `./test/run-tests.sh --all` (Docker only) or `make -C test`.

For any issue please reach out to support@quarkifi.com or raise an issue.
