#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

struct WiFiInfo
{
  String ssid;
  String bssid;
  int32_t rssi;
  int32_t channel;
  wifi_auth_mode_t encryptionType;
  String mac;
  IPAddress localIP;
  IPAddress subnet;
  IPAddress gateway;
  int txPower;
};

void initWiFi();
void wifiTask(void *parameter);
WiFiInfo getWiFiInfo();
void handleDNS();
