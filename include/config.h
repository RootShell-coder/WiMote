#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct JsonConfigLog {
  bool enable_serial_logs = false;
};

struct Config {
  struct {
    String ssid;
    String password;
    String hostname;  // Добавляем hostname в основную структуру конфигурации
  } wifi;

  struct {
    String server;
    int timezone;
    int update_interval;
  } ntp;

  struct {
    int json_size = 2048;
    int logger_stack = 8192;
    int wifi_stack = 8192;
    int task_stack = 8192;
    int ir_stack = 4096;
  } system;

  struct {
    String clientID;
    String host;
    int port;
    String user;
    String password;
    String base_topic;
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
