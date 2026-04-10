#pragma once

#include <Arduino.h>
#include "../interfaces/IOtaPlatform.h"

// ================== OTA States ==================
enum class OtaState {
    IDLE,
    DOWNLOADING,
    UPDATING,
    SUCCESS,
    FAILED
};

// ================== CommonOtaPlatform ==================
// Shared base class for all OTA platform implementations.
// Provides:
//   - Status callback management
//   - State tracking
//   - Retry configuration
//   - Progress reporting helpers
//   - Basic Auth encoding
// ========================================================

class CommonOtaPlatform : public IOtaPlatform {
public:
    static const int DEFAULT_MAX_RETRIES = 2;
    static const int DEFAULT_TIMEOUT_MS  = 20000;  // 20s no-data timeout
    static const int BUFFER_SIZE         = 256;

    CommonOtaPlatform();
    virtual ~CommonOtaPlatform() {}

    void setCallback(void (*callback)(const String &status)) override;

    OtaState getState() const;
    bool     isBusy()   const;

protected:
    void (*_callback)(const String &status);
    OtaState _state;
    int      _maxRetries;
    int      _timeoutMs;

    // Emit a status string to the registered callback (and Serial)
    void publishStatus(const String &status);

    // Encode username:password as "Basic <base64>"
    // Returns empty string if either argument is empty
    String buildBasicAuth(const String &username, const String &password);

    // Report download progress — throttled to once per second
    void reportProgress(size_t written, size_t total);

private:
    unsigned long _lastProgressMs;
};
