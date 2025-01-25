#include <Esp.h>
#include <WiFi.h>
#include "ntp_client.h"
#include "config.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_sntp.h"

struct tm timeinfo = {0};

void initNTP() {
    configTime(config.ntp.timezone, 0, config.ntp.server.c_str());
    sntp_set_sync_interval(config.ntp.update_interval / portTICK_PERIOD_MS);
    sntp_set_time_sync_notification_cb([](struct timeval *tv) {
                                           ESP_LOGI("SNTP", "Time synchronized");
                                           updateTimeInfo(); });
}

time_t getEpochTime() {
  return time(nullptr);
}

void updateTimeInfo() {
  if (!globalDoc.containsKey("time")) {
    globalDoc.createNestedObject("time");
  }

  JsonObject timeInfo = globalDoc["time"];
  bool synced = getLocalTime(&timeinfo);
  if (!synced) {
    ESP_LOGW("NTP", "Timeinfo lost synchronization");
  }

  timeInfo["synchronized"] = synced;
  timeInfo["epoch"] = getEpochTime();
  timeInfo["uptime"] = millis() / 1000;
}

void ntpTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000);
    while(1) {
        if(WiFi.status() == WL_CONNECTED) {
            updateTimeInfo();
        }
        vTaskDelay(xDelay);
    }
}
