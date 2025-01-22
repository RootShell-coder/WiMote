#include <Arduino.h>
#include "tasks.h"
#include "wifi_client.h"
#include "ntp_client.h"
#include "config.h"
#include "web_server.h"
#include "mqtt_client.h"

StaticJsonDocument<2048> globalDoc;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  if (!loadConfig())
  {
    Serial.println("Configuration failed to load. Halting.");
    while (1)
      ;
  }

  globalDoc.clear();

  JsonObject systemObj = globalDoc.createNestedObject("system");
  JsonObject wifiObj = globalDoc.createNestedObject("wifi");
  JsonObject timeObj = globalDoc.createNestedObject("time");
  JsonObject mqttObj = globalDoc.createNestedObject("mqtt"); // Добавляем объект mqtt

  systemObj["heap_free"] = ESP.getFreeHeap();
  systemObj["heap_size"] = ESP.getHeapSize();
  systemObj["cpu_freq_mhz"] = getCpuFrequencyMhz();
  systemObj["sdk_version"] = ESP.getSdkVersion();
  systemObj["flash_size"] = ESP.getFlashChipSize() / 1024;

  wifiObj["status"] = "initializing";
  wifiObj["last_update"] = 0;

  timeObj["synchronized"] = false;
  timeObj["epoch"] = 0;
  timeObj["uptime"] = 0;

  mqttObj["state"] = -3;
  mqttObj["status"] = "not connected";

  initWiFi();
  initMQTT();
  initTasks();
}

void loop()
{
  handleDNS();
}
