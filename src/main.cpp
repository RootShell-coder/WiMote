#include <Arduino.h>
#include "config.h"
#include "wifiManager.h"

void setup(){
    Serial.begin(115200);
    delay(1000);

    if(!Config::init()){
        Serial.println("Config initialization failed");
        return;
    }

    const Config& cfg = Config::instance();
    Serial.printf("Device name: %s\n", cfg.getName());
    Serial.printf("WiFi SSID: %s\n", cfg.getWifi().ssid.c_str());  // Добавлен .c_str()

    if(!WiFiManager::instance().begin()) {
        Serial.println("Failed to connect, starting config portal");
        WiFiManager::instance().startPortal();
    } else {
        Serial.println("Connected to WiFi");
        Serial.println(WiFi.localIP());
    }
}

void loop(){
    WiFiManager::instance().process();
}
