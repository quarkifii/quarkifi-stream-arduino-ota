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
// ESP32_MQTT_OTA Example
// ============================================================
// Demonstrates QstreamArduinoOta integrated with MQTT on ESP32.
//
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

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <QstreamArduinoOta.h>

// ================== CONFIG ==================
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// Primary broker: mqtt.qconsole.quarkifi.com
// Also compatible with: stream.quarkifi.com
// Port 1883 — Unsecured | Port 8883 — TLS-Secured
const char* MQTT_HOST = "mqtt.qconsole.quarkifi.com";
const int   MQTT_PORT = 1883;
const char* MQTT_USER = "your-mqtt-user";
const char* MQTT_PASS = "your-mqtt-pass";

const char* TOPIC_PUB = "your-mqtt-user/your-device-id/upstream";
const char* TOPIC_SUB = "your-mqtt-user/your-device-id/downstream";

const int PUBLISH_INTERVAL = 10000;
const int LED_PIN          = 2;

// ================== GLOBALS ==================
WiFiClient        espClient;
PubSubClient      mqttClient(espClient);
QstreamArduinoOta ota;

unsigned long lastPublish = 0;

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
        Serial.println("Restarting... ");
    } else if (status >= 10) {
        Serial.println("Error on OTA: " + String(status));
    }

    JsonDocument doc;
    doc["ota_status"]   = status;
    doc["ota_progress"] = progress;      // 0–100; only meaningful when status == 1 (DOWNLOADING)

    char buffer[64];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_PUB, buffer);
}

// ================== START OTA ==================
// Registers the callback and triggers OTA in one call.
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
    while (!mqttClient.connected()) {
        Serial.print("Connecting MQTT...");

        String clientId = "ESP32-" + String(random(0xffff), HEX);

        if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println("connected");
            mqttClient.subscribe(TOPIC_SUB);
        } else {
            Serial.print("failed rc=");
            Serial.println(mqttClient.state());
            delay(3000);
        }
    }
}

// ================== PUBLISH ==================
void publishData() {
    JsonDocument doc;                    // ArduinoJson v7 — no size template needed
    doc["temperature"] = random(20, 40);
    doc["humidity"]    = random(30, 80);
    doc["light"]       = random(0, 1000);
    doc["pressure"]    = random(900, 1100);

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPIC_PUB, buffer);
    Serial.println("Published: " + String(buffer));
}

// ================== MQTT CALLBACK ==================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.println("Received: " + message);
    JsonDocument doc;
    if (deserializeJson(doc, message) != DeserializationError::Ok) return;

    // ---- OTA trigger ----
    if (doc["ota_url__"].is<const char*>()) {
        String url  = doc["ota_url__"].as<String>();
        String user = doc["auth_username__"] | "";
        String pass = doc["auth_password__"] | "";

        startOta(url, user, pass);
        return;
    }

    // ---- Normal control message ----
    int sw = doc["switch"] | 0;
    digitalWrite(LED_PIN, sw == 1 ? HIGH : LOW);
}

// ================== SETUP ==================
void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    connectWiFi();

    // Initialize MQTT
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);

    randomSeed(esp_random());
}

// ================== LOOP ==================
void loop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }

    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastPublish > PUBLISH_INTERVAL) {
        lastPublish = now;
        publishData();
    }
}
