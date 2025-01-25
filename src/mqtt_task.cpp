#include "mqtt_task.h"
#include "mqtt_client.h"
#include "config.h"
#include <WiFi.h>
#include "ArduinoJson.h"

// Удаляем определения структур, используем те, что определены в mqtt_client.h
static SystemState lastSystem;
static WiFiState lastWiFi;

// Уберем значение по умолчанию, оно уже определено в заголовочном файле
void publishValue(const char *topic, const char *value, bool retain) {
  mqttManager.publish(topic, value, retain);
}

void publishSystemInfo() {
  JsonObject system = globalDoc["system"];
  JsonObject time = globalDoc["time"];
  JsonObject mqtt = globalDoc["mqtt"];

  uint32_t heap_free = system["heap_free"];
  if (heap_free != lastSystem.heap_free) {
    publishValue("system/heap_free", String(heap_free).c_str(), true);
    lastSystem.heap_free = heap_free;
  }

  uint32_t heap_size = system["heap_size"];
  if (heap_size != lastSystem.heap_size) {
    publishValue("system/heap_size", String(heap_size).c_str(), true);
    lastSystem.heap_size = heap_size;
  }

  uint32_t cpu_freq = system["cpu_freq_mhz"];
  if (cpu_freq != lastSystem.cpu_freq) {
    publishValue("system/cpu_freq_mhz", String(cpu_freq).c_str(), true);
    lastSystem.cpu_freq = cpu_freq;
  }

  String sdk_version = system["sdk_version"];
  if (sdk_version != lastSystem.sdk_version) {
    publishValue("system/sdk_version", sdk_version.c_str(), true);
    lastSystem.sdk_version = sdk_version;
  }

  uint32_t flash_size = system["flash_size"];
  if (flash_size != lastSystem.flash_size) {
    publishValue("system/flash_size", String(flash_size).c_str(), true);
    lastSystem.flash_size = flash_size;
  }

  String client_id = mqtt["client_id"];
  if (client_id != lastSystem.client_id) {
    publishValue("system/client_id", client_id.c_str(), true);
    lastSystem.client_id = client_id;
  }

  uint32_t uptime = time["uptime"];
  if (uptime != lastSystem.uptime) {
    publishValue("system/uptime", String(uptime).c_str(), true);
    lastSystem.uptime = uptime;
  }
}

void publishWiFiInfo() {
  JsonObject wifi = globalDoc["wifi"];

  String status = wifi["status"];
  if (status != lastWiFi.status) {
    publishValue("wifi/status", status.c_str(), true);
    lastWiFi.status = status;
  }

  if (status == "connected") {
    String ssid = wifi["ssid"];
    if (ssid != lastWiFi.ssid) {
      publishValue("wifi/ssid", ssid.c_str(), true);
      lastWiFi.ssid = ssid;
    }

    unsigned long update = wifi["last_update"];
    if (update != lastWiFi.last_update) {
      publishValue("wifi/last_update", String(update / 1000).c_str(), true);
      lastWiFi.last_update = update;
    }

    String ip = wifi["ip"];
    if (ip != lastWiFi.ip) {
      publishValue("wifi/ip", ip.c_str(), true);
      lastWiFi.ip = ip;
    }

    int rssi = wifi["rssi"];
    if (rssi != lastWiFi.rssi) {
      publishValue("wifi/rssi", String(rssi).c_str(), true);
      lastWiFi.rssi = rssi;
    }

    int channel = wifi["channel"];
    if (channel != lastWiFi.channel) {
      publishValue("wifi/channel", String(channel).c_str(), true);
      lastWiFi.channel = channel;
    }
  }
}

void mqttPublishTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(10000);
    while(1) {
        if(WiFi.status() == WL_CONNECTED) {
            publishSystemInfo();
            publishWiFiInfo();
        }
        vTaskDelay(xDelay);
    }
}

void mqttReceiveTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(100);
    while(1) {
        if(WiFi.status() == WL_CONNECTED) {
            mqttManager.handle();  // Используем метод handle нового класса вместо handleMQTT
        }
        vTaskDelay(xDelay);
    }
}
