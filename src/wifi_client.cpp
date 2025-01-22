#include "wifi_client.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ntp_client.h"
#include "web_server.h"
#include <DNSServer.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;
const char *AP_SSID_PREFIX = "WiMote_";
const char *HOSTNAME = "WiMote";

void initWiFi()
{
  JsonObject wifiObj = globalDoc["wifi"];

  WiFi.hostname(HOSTNAME);

  if (!config.wifi.ssid.length())
  {
    // Create an open access point
    String apName = AP_SSID_PREFIX + String((uint32_t)ESP.getEfuseMac(), HEX);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str());

    IPAddress apIP = WiFi.softAPIP();

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);

    wifiObj["status"] = "AP Mode";
    wifiObj["ssid"] = apName;
    wifiObj["ip"] = apIP.toString();

    initWebServer();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8));
  WiFi.begin(config.wifi.ssid.c_str(), config.wifi.password.c_str());
}

WiFiInfo getWiFiInfo()
{
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

void wifiTask(void *parameter)
{
  int retries = 0;
  const int maxRetries = 10;

  while (1)
  {
    if (WiFi.getMode() == WIFI_AP)
    {
      WiFiInfo info = getWiFiInfo();
      JsonObject wifi = globalDoc["wifi"];
      wifi["status"] = "AP Mode";
      wifi["connected_stations"] = WiFi.softAPgetStationNum();
      wifi["ip"] = WiFi.softAPIP().toString();
      wifi["last_update"] = millis();
    }
    else if (WiFi.status() != WL_CONNECTED)
    {
      globalDoc["wifi"]["status"] = "disconnected";
      globalDoc["wifi"]["retry_count"] = retries;
      globalDoc["wifi"]["last_update"] = millis();

      if (retries < maxRetries)
      {
        WiFi.disconnect();
        WiFi.reconnect();
        retries++;
      }
      else
      {
        if (WiFi.getMode() == WIFI_AP && WiFi.softAPgetStationNum() > 0)
        {
          retries = 0;
          continue;
        }
        globalDoc["wifi"]["status"] = "restarting";
        ESP.restart();
      }
    }
    else
    {
      if (retries > 0)
      {
        initWebServer();
        initNTP();
        updateTimeInfo();
        retries = 0;
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

void handleDNS()
{
  if (!config.wifi.ssid.length())
  {
    dnsServer.processNextRequest();
  }
}
