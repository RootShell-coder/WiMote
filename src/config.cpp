#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "config.h"

Config config;

bool loadConfig()
{

  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed");
    return false;
  }

  if (!SPIFFS.exists("/config.json"))
  {
    Serial.println("Config file not found");
    return false;
  }

  File file = SPIFFS.open("/config.json", "r");
  if (!file)
  {
    Serial.println("Failed to open config file");
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.printf("Config parse failed: %s\n", error.c_str());
    return false;
  }

  JsonObject wifi = doc["wifi"];
  if (wifi)
  {
    config.wifi.ssid = wifi["ssid"] | config.wifi.ssid;
    config.wifi.password = wifi["password"] | config.wifi.password;
  }

  JsonObject ntp = doc["ntp"];
  if (ntp)
  {
    const char *server = ntp["server"];
    config.ntp.server = server;
    config.ntp.timezone = ntp["timezone"] | config.ntp.timezone;
    config.ntp.update_interval = ntp["update_interval"] | config.ntp.update_interval;
  }

  JsonObject sys = doc["system"];
  if (sys)
  {
    config.system.json_size = sys["json_size"] | config.system.json_size;
    config.system.logger_stack = sys["logger_stack"] | config.system.logger_stack;
    config.system.wifi_stack = sys["wifi_stack"] | config.system.wifi_stack;
    config.system.task_stack = sys["task_stack"] | config.system.task_stack;
  }

  JsonObject mqtt = doc["mqtt"];
  if (mqtt)
  {
    config.mqtt.clientID = mqtt["clientID"] | config.mqtt.clientID;
    config.mqtt.host = mqtt["host"] | config.mqtt.host;
    config.mqtt.port = mqtt["port"] | config.mqtt.port;
    config.mqtt.user = mqtt["user"] | config.mqtt.user;
    config.mqtt.password = mqtt["password"] | config.mqtt.password;
    config.mqtt.base_topic = mqtt["base_topic"] | config.mqtt.base_topic;

    if (config.mqtt.host.isEmpty())
    {
      Serial.println("Error: MQTT host is empty. Check config.json.");
      return false;
    }
    else
    {
      Serial.printf("MQTT host from config: %s\n", config.mqtt.host.c_str());
    }
  }

  Serial.println("Configuration loaded successfully");
  return true;
}

bool resetWiFiConfig()
{
  StaticJsonDocument<1024> doc;

  File file = SPIFFS.open("/config.json", "r");
  if (file)
  {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
      return false;
  }

  doc.remove("wifi");

  file = SPIFFS.open("/config.json", "w");
  if (!file)
    return false;

  serializeJson(doc, file);
  file.close();
  loadConfig();
  return true;
}

bool resetMQTTConfig()
{
  StaticJsonDocument<1024> doc;

  File file = SPIFFS.open("/config.json", "r");
  if (file)
  {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
      return false;
  }

  doc.remove("mqtt");

  file = SPIFFS.open("/config.json", "w");
  if (!file)
    return false;

  serializeJson(doc, file);
  file.close();
  loadConfig();
  return true;
}

bool resetNTPConfig()
{
  StaticJsonDocument<1024> doc;

  File file = SPIFFS.open("/config.json", "r");
  if (file)
  {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
      return false;
  }

  doc.remove("ntp");

  file = SPIFFS.open("/config.json", "w");
  if (!file)
    return false;

  serializeJson(doc, file);
  file.close();
  loadConfig();
  return true;
}

bool resetAllConfig()
{
  File file = SPIFFS.open("/config.json", "w");
  if (!file)
    return false;

  StaticJsonDocument<1024> doc;
  serializeJson(doc, file);
  file.close();
  loadConfig();
  return true;
}
