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
// ESP8266_MQTT_OTA Example
// ============================================================
// Demonstrates Quarkifi Stream Arduino OTA integrated with MQTT on ESP8266.
// This demo program can be used to integrate with Quarkifi Stream Platform 
// (https://qstream.quarkifi.com) or any other OTA supported platform as well
//
// If you are not using Quarkifi Stream Platform make sure you adjust below parameters
// Receives a downstream MQTT message:
//   {
//     "ota_url__":       "https://example.com/firmware.bin",
//     "auth_username__": "user",   // optional
//     "auth_password__": "pass"    // optional
//   }
//
// Publishes OTA status back upstream:
//   { "ota_status": <code>, "ota_progress": <0-100> }
//
// OtaStatus code reference:
//   STARTING    (0)  — OTA attempt is beginning
//   DOWNLOADING (1)  — Firmware downloading; ota_progress = 0–100
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

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <QuarkifiStreamOTA.h>

// ================== CONFIG ==================
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// MQTT broker: mqtt.qconsole.quarkifi.com
// Replace with any other broker if you are not using Quarkifi Stream platform
// Above broker makes seamless OTA . Learn more at -> https://qstream.quarkifi.com
// Port 1883 — Unsecured | Port 8883 — TLS-Secured
// Once device is created the below credentials can be retrieved from Quarkifi Stream Platform

const char* MQTT_HOST = "mqtt.qconsole.quarkifi.com";
const int   MQTT_PORT = 1883;
const char* MQTT_USER = "your-mqtt-user";
const char* MQTT_PASS = "your-mqtt-pass";

const char* TOPIC_PUB = "your-mqtt-user/your-device-id/upstream";
const char* TOPIC_SUB = "your-mqtt-user/your-device-id/downstream";

// ================== GLOBALS ==================
// Use below if using secured(TLS) MQTT - 8883 
WiFiClientSecure        espClientSecure;
// Use below if using unsecured MQTT - 1883
WiFiClient        espClient;
PubSubClient      *mqttClient;
QuarkifiStreamArduinoOTA ota;

// ================== OTA STATUS CALLBACK ==================
// Registered right before startOTA() — not in setup().
// Publishes numeric status and progress back to MQTT.
void onOtaStatus(const byte status, const byte progress) {
    if (status == 0) {
        Serial.println("Starting OTA...");
    } else if (status == 1) {
        Serial.println("Downloading: " + String(progress) + "%");
    } else if (status == 3) {
        Serial.println("Download successfully ");
    } else if (status == 4) {
        Serial.println("Restarting...");
    } else if (status >= 10) {
        Serial.println("Error on OTA: " + String(status));
    }
    // DO NOT CHANGE Below structure if using Quarkifi Stream Platfrom 
    // (https://qstream.quarkifi.com)
    JsonDocument doc;
    doc["ota_status__"]   = status;
    doc["ota_progress__"] = progress;      // 0–100; only meaningful when status == 1 (DOWNLOADING)

    char buffer[64];
    serializeJson(doc, buffer);
    mqttClient->publish(TOPIC_PUB, buffer);
}

// ================== START OTA ==================
// Called by the library whenever OTA state changes.
// Call this instead of ota.startOTA() directly.
void startOta(const String& url, const String& user, const String& pass) {
    ota.setCallback(onOtaStatus);
    String auth = ota.buildAuth(user, pass);
    ota.startOTA(url, auth);
}

// ================== WIFI ==================
void connectWiFi() {
    Serial.print("Connecting WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.print("\nWiFi connected: ");
    Serial.println(WiFi.localIP());
}

// ================== MQTT ==================
void connectMQTT() {
    while (!mqttClient->connected()) {
        Serial.print("Connecting MQTT...");

        String clientId = "ESP8266-" + String(ESP.getChipId());

        if (mqttClient->connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println("connected");
            mqttClient->subscribe(TOPIC_SUB);
        } else {
            Serial.print("failed rc=");
            Serial.println(mqttClient->state());
            delay(2000);
        }
    }
}

// ================== MQTT CALLBACK ==================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("Received on Topic: " + String(topic));
    Serial.println("Received: " + message);

    JsonDocument doc;
    if (deserializeJson(doc, message) != DeserializationError::Ok) return;

    // ---- OTA trigger ----
    // Below arguments are fixed with Quarkifi Stream Platform 
    // DO NOT Change below params if using Quarkifi Stream (https://qstream.quarkifi.com)
    if (doc["ota_url__"].is<const char*>()) {
        String url  = doc["ota_url__"].as<String>();
        String user = doc["auth_username__"] | "";
        String pass = doc["auth_password__"] | "";

        startOta(url, user, pass);
        return;
    }

}

// ================== SETUP ==================
void setup() {
    Serial.begin(115200);

    connectWiFi();
    if(MQTT_PORT == 1883) {
        mqttClient =  new PubSubClient(espClient);
    }else if (MQTT_PORT == 8883) {
        // If needed to avoid below setInsecure, replace this with latest CA root chain of mqtt host
        // setInsecure uses secured TLS transport but can allow man in the middle attack
        espClientSecure.setInsecure();
        mqttClient =  new PubSubClient(espClientSecure);
    }

    mqttClient->setServer(MQTT_HOST, MQTT_PORT);
    mqttClient->setCallback(mqttCallback);
    mqttClient->setBufferSize(1024);

    randomSeed(ESP.getChipId());
}

// ================== LOOP ==================
void loop() {
    connectMQTT();
    mqttClient->loop();
}
