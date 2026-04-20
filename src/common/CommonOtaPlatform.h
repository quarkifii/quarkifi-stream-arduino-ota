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
// Shared base class for all OTA platform implementations.
// Provides:
//   - Status callback management (byte status + byte progress)
//   - Internal state tracking
//   - Retry and timeout configuration
//   - Progress reporting (throttled to once per second)
// ========================================================

class CommonOtaPlatform : public IOtaPlatform {
public:
    static const int DEFAULT_MAX_RETRIES = 3;
    static const int DEFAULT_TIMEOUT_MS  = 20000; // 20s no-data timeout
    static const int BUFFER_SIZE         = 1024;

    CommonOtaPlatform();
    virtual ~CommonOtaPlatform() {}

    void setCallback(void (*callback)(const byte status, const byte progress)) override;

    OtaState getState() const;
    bool     isBusy()   const;

protected:
    void (*_callback)(const byte status, const byte progress);
    OtaState _state;
    int      _maxRetries;
    int      _timeoutMs;

    // Emit a status code (with optional progress) to the registered callback.
    void publishStatus(byte status, byte progress = 0);

    // Report download progress — throttled to once per second.
    // Emits OtaStatus::DOWNLOADING with progress 0–100.
    void reportProgress(size_t written, size_t total);

private:
    unsigned long _lastProgressMs;
};
