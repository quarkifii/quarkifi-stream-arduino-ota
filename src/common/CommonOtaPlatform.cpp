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

#include "CommonOtaPlatform.h"

// ================== CommonOtaPlatform ==================

CommonOtaPlatform::CommonOtaPlatform()
    : _callback(nullptr),
      _state(OtaState::IDLE),
      _maxRetries(DEFAULT_MAX_RETRIES),
      _timeoutMs(DEFAULT_TIMEOUT_MS),
      _lastProgressMs(0)
{}

void CommonOtaPlatform::setCallback(void (*callback)(const byte status, const byte progress)) {
    _callback = callback;
}

OtaState CommonOtaPlatform::getState() const {
    return _state;
}

bool CommonOtaPlatform::isBusy() const {
    return _state == OtaState::DOWNLOADING || _state == OtaState::UPDATING;
}

void CommonOtaPlatform::publishStatus(byte status, byte progress) {
    if (_callback) {
        _callback(status, progress);
    }
}

void CommonOtaPlatform::reportProgress(size_t written, size_t total) {
    if (total == 0) return;

    unsigned long now = millis();
    if (now - _lastProgressMs < 1000) return;
    _lastProgressMs = now;

    byte percent = (byte)((written * 100UL) / total);
    publishStatus(OtaStatus::DOWNLOADING, percent);
}
