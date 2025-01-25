#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "config.h"

void initWiFi();
void wifiTask(void *parameter);
void dnsTask(void *parameter);
void handleDNS();

// Вспомогательные функции
String generateAPName();
void setupDNSServer(const IPAddress& apIP);
void setupAPMode(JsonObject& wifiObj);
void setupSTAMode();
void updateWiFiStatus(JsonObject& wifiObj, const char* status, const String& ssid, const String& ip);

struct WiFiInfo {
    String ssid;
    String bssid;
    int32_t rssi;
    int32_t channel;
    wifi_auth_mode_t encryptionType;
    String mac;
    IPAddress localIP;
    IPAddress subnet;
    IPAddress gateway;
    int8_t txPower;
};

WiFiInfo getWiFiInfo();
