#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct JsonConfigLog
{
  bool enable_serial_logs = false;
};

struct Config
{
  struct
  {
    String ssid;
    String password;
  } wifi;

  struct
  {
    String server;
    int timezone;
    int update_interval;
  } ntp;

  struct
  {
    int json_size = 2048;
    int logger_stack = 4096;
    int wifi_stack = 4096;
    int task_stack = 2048;
  } system;

  struct
  {
    String clientID;
    String host;
    int port;
    String user;
    String password;
    String base_topic; // Изменено с topic на base_topic
  } mqtt;
};

extern StaticJsonDocument<2048> globalDoc;
extern Config config;
extern JsonConfigLog logConfig;
bool loadConfig();
bool resetWiFiConfig();
bool resetMQTTConfig();
bool resetNTPConfig();
bool resetAllConfig();
