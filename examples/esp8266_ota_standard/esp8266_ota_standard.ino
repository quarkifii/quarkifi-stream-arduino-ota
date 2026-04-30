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

// ============================================================
// Simple_OTA Example
// ============================================================
// Demonstrates Quarkifi Stream Arduino OTA integrated with MQTT on ESP32.
// This demo program can be used to integrate with Quarkifi Stream Platform 
// (https://qstream.quarkifi.com) or any other OTA ready platform
// To triggers an OTA update once set otaReady = true
// Status is printed to Serial using OtaStatus codes.
//
// OtaStatus code reference:
//   STARTING    (0)  — OTA attempt is beginning
//   DOWNLOADING (1)  — Firmware downloading; progress = 0–100
//   INSTALLING  (2)  — Writing firmware to flash
//   SUCCESS     (3)  — Firmware written and verified
//   RESTARTING  (4)  — Device is about to restart
//   ERR_BUSY    (10) — OTA already in progress; request ignored
//   ERR_NO_WIFI (11) — Wi-Fi not connected
//   ERR_HTTP    (12) — HTTP request failed (non-200 response)
//   ERR_NO_SPACE(13) — Not enough flash space
//   ERR_WRITE   (14) — Flash write error
//   ERR_TIMEOUT (15) — No data received within timeout window
//   ERR_FINALIZE(16) — Update.end() failed
//   ERR_RETRIES (17) — All retry attempts exhausted
// ============================================================

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <QuarkifiStreamOTA.h>

// ================== CONFIG ==================
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASS     = "YOUR_PASSWORD";

const char* OTA_URL       = "https://example.com/firmware.bin";
const char* OTA_USER      = "OTA_USERNAME";   // leave blank if no auth
const char* OTA_PASSWORD  = "OTA_PASSWORD";   // leave blank if no auth

// ================== GLOBALS ==================
QuarkifiStreamArduinoOTA ota;

// OTA Control boolean.
// Turn true to fire OTA
bool otaReady = false;

// Runtime
bool          otaFired   = false;


// ================== OTA STATUS CALLBACK ==================
// Called by the library whenever OTA state changes.
// Register this right before calling startOTA(), not in setup().
void onOtaStatus(const byte status, const byte progress) {
    switch (status) {
        case OtaStatus::STARTING:
            Serial.println("[OTA] Starting...");
            break;
        case OtaStatus::DOWNLOADING:
            Serial.print("[OTA] Downloading: ");
            Serial.print(progress);
            Serial.println("%");
            break;
        case OtaStatus::INSTALLING:
            Serial.println("[OTA] Installing...");
            break;
        case OtaStatus::SUCCESS:
            Serial.println("[OTA] Success!");
            break;
        case OtaStatus::RESTARTING:
            Serial.println("[OTA] Restarting...");
            break;
        case OtaStatus::ERR_BUSY:
            Serial.println("[OTA] Error: already in progress");
            break;
        case OtaStatus::ERR_NO_WIFI:
            Serial.println("[OTA] Error: Wi-Fi not connected");
            break;
        case OtaStatus::ERR_HTTP:
            Serial.println("[OTA] Error: HTTP request failed");
            break;
        case OtaStatus::ERR_NO_SPACE:
            Serial.println("[OTA] Error: not enough flash space");
            break;
        case OtaStatus::ERR_WRITE:
            Serial.println("[OTA] Error: flash write failed");
            break;
        case OtaStatus::ERR_TIMEOUT:
            Serial.println("[OTA] Error: download timed out");
            break;
        case OtaStatus::ERR_FINALIZE:
            Serial.println("[OTA] Error: failed to finalize update");
            break;
        case OtaStatus::ERR_RETRIES:
            Serial.println("[OTA] Error: all retries exhausted");
            break;
        default:
            Serial.print("[OTA] Unknown status: ");
            Serial.println(status);
            break;
    }
}

// ================== WIFI ==================
void connectWiFi() {
    Serial.print("Connecting WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.print("\nWiFi connected: ");
    Serial.println(WiFi.localIP());
}

// ================== SETUP ==================
void setup() {
    Serial.begin(115200);
    delay(1000);
    connectWiFi();

}

// ================== LOOP ==================

void loop() {

    // Fire OTA once after 60 seconds
    if (!otaFired && otaReady) {
        otaFired = true;

        Serial.println("\nTriggering OTA...");

        // Register callback and trigger — no need to call begin()
        ota.setCallback(onOtaStatus);
        // Below parameters can be managed using Quarkifi Stream Platform
        // MQTT is a preferred way of receiving these parameters 
        // However other means can also be used
        // OTA_URL -> firmware binary http path url. e.g https://xxxxxxx 
        // OTA_USER -> URL http basic auth username
        // OTA_PASSWORD -> URL http basic auth password

        String auth   = ota.buildAuth(OTA_USER, OTA_PASSWORD);
        bool   result = ota.startOTA(OTA_URL, auth);

        if (!result) {
            Serial.println("OTA failed to start.");
        }
    }
    delay(100);
}
