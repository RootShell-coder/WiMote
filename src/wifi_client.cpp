#include "wifi_client.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ntp_client.h"
#include "web_server.h"
#include <DNSServer.h>
#include "esp_wifi.h"
#include <esp_wifi_types.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;
const char *AP_SSID_PREFIX = "WiMote_IR_";
const char *HOSTNAME = "WiMote";

void initWiFi() {
  JsonObject wifiObj = globalDoc["wifi"];

  // Используем mqtt.clientID как hostname если не задан свой
  const char* hostname = config.wifi.hostname.length() > 0 ?
                        config.wifi.hostname.c_str() :
                        config.mqtt.clientID.c_str();

  WiFi.hostname(hostname);

  if (!config.wifi.ssid.length()) {
    setupAPMode(wifiObj);
    return;
  }

  setupSTAMode();
}

void setupAPMode(JsonObject& wifiObj) {
  String apName = generateAPName();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str());

  IPAddress apIP = WiFi.softAPIP();
  setupDNSServer(apIP);

  updateWiFiStatus(wifiObj, "AP Mode", apName, apIP.toString());
  initWebServer();
}

void setupSTAMode() {
  WiFi.mode(WIFI_STA);

  // Установка режима работы WiFi
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11N);  // Только режим N

  // Установка ширины канала 20 МГц
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);

  // Включение помехозащищённости
  esp_wifi_set_ps(WIFI_PS_NONE);  // Отключаем энергосбережение

  // Настройка антенны (используем правильную функцию)
  wifi_ant_config_t ant_config = {
    .rx_ant_mode = WIFI_ANT_MODE_AUTO,
    .rx_ant_default = WIFI_ANT_ANT0,
    .tx_ant_mode = WIFI_ANT_MODE_AUTO,
    .enabled_ant0 = 1,
    .enabled_ant1 = 1
  };
  esp_wifi_set_ant(&ant_config);

  // Установка hostname
  const char* hostname = config.wifi.hostname.length() > 0 ?
                        config.wifi.hostname.c_str() :
                        config.mqtt.clientID.c_str();

  WiFi.setHostname(hostname);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));

  if (!WiFi.setHostname(hostname)) {
    Serial.println("Failed to set hostname");
  } else {
    Serial.printf("Hostname set to: %s\n", hostname);
  }

  // Применяем дополнительные настройки WiFi
  WiFi.setSleep(false);                 // Отключаем режим сна
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Максимальная мощность передачи

  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
}

WiFiInfo getWiFiInfo() {
  WiFiInfo info;
  info.ssid = WiFi.SSID();
  info.bssid = WiFi.BSSIDstr();
  info.rssi = WiFi.RSSI();
  info.channel = WiFi.channel();
  info.encryptionType = (wifi_auth_mode_t)WiFi.getMode();
  info.mac = WiFi.macAddress();
  info.localIP = WiFi.localIP();
  info.subnet = WiFi.subnetMask();
  info.gateway = WiFi.gatewayIP();
  info.txPower = WiFi.getTxPower();
  return info;
}

// Обновляем константы для управления мощностью
const int8_t RSSI_THRESHOLD_EXCELLENT = -50;  // Отличный сигнал
const int8_t RSSI_THRESHOLD_GOOD = -65;       // Хороший сигнал
const int8_t RSSI_THRESHOLD_FAIR = -75;       // Удовлетворительный сигнал
const uint8_t POWER_CHECK_INTERVAL = 10;      // Проверять каждые 10 циклов

// Добавляем константы мощности WiFi
const wifi_power_t WIFI_MIN_POWER = WIFI_POWER_MINUS_1dBm;    // Минимальная мощность
const wifi_power_t WIFI_LOW_POWER = WIFI_POWER_2dBm;          // Низкая мощность
const wifi_power_t WIFI_MED_POWER = WIFI_POWER_5dBm;          // Средняя мощность
const wifi_power_t WIFI_MAX_POWER = WIFI_POWER_19_5dBm;       // Максимальная мощность

void wifiTask(void *parameter) {
  int retries = 0;
  const int maxRetries = 10;
  uint8_t powerCheckCounter = 0;

  while (1) {
    if (WiFi.getMode() == WIFI_AP) {
      WiFiInfo info = getWiFiInfo();
      JsonObject wifi = globalDoc["wifi"];
      wifi["status"] = "AP Mode";
      wifi["connected_stations"] = WiFi.softAPgetStationNum();
      wifi["ip"] = WiFi.softAPIP().toString();
      wifi["last_update"] = millis();
    }
    else if (WiFi.status() != WL_CONNECTED) {
      globalDoc["wifi"]["status"] = "disconnected";
      globalDoc["wifi"]["retry_count"] = retries;
      globalDoc["wifi"]["last_update"] = millis();

      if (retries < maxRetries) {
        WiFi.disconnect();
        WiFi.reconnect();
        retries++;
      }
      else{
        if (WiFi.getMode() == WIFI_AP && WiFi.softAPgetStationNum() > 0) {
          retries = 0;
          continue;
        }
        globalDoc["wifi"]["status"] = "restarting";
        ESP.restart();
      }
    }
    else{
      if (retries > 0) {
        // После успешного подключения
        Serial.printf("Connected with hostname: %s\n", WiFi.getHostname());
        initWebServer();
        initNTP();
        updateTimeInfo();
        retries = 0;
      }

      // Адаптивное управление мощностью
      if (++powerCheckCounter >= POWER_CHECK_INTERVAL) {
        powerCheckCounter = 0;
        int8_t rssi = WiFi.RSSI();
        wifi_power_t currentPower = WiFi.getTxPower();
        wifi_power_t newPower = currentPower;

        if (rssi >= RSSI_THRESHOLD_EXCELLENT) {
          // Сигнал отличный - минимальная мощность
          newPower = WIFI_MIN_POWER;
        }
        else if (rssi >= RSSI_THRESHOLD_GOOD) {
          // Сигнал хороший - низкая мощность
          newPower = WIFI_LOW_POWER;
        }
        else if (rssi >= RSSI_THRESHOLD_FAIR) {
          // Сигнал нормальный - средняя мощность
          newPower = WIFI_MED_POWER;
        }
        else {
          // Сигнал слабый - максимальная мощность
          newPower = WIFI_MAX_POWER;
        }

        // Применяем новую мощность только если она изменилась
        if (newPower != currentPower) {
          WiFi.setTxPower(newPower);
          Serial.printf("RSSI: %d, Adjusting TX power to: %d\n", rssi, newPower);
        }
      }

      WiFiInfo info = getWiFiInfo();
      JsonObject wifi = globalDoc["wifi"];
      wifi["status"] = "connected";
      wifi["ssid"] = info.ssid;
      wifi["bssid"] = info.bssid;
      wifi["channel"] = info.channel;
      wifi["rssi"] = info.rssi;
      wifi["tx_power"] = info.txPower;
      wifi["mac"] = info.mac;
      wifi["ip"] = info.localIP.toString();
      wifi["subnet"] = info.subnet.toString();
      wifi["gateway"] = info.gateway.toString();
      wifi["auth_mode"] = (int)info.encryptionType;
      wifi["last_update"] = millis();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void handleDNS() {
  if (!config.wifi.ssid.length()) {
    dnsServer.processNextRequest();
  }
}

void dnsTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(10);
    while(1) {
        handleDNS();
        vTaskDelay(xDelay);
    }
}

String generateAPName() {
    return String(AP_SSID_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
}

void setupDNSServer(const IPAddress& apIP) {
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);

    xTaskCreate(
        dnsTask,
        "DNS",
        config.system.task_stack,
        NULL,
        1,
        NULL
    );
}

void updateWiFiStatus(JsonObject& wifiObj, const char* status, const String& ssid, const String& ip) {
    wifiObj["status"] = status;
    wifiObj["ssid"] = ssid;
    wifiObj["ip"] = ip;
}
