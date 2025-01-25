#include "web_server.h"
#include <SPIFFS.h>
#include <AsyncJson.h>
#include <WiFi.h>
#include "config.h"
#include "ntp_client.h"
#include "mqtt_client.h"
#include "ir.h" // Добавляем включение заголовочного файла

AsyncWebServer server(80);

bool saveConfigFile(const char* json) {
    if(!SPIFFS.begin(true)) {
        return false;
    }

    File file = SPIFFS.open("/config.json", "w");
    if(!file) {
        return false;
    }

    size_t bytesWritten = file.print(json);
    file.close();
    return bytesWritten > 0;
}

// Убедимся, что нет использования таймеров
void initWebServer() {
    if(!SPIFFS.begin(true)) {
        return;
    }

    // Комментируем или удаляем server.serveStatic("/fa-solid-900.woff2", ...)
    // server.serveStatic("/fa-solid-900.woff2", SPIFFS, "/fa-solid-900.woff2")
    //     .setCacheControl("max-age=31536000");

    // Добавляем ручной обработчик для /fa-solid-900.woff2
    server.on("/fa-solid-900.woff2", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse* response = request->beginResponse(
            SPIFFS,
            "/fa-solid-900.woff2",
            "font/woff2"
        );
        response->addHeader("Cache-Control", "max-age=31536000");
        request->send(response);
    });

    // Удаляем проверку WiFi статуса, чтобы сервер работал и в режиме AP
    // if(WiFi.getMode() != WIFI_AP && WiFi.status() != WL_CONNECTED) {
    //     return;
    // }

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

    // Добавляем обработчик для корневого пути в режиме AP
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Проверяем, находимся ли мы в режиме AP
        if (WiFi.getMode() == WIFI_AP && request->host() == "192.168.4.1") {
            // В режиме AP отправляем страницу конфигурации
            request->send(SPIFFS, "/config.html", "text/html");
        } else {
            // В обычном режиме отправляем главную страницу
            request->send(SPIFFS, "/index.html", "text/html");
        }
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/config.html", "text/html");
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json;
        serializeJson(globalDoc, json);
        request->send(200, "application/json", json);
    });

    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler(
        "/config",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!json.is<JsonObject>()) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            StaticJsonDocument<1024> doc;
            {
                File currentFile = SPIFFS.open("/config.json", "r");
                if (currentFile) {
                    DeserializationError err = deserializeJson(doc, currentFile);
                    currentFile.close();
                    if (err) {
                        doc.clear();
                    }
                }
            }

            JsonObject obj = json.as<JsonObject>();

            if (obj.containsKey("wifi")) {
                doc["wifi"] = obj["wifi"];
            }

            if (obj.containsKey("ntp")) {
                JsonObject ntpNew = obj["ntp"];
                if (!doc.containsKey("ntp")) {
                    doc.createNestedObject("ntp");
                }
                JsonObject ntpDoc = doc["ntp"];

                if (ntpNew.containsKey("server")) {
                    ntpDoc["server"] = ntpNew["server"].as<const char*>();
                }
                if (ntpNew.containsKey("timezone")) {
                    ntpDoc["timezone"] = ntpNew["timezone"].as<int>();
                }
                if (ntpNew.containsKey("update_interval")) {
                    ntpDoc["update_interval"] = ntpNew["update_interval"].as<int>();
                }
            }

            if (obj.containsKey("mqtt")) {
                JsonObject mqttNew = obj["mqtt"];
                if (!doc.containsKey("mqtt")) {
                    doc.createNestedObject("mqtt");
                }
                JsonObject mqttDoc = doc["mqtt"];

                if (mqttNew.containsKey("clientID")) {
                    mqttDoc["clientID"] = mqttNew["clientID"].as<const char*>();
                }
                if (mqttNew.containsKey("host")) {
                    mqttDoc["host"] = mqttNew["host"].as<const char*>();
                }
                if (mqttNew.containsKey("port")) {
                    mqttDoc["port"] = mqttNew["port"].as<int>();
                }
                if (mqttNew.containsKey("user")) {
                    mqttDoc["user"] = mqttNew["user"].as<const char*>();
                }
                if (mqttNew.containsKey("password")) {
                    mqttDoc["password"] = mqttNew["password"].as<const char*>();
                }
                if (mqttNew.containsKey("base_topic")) {
                    mqttDoc["base_topic"] = mqttNew["base_topic"].as<const char*>();
                }
            }

            String jsonString;
            serializeJson(doc, jsonString);
            if (saveConfigFile(jsonString.c_str())) {
                loadConfig();
                if (obj.containsKey("ntp")) {
                    initNTP();
                }
                if (obj.containsKey("mqtt")) {
                    // Заменяем initMQTT() на mqttManager.init()
                    mqttManager.init();
                }
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Save failed");
            }
        }
    );

    server.addHandler(handler);
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    server.on("/config/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(resetWiFiConfig()) {
            request->send(200, "text/plain", "OK");
            ESP.restart();
        } else {
            request->send(500, "text/plain", "Failed to reset WiFi configuration");
        }
    });

    // Добавляем обработчик для сброса MQTT конфигурации
    server.on("/config/reset-mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(resetMQTTConfig()) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Failed to reset MQTT configuration");
        }
    });

    // Добавляем обработчик для сброса NTP конфигурации
    server.on("/config/reset-ntp", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(resetNTPConfig()) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "Failed to reset NTP configuration");
        }
    });

    // Добавляем обработчик для полного сброса конфигурации
    server.on("/config/reset-all", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(resetAllConfig()) {
            request->send(200, "text/plain", "OK");
            ESP.restart();
        } else {
            request->send(500, "text/plain", "Failed to reset configuration");
        }
    });

    // Добавляем обработчики для автоматического открытия страницы настройки
    server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("http://" + WiFi.softAPIP().toString());
    });

    server.on("/fwlink", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("http://" + WiFi.softAPIP().toString());
    });

    server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("http://" + WiFi.softAPIP().toString());
    });

    server.on("/library/test/success.html", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("http://" + WiFi.softAPIP().toString());
    });

    server.on("/kindle-wifi/wifistub.html", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("http://" + WiFi.softAPIP().toString());
    });

    server.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    server.on("/redirect", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    server.on("/success.txt", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "success");
    });

    server.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    server.on("/api/mqtt/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (resetMQTTConfig()) {
            mqttManager.init();  // Заменяем initMQTT() на вызов метода init() класса MQTTManager
            request->send(200, "application/json", R"({"status":"ok"})");
        }
        else {
            request->send(500, "application/json", R"({"status":"error"})");
        }
    });

    // Обновляем обработчик для IR команд
    server.on("/ir/send", HTTP_POST,
        [](AsyncWebServerRequest *request){},  // Пустой обработчик для POST
        NULL,                                  // Обработчик для upload
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (!len) {
                request->send(400, "application/json", "{\"error\":\"Empty request\"}");
                return;
            }

            // Создаем временный буфер для добавления завершающего нуля
            char *buffer = new char[len + 1];
            memcpy(buffer, data, len);
            buffer[len] = '\0';

            // Передаем команду в IR manager
            irManager.transmit(buffer);

            delete[] buffer;
            request->send(200, "application/json", "{\"status\":\"success\"}");
        }
    );

    // Добавляем обработчик для получения IR команд
    server.on("/ir/commands", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/ir_commands.json", "application/json");
    });

    // Добавляем обработчик для сохранения IR команд
    AsyncCallbackJsonWebHandler* irHandler = new AsyncCallbackJsonWebHandler(
        "/ir/commands/save",
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            if (!json.is<JsonObject>()) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            File file = SPIFFS.open("/ir_commands.json", "w");
            if(!file) {
                request->send(500, "text/plain", "Failed to open file");
                return;
            }

            String jsonString;
            serializeJson(json, jsonString);
            if (file.print(jsonString)) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Failed to save");
            }
            file.close();
        }
    );
    server.addHandler(irHandler);

    // Обновляем обработчик для обучения IR командам
    server.on("/ir/learn", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (irManager.isLearning()) {
            request->send(409, "application/json", "{\"error\":\"Already learning\"}");
            return;
        }

        if (irManager.startLearning()) {
            request->send(200, "application/json", "{\"status\":\"learning\"}");
        } else {
            request->send(500, "application/json", "{\"error\":\"Failed to start learning\"}");
        }
    });

    // Обновляем обработчик для проверки статуса обучения
    server.on("/ir/learn/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String lastMessage = irManager.getLastMessage();

        if (irManager.isLearning()) {
            request->send(200, "application/json",
                "{\"completed\":false,\"status\":\"learning\"}");
        } else if (lastMessage.length() > 0) {
            // Проверяем, является ли сообщение ошибкой
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, lastMessage);

            if (!error && doc.containsKey("error")) {
                request->send(200, "application/json",
                    "{\"completed\":true,\"error\":\"" + String(doc["error"].as<const char*>()) + "\"}");
            } else {
                request->send(200, "application/json",
                    "{\"completed\":true,\"result\":" + lastMessage + "}");
            }
            irManager.clearBuffer();
        } else {
            request->send(200, "application/json",
                "{\"completed\":false,\"status\":\"waiting\"}");
        }
    });

    server.on("/remote", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    // Обновляем обработчик NotFound для поддержки режима AP
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (WiFi.getMode() == WIFI_AP) {
            request->redirect("/config.html");
        } else {
            request->redirect("/");
        }
    });

    server.begin();
}
