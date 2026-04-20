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

// ================== OtaStatus ==================
// Status codes passed to the onOtaStatus callback.
//
// Callback signature:
//   void onOtaStatus(const byte status, const byte progress)
//
// `progress` is only meaningful when status == OtaStatus::DOWNLOADING.
//   0   = not yet started
//   1–99 = in progress
//   100 = download complete
//
// For all other status codes, progress is always 0.
//
// -----------------------------------------------
// Code  | Constant        | Meaning
// -----------------------------------------------
//   0   | STARTING        | OTA attempt is beginning (one per retry)
//   1   | DOWNLOADING     | Firmware download in progress; see progress
//   2   | INSTALLING      | Writing firmware to flash
//   3   | SUCCESS         | Firmware written and verified successfully
//   4   | RESTARTING      | Device is about to restart with new firmware
// -----------------------------------------------
//  10   | ERR_BUSY        | OTA already in progress; request ignored
//  11   | ERR_NO_WIFI     | Wi-Fi not connected; cannot start OTA
//  12   | ERR_HTTP        | HTTP request failed (non-200 response)
//  13   | ERR_NO_SPACE    | Not enough flash space for the firmware
//  14   | ERR_WRITE       | Flash write error during update
//  15   | ERR_TIMEOUT     | No data received within the timeout window
//  16   | ERR_FINALIZE    | Update.end() failed; firmware not committed
//  17   | ERR_RETRIES     | All retry attempts exhausted; OTA abandoned
// -----------------------------------------------

namespace OtaStatus {
    // ---- Lifecycle ----
    const byte STARTING    = 0;
    const byte DOWNLOADING = 1;
    const byte INSTALLING  = 2;
    const byte SUCCESS     = 3;
    const byte RESTARTING  = 4;

    // ---- Errors ----
    const byte ERR_BUSY    = 10;
    const byte ERR_NO_WIFI = 11;
    const byte ERR_HTTP    = 12;
    const byte ERR_NO_SPACE= 13;
    const byte ERR_WRITE   = 14;
    const byte ERR_TIMEOUT = 15;
    const byte ERR_FINALIZE= 16;
    const byte ERR_RETRIES = 17;
}
