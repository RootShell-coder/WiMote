#include "mqtt_task.h"
#include "mqtt_client.h"
#include "config.h"
#include <WiFi.h>

struct SystemState
{
  uint32_t heap_free = 0;
  uint32_t heap_size = 0;
  uint32_t cpu_freq = 0;
  uint32_t flash_size = 0;
  uint32_t uptime = 0;
  String sdk_version;
  String client_id;
};

struct WiFiState
{
  String status;
  String ssid;
  String ip;
  unsigned long last_update = 0;
  int rssi = 0;
  int channel = 0;
};

static SystemState lastSystem;
static WiFiState lastWiFi;

void publishValue(const char *topic, const char *value, bool retain = true)
{
  if (getMQTTState() == 0)
  {
    publishMessage(topic, value, retain);
  }
}

void publishSystemInfo()
{
  JsonObject system = globalDoc["system"];
  JsonObject time = globalDoc["time"];
  JsonObject mqtt = globalDoc["mqtt"];

  uint32_t heap_free = system["heap_free"];
  if (heap_free != lastSystem.heap_free)
  {
    publishValue("system/heap_free", String(heap_free).c_str());
    lastSystem.heap_free = heap_free;
  }

  uint32_t heap_size = system["heap_size"];
  if (heap_size != lastSystem.heap_size)
  {
    publishValue("system/heap_size", String(heap_size).c_str());
    lastSystem.heap_size = heap_size;
  }

  uint32_t cpu_freq = system["cpu_freq_mhz"];
  if (cpu_freq != lastSystem.cpu_freq)
  {
    publishValue("system/cpu_freq_mhz", String(cpu_freq).c_str());
    lastSystem.cpu_freq = cpu_freq;
  }

  String sdk_version = system["sdk_version"];
  if (sdk_version != lastSystem.sdk_version)
  {
    publishValue("system/sdk_version", sdk_version.c_str());
    lastSystem.sdk_version = sdk_version;
  }

  uint32_t flash_size = system["flash_size"];
  if (flash_size != lastSystem.flash_size)
  {
    publishValue("system/flash_size", String(flash_size).c_str());
    lastSystem.flash_size = flash_size;
  }

  String client_id = mqtt["client_id"];
  if (client_id != lastSystem.client_id)
  {
    publishValue("system/client_id", client_id.c_str());
    lastSystem.client_id = client_id;
  }

  uint32_t uptime = time["uptime"];
  if (uptime != lastSystem.uptime)
  {
    publishValue("system/uptime", String(uptime).c_str());
    lastSystem.uptime = uptime;
  }
}

void publishWiFiInfo()
{
  JsonObject wifi = globalDoc["wifi"];

  String status = wifi["status"];
  if (status != lastWiFi.status)
  {
    publishValue("wifi/status", status.c_str());
    lastWiFi.status = status;
  }

  if (status == "connected")
  {
    String ssid = wifi["ssid"];
    if (ssid != lastWiFi.ssid)
    {
      publishValue("wifi/ssid", ssid.c_str());
      lastWiFi.ssid = ssid;
    }

    unsigned long update = wifi["last_update"];
    if (update != lastWiFi.last_update)
    {
      publishValue("wifi/last_update", String(update / 1000).c_str());
      lastWiFi.last_update = update;
    }

    String ip = wifi["ip"];
    if (ip != lastWiFi.ip)
    {
      publishValue("wifi/ip", ip.c_str());
      lastWiFi.ip = ip;
    }

    int rssi = wifi["rssi"];
    if (rssi != lastWiFi.rssi)
    {
      publishValue("wifi/rssi", String(rssi).c_str());
      lastWiFi.rssi = rssi;
    }

    int channel = wifi["channel"];
    if (channel != lastWiFi.channel)
    {
      publishValue("wifi/channel", String(channel).c_str());
      lastWiFi.channel = channel;
    }
  }
}

void mqttTask(void *parameter)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000);

  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    if (WiFi.status() == WL_CONNECTED)
    {
      handleMQTT();
      if (getMQTTState() == 0)
      {
        publishSystemInfo();
        publishWiFiInfo();
      }
    }
  }
}
