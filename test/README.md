# QuarkifiStreamOTA host tests

Unit tests that compile the library on a PC against Arduino/ESP mocks — no board needed.

### With Docker (nothing else to install)

```sh
./test/run-tests.sh --all        # ESP32 + ESP8266 tests
./test/run-tests.sh --esp32      # one platform (or --esp8266)
./test/run-tests.sh --sanitize   # with AddressSanitizer + UBSan
```

The script builds a small `debian:bookworm-slim` image with `g++` and `make`,
copies `src/` and `test/` into it, and runs `make` there. Nothing is written
to your working tree. The exit code is non-zero if any test fails, so it can be
used in CI as-is.

### Natively

```sh
cd test
make            # builds and runs both ESP32 and ESP8266 variants
make esp32      # or just one platform
make sanitize   # AddressSanitizer + UBSan
```

Requires `g++` (C++17) and `make`.

## Layout

| Path | Purpose |
|------|---------|
| `mocks/` | Fake `Arduino.h`, `WiFi.h`, `WiFiClientSecure.h`, `HTTPClient.h`, `Update.h`, ESP8266 equivalents, `base64.h`, `mbedtls/base64.h` |
| `mocks/MockState.h` | Scriptable state (`mock::state`): Wi‑Fi status, HTTP responses (status, body, headers) per request, stalls/disconnects, flash/MD5 failures, virtual clock, system epoch, and call recordings (method, headers, TLS mode, CA cert, trust anchors, X509 time) |
| `framework/` | Tiny test runner (`TEST`, `EXPECT_EQ`, `ASSERT_TRUE`, `KNOWN_ISSUE`) |
| `test_common.cpp` | `CommonOtaPlatform` (callback, state, argument validation, progress reporting) |
| `test_facade.cpp` | `QuarkifiStreamArduinoOTA` (`buildAuth`, `startOTA`, `setCallback`, `setRootCA`) |
| `test_platform.cpp` | `ESP32OtaPlatform` / `ESP8266OtaPlatform` — success, retried vs permanent errors, every status code, timeouts, MD5, root CA, ESP8266 `Date`-header time, restart sequence |
| `test_httpdate.cpp` | `HttpDate::parse` — IMF-fixdate, RFC 850, asctime, leap years, invalid input |

Time is virtual: `millis()` only moves when the code calls `delay()` or a mock
stream read consumes time, so timeout tests run instantly.

`KNOWN_ISSUE(desc, correctCondition)` (currently unused) records a bug still present in the library
without failing the run. When a fix lands the runner prints
`KNOWN ISSUE NOW PASSES` — turn that check into an `EXPECT_*`.

Platform-specific mock APIs (`setCACert` on ESP32; `setTrustAnchors`,
`setX509Time`, `BearSSL::X509List` on ESP8266) only exist in that platform's
build, so calling the wrong one fails to compile — as it would on the real core.
