// ============================================================
// ESP8266_MQTT_OTA Example
// ============================================================
// Demonstrates QstreamArduinoOta integrated with MQTT on ESP8266.
//
// Receives a downstream MQTT message:
//   {
//     "ota_url__":        "https://example.com/firmware.bin",
//     "auth_username__":  "user",          // optional
//     "auth_password__":  "pass"           // optional
//   }
//
// Publishes OTA status back upstream:
//   { "ota_status": "downloading: 45%" }
// ============================================================

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <QstreamArduinoOta.h>

// ================== CONFIG ==================
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

const char* MQTT_HOST = "mqtt.example.com";
const int   MQTT_PORT = 1883;
const char* MQTT_USER = "your-mqtt-user";
const char* MQTT_PASS = "your-mqtt-pass";

const char* TOPIC_PUB = "your-mqtt-user/your-device-id/upstream";
const char* TOPIC_SUB = "your-mqtt-user/your-device-id/downstream";

const int PUBLISH_INTERVAL = 10000;
const int LED_PIN          = LED_BUILTIN;

// ================== GLOBALS ==================
WiFiClient   espClient;
PubSubClient mqttClient(espClient);
QstreamArduinoOta ota;

unsigned long lastPublish = 0;

// ================== OTA STATUS CALLBACK ==================
void onOtaStatus(const String &status) {
    Serial.println("[OTA] " + status);
    
    StaticJsonDocument<128> doc;
    doc["ota_status"] = status;

    char buffer[128];
    serializeJson(doc, buffer);

    mqttClient.publish(TOPIC_PUB, buffer);
}

// ================== WIFI ==================
void connectWiFi() {
    Serial.print("Connecting WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

// ================== MQTT ==================
void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting MQTT...");

        String clientId = "ESP8266-" + String(ESP.getChipId());

        if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
            Serial.println("connected");
            mqttClient.subscribe(TOPIC_SUB);
        } else {
            Serial.print("failed rc=");
            Serial.println(mqttClient.state());
            delay(2000);
        }
    }
}

// ================== PUBLISH ==================
void publishData() {
    StaticJsonDocument<256> doc;
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
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    Serial.println("Received: " + message);

    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, message) != DeserializationError::Ok) return;

    // ---- OTA trigger ----
    if (doc["ota_url__"]) {
        String url  = doc["ota_url__"].as<String>();
        String user = doc["auth_username__"] | "";
        String pass = doc["auth_password__"] | "";

        String auth = QstreamArduinoOta::buildAuth(user, pass);
        ota.startOTA(url, auth);
        return;
    }

    // ---- Normal control message ----
    int sw = doc["switch"] | 0;
    digitalWrite(LED_PIN, sw ? LOW : HIGH); // ESP8266 LED is active-LOW
}

// ================== SETUP ==================
void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // LED off (active-LOW)

    connectWiFi();

    // Initialize OTA
    ota.begin();
    ota.setCallback(onOtaStatus);

    // Initialize MQTT
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);

    randomSeed(ESP.getChipId());
}

// ================== LOOP ==================
void loop() {
    connectMQTT();
    mqttClient.loop();

    unsigned long now = millis();
    if (now - lastPublish > PUBLISH_INTERVAL) {
        lastPublish = now;
        publishData();
    }
}
