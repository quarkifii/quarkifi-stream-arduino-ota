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

#pragma once

#include <Arduino.h>

#if defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Updater.h>
#endif
#include <WiFiClientSecure.h>

#include "../interfaces/IOtaPlatform.h"
#include "../interfaces/OtaStatus.h"

// ================== OtaState ==================
// Internal state machine — not exposed to the user.
// Users receive OtaStatus codes via the callback instead.
enum class OtaState {
    IDLE,
    DOWNLOADING,
    UPDATING,
    SUCCESS,
    FAILED
};

// ================== CommonOtaPlatform ==================
// Shared implementation for all OTA platforms:
//   - Status callback management (byte status + byte progress)
//   - Argument validation, retry loop, permanent-vs-transient errors
//   - HTTP(S) download + flash write, MD5 verification
//   - Progress reporting (0 first, 100 last, throttled to 1/s between)
// Platform subclasses only provide TLS configuration and restart.
// ========================================================

class CommonOtaPlatform : public IOtaPlatform {
public:
    static const int DEFAULT_MAX_RETRIES = 3;
    static const int DEFAULT_TIMEOUT_MS  = 20000;   // 20s no-data timeout
    static const int BUFFER_SIZE         = 1024;
    static const int RETRY_DELAY_MS      = 2000;

    CommonOtaPlatform();
    virtual ~CommonOtaPlatform() {}

    // Not copyable — subclasses may own TLS resources.
    CommonOtaPlatform(const CommonOtaPlatform &) = delete;
    CommonOtaPlatform &operator=(const CommonOtaPlatform &) = delete;

    void setCallback(void (*callback)(const byte status, const byte progress)) override;
    void setRootCA(const char *rootCaPem) override;
    bool startOTA(const String &url, const String &auth, const String &md5) override;

    OtaState getState() const;
    bool     isBusy() const;

    // True for 32 hex characters (upper or lower case).
    static bool isValidMd5(const String &md5);

    // Whether a failed HTTP status code is permanent (not worth retrying).
    static bool isPermanentHttpError(int httpCode);

protected:
    enum class Attempt { OK, RETRY, ABORT };

    void (*_callback)(const byte status, const byte progress);
    OtaState _state;
    int      _maxRetries;
    int      _timeoutMs;
    String   _rootCA;

    // ---- platform hooks ----
    // Configure the TLS client for an https:// URL (verify or insecure).
    // Publish an error and return RETRY/ABORT to fail the attempt.
    virtual Attempt configureTls(WiFiClientSecure &client, const String &url) = 0;
    // Commit the written image (Update.end(...)).
    virtual bool finalizeUpdate() = 0;
    // Reboot into the new firmware. Called after RESTARTING is published.
    virtual void restartDevice() = 0;
    // Called whenever the root CA changes.
    virtual void onRootCAChanged() {}

    // Execute one OTA attempt.
    Attempt performOTA(const String &url, const String &auth, const String &md5);

    // Emit a status code (with optional progress) to the registered callback.
    void publishStatus(byte status, byte progress = 0);

    // Report download progress. The first call (0%) and the final call
    // (written == total, 100%) are always emitted; calls in between are
    // throttled to once per second and de-duplicated.
    void reportProgress(size_t written, size_t total);
    void resetProgress();

private:
    Attempt fail(Attempt result);

    bool          _running;
    bool          _progressStarted;
    byte          _lastPercent;
    unsigned long _lastProgressMs;
};
