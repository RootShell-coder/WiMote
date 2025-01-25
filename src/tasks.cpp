#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "tasks.h"
#include "config.h"
#include "ntp_client.h"
#include "web_server.h"
#include "wifi_client.h"
#include "mqtt_client.h"
#include "mqtt_task.h"
#include "ir.h"
#include "timer_manager.h"

// Объявление функции onTimer перед использованием
void IRAM_ATTR onTimer();

void timeUpdateTask(void *parameter) {
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  while (1) {
    updateTimeInfo();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void publishSystemChanges(JsonObject system) {
  static uint32_t last_heap_free = 0;
  static uint32_t last_heap_size = 0;
  static uint32_t last_cpu_freq = 0;
  static String last_sdk_version;
  static uint32_t last_flash_size = 0;
  uint32_t heap_free = ESP.getFreeHeap();
  if (heap_free != last_heap_free) {
    mqttManager.publish("system/heap_free", String(heap_free).c_str(), true);
    last_heap_free = heap_free;
  }

  uint32_t heap_size = ESP.getHeapSize();
  if (heap_size != last_heap_size) {
    mqttManager.publish("system/heap_size", String(heap_size).c_str(), true);
    last_heap_size = heap_size;
  }

  uint32_t cpu_freq = getCpuFrequencyMhz();
  if (cpu_freq != last_cpu_freq) {
    mqttManager.publish("system/cpu_freq_mhz", String(cpu_freq).c_str(), true);
    last_cpu_freq = cpu_freq;
  }

  String sdk_version = ESP.getSdkVersion();
  if (sdk_version != last_sdk_version) {
    mqttManager.publish("system/sdk_version", sdk_version.c_str(), true);
    last_sdk_version = sdk_version;
  }

  uint32_t flash_size = ESP.getFlashChipSize() / 1024;
  if (flash_size != last_flash_size) {
    mqttManager.publish("system/flash_size", String(flash_size).c_str(), true);
    last_flash_size = flash_size;
  }
}

void publishWiFiInfo(JsonObject wifi) {
  static String last_status;
  static String last_ssid;
  static String last_ip;
  static int last_rssi = 0;
  static int last_channel = 0;

  String status = wifi["status"].as<String>();
  if (status != last_status) {
    mqttManager.publish("wifi/status", status.c_str(), true);
    last_status = status;
  }

  if (status == "connected") {
    String ssid = wifi["ssid"].as<String>();
    if (ssid != last_ssid) {
      mqttManager.publish("wifi/ssid", ssid.c_str(), true);
      last_ssid = ssid;
    }

    String ip = wifi["ip"].as<String>();
    if (ip != last_ip) {
      mqttManager.publish("wifi/ip", ip.c_str(), true);
      last_ip = ip;
    }

    int rssi = wifi["rssi"].as<int>();
    if (rssi != last_rssi) {
      mqttManager.publish("wifi/rssi", String(rssi).c_str(), true);
      last_rssi = rssi;
    }

    int channel = wifi["channel"].as<int>();
    if (channel != last_channel) {
      mqttManager.publish("wifi/channel", String(channel).c_str(), true);
      last_channel = channel;
    }
  }
}

void loggerTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000);

  while (1) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    JsonObject system = globalDoc["system"];
    system["heap_free"] = ESP.getFreeHeap();
    system["heap_size"] = ESP.getHeapSize();
    system["cpu_freq_mhz"] = getCpuFrequencyMhz();
    system["sdk_version"] = ESP.getSdkVersion();
    system["flash_size"] = ESP.getFlashChipSize() / 1024;
    JsonObject mqtt = globalDoc["mqtt"];
    if (WiFi.status() == WL_CONNECTED) {
      int mqttState = mqttManager.getState(); // Заменяем getMQTTState() на mqttManager.getState()
      mqtt["state"] = mqttState;
      mqtt["status"] = mqttState == 0 ? "connected" : mqttState == -1 ? "connection timeout"
                                                : mqttState == -2   ? "connection failed"
                                                : mqttState == -3   ? "not connected"
                                                : mqttState == -4   ? "connection lost"
                                                                    : "unknown";
      mqtt["broker"] = config.mqtt.host;
      mqtt["port"] = config.mqtt.port;

      String fullClientId = (config.mqtt.clientID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX));
      fullClientId.toLowerCase();
      mqtt["client_id"] = fullClientId;
      mqtt["base_topic"] = config.mqtt.base_topic + config.mqtt.clientID;
    }
    else{
      mqtt["state"] = -5;
      mqtt["status"] = "wifi disconnected";
    }

    if (logConfig.enable_serial_logs) {
      serializeJsonPretty(globalDoc, Serial);
      Serial.println();
    }
  }
}

void initTasks() {
    xTaskCreate(loggerTask, "Logger", config.system.logger_stack, NULL, 1, NULL);
    xTaskCreate(timeUpdateTask, "TimeUpdate", config.system.task_stack, NULL, 2, NULL);
    xTaskCreate(wifiTask, "WiFi", config.system.wifi_stack, NULL, 2, NULL);
    xTaskCreate(
        ntpTask,
        "NTP",
        config.system.task_stack,
        NULL,
        1,
        NULL
    );
}
