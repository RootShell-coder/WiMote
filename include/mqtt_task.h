#pragma once
#include <Arduino.h>
#include "ArduinoJson.h"
#include "mqtt_client.h"

struct SystemState {
    uint32_t heap_free = 0;
    uint32_t heap_size = 0;
    uint32_t cpu_freq = 0;
    uint32_t flash_size = 0;
    uint32_t uptime = 0;
    String sdk_version;
    String client_id;
};

struct WiFiState {
    String status;
    String ssid;
    String ip;
    unsigned long last_update = 0;
    int rssi = 0;
    int channel = 0;
};

void mqttPublishTask(void *parameter);
void mqttReceiveTask(void *parameter);
void publishSystemInfo();
void publishWiFiInfo();
