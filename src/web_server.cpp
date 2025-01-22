#include "web_server.h"
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <WiFi.h>
#include "config.h"
#include "ntp_client.h"
#include "mqtt_client.h"

AsyncWebServer server(80);

bool saveConfigFile(const char *json)
{
  if (!SPIFFS.begin(true))
  {
    return false;
  }

  File file = SPIFFS.open("/config.json", "w");
  if (!file)
  {
    return false;
  }

  size_t bytesWritten = file.print(json);
  file.close();
  return bytesWritten > 0;
}

void initWebServer()
{
  if (WiFi.getMode() != WIFI_AP && WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (!SPIFFS.begin(true))
  {
    return;
  }

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html"); });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/config.json", "application/json"); });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
            {
        String json;
        serializeJson(globalDoc, json);
        request->send(200, "application/json", json); });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler(
      "/config",
      [](AsyncWebServerRequest *request, JsonVariant &json)
      {
        if (!json.is<JsonObject>())
        {
          request->send(400, "text/plain", "Invalid JSON");
          return;
        }

        StaticJsonDocument<1024> doc;
        {
          File currentFile = SPIFFS.open("/config.json", "r");
          if (currentFile)
          {
            DeserializationError err = deserializeJson(doc, currentFile);
            currentFile.close();
            if (err)
            {
              doc.clear();
            }
          }
        }

        JsonObject obj = json.as<JsonObject>();

        if (obj.containsKey("wifi"))
        {
          doc["wifi"] = obj["wifi"];
        }

        if (obj.containsKey("ntp"))
        {
          JsonObject ntpNew = obj["ntp"];
          if (!doc.containsKey("ntp"))
          {
            doc.createNestedObject("ntp");
          }
          JsonObject ntpDoc = doc["ntp"];

          if (ntpNew.containsKey("server"))
          {
            ntpDoc["server"] = ntpNew["server"].as<const char *>();
          }
          if (ntpNew.containsKey("timezone"))
          {
            ntpDoc["timezone"] = ntpNew["timezone"].as<int>();
          }
          if (ntpNew.containsKey("update_interval"))
          {
            ntpDoc["update_interval"] = ntpNew["update_interval"].as<int>();
          }
        }

        if (obj.containsKey("mqtt"))
        {
          JsonObject mqttNew = obj["mqtt"];
          if (!doc.containsKey("mqtt"))
          {
            doc.createNestedObject("mqtt");
          }
          JsonObject mqttDoc = doc["mqtt"];

          if (mqttNew.containsKey("clientID"))
          {
            mqttDoc["clientID"] = mqttNew["clientID"].as<const char *>();
          }
          if (mqttNew.containsKey("host"))
          {
            mqttDoc["host"] = mqttNew["host"].as<const char *>();
          }
          if (mqttNew.containsKey("port"))
          {
            mqttDoc["port"] = mqttNew["port"].as<int>();
          }
          if (mqttNew.containsKey("user"))
          {
            mqttDoc["user"] = mqttNew["user"].as<const char *>();
          }
          if (mqttNew.containsKey("password"))
          {
            mqttDoc["password"] = mqttNew["password"].as<const char *>();
          }
          if (mqttNew.containsKey("base_topic"))
          {
            mqttDoc["base_topic"] = mqttNew["base_topic"].as<const char *>();
          }
        }

        String jsonString;
        serializeJson(doc, jsonString);
        if (saveConfigFile(jsonString.c_str()))
        {
          loadConfig();
          if (obj.containsKey("ntp"))
          {
            initNTP();
          }
          if (obj.containsKey("mqtt"))
          {
            initMQTT();
          }
          request->send(200, "text/plain", "OK");
        }
        else
        {
          request->send(500, "text/plain", "Save failed");
        }
      });

  server.addHandler(handler);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  server.on("/config/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        if(resetWiFiConfig()) {
            request->send(200, "text/plain", "OK");
            ESP.restart();
        } else {
            request->send(500, "text/plain", "Failed to reset WiFi configuration");
        } });

  server.on("/config/reset-mqtt", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        if(resetMQTTConfig()) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Failed to reset MQTT configuration");
        } });

  server.on("/config/reset-ntp", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        if(resetNTPConfig()) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Failed to reset NTP configuration");
        } });

  server.on("/config/reset-all", HTTP_POST, [](AsyncWebServerRequest *request)
            {
        if(resetAllConfig()) {
            request->send(200, "text/plain", "OK");
            ESP.restart();
        } else {
            request->send(500, "text/plain", "Failed to reset configuration");
        } });

  server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("http://" + WiFi.softAPIP().toString()); });

  server.on("/fwlink", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("http://" + WiFi.softAPIP().toString()); });

  server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("http://" + WiFi.softAPIP().toString()); });

  server.on("/library/test/success.html", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("http://" + WiFi.softAPIP().toString()); });

  server.on("/kindle-wifi/wifistub.html", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("http://" + WiFi.softAPIP().toString()); });

  server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("/"); });

  server.on("/redirect", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("/"); });

  server.on("/success.txt", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "success"); });

  server.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *request)
            { request->redirect("/"); });

  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->redirect("http://" + WiFi.softAPIP().toString()); });

  server.begin();
}
