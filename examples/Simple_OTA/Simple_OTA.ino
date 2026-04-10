// ============================================================
// Simple_OTA Example | Support both ESP32 and ESP8266
// ============================================================

#include <Arduino.h>

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <QstreamArduinoOta.h>


// ================== CONFIG ==================
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

const char* OTA_URL = "OTA_URL";
const char* OTA_USER = "OTA_USERNAME";
const char* OTA_PASSWORD = "OTA_PASSWORD";

// ================== GLOBALS ==================
QstreamArduinoOta ota;

unsigned long startTime;
bool started = false;
byte countdown = 60;

// ================== OTA STATUS CALLBACK ==================
// Called by the library whenever OTA state changes.
void otaStatusCallback(const String &status) {
    Serial.println("[OTA] " + status);
}

// ================== WIFI ==================
void connectWiFi() {
    Serial.print("Connecting WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

// ================== SETUP ==================
void setup() {
    Serial.begin(115200);
    delay(1000);

    connectWiFi();

    // Initialize OTA
    ota.begin();
    ota.setCallback(otaStatusCallback);

    startTime = millis();
}

// ================== LOOP ==================
void loop() {

    // Start after 1 minute
    if (!started && millis() - startTime >60000) {
        started = true;

        // ---- OTA trigger ----
        Serial.println("\nTriggering OTA...");

        String auth = QstreamArduinoOta::buildAuth(OTA_USER, OTA_PASSWORD);
        bool result = ota.startOTA(OTA_URL, auth);

        if (!result) {
        Serial.println("OTA start failed immediately");
        }
    }
    // OTA Countdown
    Serial.println(countdown);
    countdown = countdown -1;
    delay(1000);
}