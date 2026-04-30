



# Quarkifi Stream Arduino OTA Usage Details

## Include Library

```arduino 
#include <QuarkifiStreamArduinoOTA.h>
```   
## Setup
Make sure Arduino IDE is setup with ESP8266/ESP32 development configurations
### Start OTA
Retrieve credentials and OTA details from OTA platform 
( e.g Quarkifi Stream (https://qstream.quarkifi.com))
```arduino 
ota.setCallback(onOtaStatus);
String auth = ota.buildAuth(user, pass);
ota.startOTA(url, auth);
``` 
The callbackack function provides OTA updates


### OTA CAlback Function

This Library Publishes OTA status back on the callback:
It can be optionally sent back to the OTA platform 
 { "ota_status": <code>, "ota_progress": <0-100> }

OtaStatus code reference:
   STARTING    (0)  — OTA attempt is beginning
   DOWNLOADING (1)  — Firmware downloading; ota_progress = 0–100
   INSTALLING  (2)  — Writing firmware to flash
   SUCCESS     (3)  — Firmware written and verified
   RESTARTING  (4)  — Device is about to restart
   ERR_BUSY    (10) — OTA already in progress; request ignored
   ERR_NO_WIFI (11) — Wi-Fi not connected
   ERR_HTTP    (12) — HTTP request failed (non-200 response)
   ERR_NO_SPACE(13) — Not enough flash space
   ERR_WRITE   (14) — Flash write error
   ERR_TIMEOUT (15) — No data received within timeout window
   ERR_FINALIZE(16) — Update.end() failed
  ERR_RETRIES (17) — All retry attempts exhausted
```arduino 
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

    // DO NOT CHANGE Below structure if using Quarkifi Stream Platfrom (https://qstream.quarkifi.com)
    JsonDocument doc;
    doc["ota_status__"]   = status;
    doc["ota_progress__"] = progress;      // 0–100; only meaningful when status == 1 (DOWNLOADING)

    char buffer[64];
    serializeJson(doc, buffer);
    mqttClient->publish(TOPIC_PUB, buffer);
} 
```  

For any issue please reach out to support@quarkifi.com or raise an issue
