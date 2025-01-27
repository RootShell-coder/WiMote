#include "wifiManager.h"
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <cfloat> // Добавлено для FLT_MAX

const char* PORTAL_SSID = "WiMote Setup";
const byte DNS_PORT = 53;

WiFiManager* WiFiManager::_instance = nullptr;

WiFiManager::WiFiManager() :
    _server(new AsyncWebServer(80)),
    _dnsServer(new DNSServer()),
    _isPortalActive(false) {}

WiFiManager::~WiFiManager() {
    delete _server;
    delete _dnsServer;
}

WiFiManager& WiFiManager::instance() {
    if (!_instance) {
        _instance = new WiFiManager();
    }
    return *_instance;
}

bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    const WifiConfig& cfg = Config::instance().getWifi();

    if (cfg.ssid.length() > 0) {
        Serial.printf("Attempting to connect to %s\n", cfg.ssid.c_str());
        if(connect()) {
            // После успешного подключения запускаем веб-сервер
            setupWebServer();
            _server->begin();
            return true;
        }
        // Если подключение не удалось, запускаем портал
        Serial.println("Connection failed, starting portal");
        startPortal();
        return false;
    }

    Serial.println("No WiFi credentials found, starting portal");
    startPortal();
    return false;
}

bool WiFiManager::connect() {
    const WifiConfig& cfg = Config::instance().getWifi();

    // Проверяем корректность настроек
    if(cfg.ssid.length() == 0) {
        Serial.println("SSID is empty!");
        return false;
    }

    Serial.printf("Connecting to %s (pwd length: %d)\n",
        cfg.ssid.c_str(), cfg.password.length());

    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(cfg.auto_reconnect);

    // Настраиваем WiFi перед подключением
    wifi_config_t conf;
    esp_wifi_get_config(WIFI_IF_STA, &conf);
    conf.sta.listen_interval = 1;
    esp_wifi_set_config(WIFI_IF_STA, &conf);
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Применяем настройки мощности
    esp_wifi_set_bandwidth(WIFI_IF_STA, cfg.channel_width == 40 ? WIFI_BW_HT40 : WIFI_BW_HT20);
    esp_wifi_set_max_tx_power(cfg.power.max_power);

    // Начинаем подключение
    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());

    int targetRSSI = cfg.power.target_rssi;
    int minPower = cfg.power.min_power;
    int maxPower = cfg.power.max_power;
    int currentPower = maxPower;

    esp_wifi_set_bandwidth(WIFI_IF_STA, cfg.channel_width == 40 ? WIFI_BW_HT40 : WIFI_BW_HT20);
    esp_wifi_set_max_tx_power(currentPower);

    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
    Serial.printf("Connecting to %s with power %d dBm\n", cfg.ssid.c_str(), currentPower/4);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < cfg.connect_timeout) {
        delay(500);
        Serial.print(".");

        // Динамическая регулировка мощности сигнала
        if (WiFi.status() == WL_CONNECTED) {
            int rssi = WiFi.RSSI();
            if (rssi > targetRSSI && currentPower > minPower) {
                currentPower = max(currentPower - 4, minPower);
                esp_wifi_set_max_tx_power(currentPower);
                Serial.printf("\nReducing power to %d dBm (RSSI: %d)\n", currentPower/4, rssi);
            } else if (rssi < targetRSSI - 10 && currentPower < maxPower) {
                currentPower = min(currentPower + 4, maxPower);
                esp_wifi_set_max_tx_power(currentPower);
                Serial.printf("\nIncreasing power to %d dBm (RSSI: %d)\n", currentPower/4, rssi);
            }
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        int8_t power;
        esp_wifi_get_max_tx_power(&power);
        Serial.printf("\nConnected! IP: %s, Power: %d dBm\n",
            WiFi.localIP().toString().c_str(), power/4);
        return true;
    }

    Serial.println("\nConnection failed!");
    return false;
}

void WiFiManager::setupCaptivePortal() {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP_STA);

    // Настройка AP с фиксированным IP
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(PORTAL_SSID);

    delay(500); // Важная пауза для стабильности

    Serial.printf("Portal started at IP: %s\n", apIP.toString().c_str());

    // Улучшенная настройка DNS сервера
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->setTTL(30);
    _dnsServer->start(DNS_PORT, "*", apIP);

    setupWebServer();
    _server->begin();
}

void WiFiManager::setupWebServer() {
    // Добавляем обработку CORS
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // Обработчик корневого пути
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { // Захватываем 'this'
        if(!request) return;

        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/www/config.html", "text/html");
        if(!response) {
            request->send(500, "text/plain", "Server Error");
            return;
        }

        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    // Обработчик корневого пути с проверкой типа устройства
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { // Захватываем 'this'
        if(!request) return;

        String userAgent = request->header("User-Agent");
        Serial.printf("User-Agent: %s\n", userAgent.c_str());

        if(isCaptivePortalRequest(request)) { // Теперь доступно через 'this'
            request->send(200, "text/html", "Captive Portal");
            return;
        }

        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/www/config.html", "text/html");
        if(!response) {
            request->send(500, "text/plain", "Server Error");
            return;
        }

        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    // Добавляем больше путей для различных устройств
    const char* captiveUrls[] = {
        "/generate_204", "/gen_204", "/mobile/status.php",
        "/check_network_status.txt", "/kindle-wifi/wifistub.html",
        "/wpad.dat", "/hotspot.html", "/success.txt",
        "/connectivity-check", "/check_network_status",
        "/network_check.txt", "/ping", "/apple-touch-icon.png",
        "/library/test/success.html", "/hotspotdetect.html",
        "/canonical.html", "/ncsi.txt", "/connecttest.txt"
    };

    for(const char* url : captiveUrls) {
        _server->on(url, HTTP_GET, handleCaptivePortal);
        _server->on(url, HTTP_POST, handleCaptivePortal);
    }

    _server->on("/scanner", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!request) return;

        // Начинаем сканирование при переходе на страницу
        if(WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
            WiFi.scanNetworks(true);
        }

        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/www/scanner.html", "text/html");
        if(!response) {
            request->send(500, "text/plain", "Server Error");
            return;
        }
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    _server->on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Проверяем, является ли запрос частью captive portal
        String host = request->host();
        if (host == "detectportal.firefox.com" ||
            host.endsWith(".apple.com") ||
            host.endsWith(".microsoft.com") ||
            host.endsWith(".google.com")) {
            request->send(200, "text/plain", "success");
            return;
        }

        // Если это не captive portal запрос, продолжаем со сканированием
        int n = WiFi.scanComplete();
        Serial.printf("Scan status: %d\n", n);

        if(request->host() == "detectportal.firefox.com") {
            request->send(200, "text/plain", "success");
            return;
        }

        Serial.printf("Scan status: %d\n", n);

        // Если сканирование не завершено, отправляем пустой массив
        if(n == WIFI_SCAN_RUNNING) {
            request->send(200, "application/json", "[]");
            return;
        }

        // Если сканирование не начато или завершилось с ошибкой, запускаем новое
        if(n == WIFI_SCAN_FAILED || n == -2) {
            WiFi.scanNetworks(true, true); // true, true для async сканирования с дополнительной информацией
            request->send(200, "application/json", "[]");
            return;
        }

        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();  // Создаём корневой объект
        JsonArray networks = root.createNestedArray("networks");  // Массив сетей теперь вложен

        if(n > 0) {
            // Создаем массив для сортировки по RSSI
            struct NetworkInfo {
                String ssid;
                int32_t rssi;
                uint8_t channel;
                String bssid;
                int chwidth;  // Ширина канала
                int second;   // Secondary channel
            };
            std::vector<NetworkInfo> sortedNetworks;

            // Массив для агрегации данных по каналам
            int channelCount[14] = {0};
            int channelSignal[14] = {0};

            // Получаем записи сканирования
            uint16_t number = n;
            wifi_ap_record_t* records = new wifi_ap_record_t[number];
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, records));

            for(int i = 0; i < n; i++) {
                NetworkInfo info = {
                    WiFi.SSID(i),
                    WiFi.RSSI(i),
                    static_cast<uint8_t>(WiFi.channel(i)),  // Приведение к uint8_t для устранения предупреждения
                    WiFi.BSSIDstr(i),
                    20,  // По умолчанию 20 МГц
                    0    // Secondary channel по умолчанию
                };

                // Добавляем отладочный вывод канала
                Serial.printf("AP %d: SSID=%s, Channel=%d\n", i, info.ssid.c_str(), info.channel);

                // Определяем ширину канала на основе поддержки 802.11n
                if(records[i].phy_11n) {
                    info.chwidth = 40;
                    // Опционально можно установить значение second, если требуется
                    info.second = 0; // Или любое другое значение по необходимости
                } else {
                    info.chwidth = 20; // Если нет поддержки 802.11n, устанавливаем 20 МГц
                }

                sortedNetworks.push_back(info);
                Serial.printf("Network: %s (Ch: %d, Width: %d MHz, RSSI: %d)\n",
                    info.ssid.c_str(), info.channel, info.chwidth, info.rssi);

                // Агрегация данных по каналам
                if(info.channel >=1 && info.channel <=14) {
                    channelCount[info.channel -1]++;
                    channelSignal[info.channel -1] += info.rssi;
                }
            }

            // Освобождаем память
            delete[] records;

            // Сортируем по уровню сигнала
            std::sort(sortedNetworks.begin(), sortedNetworks.end(),
                [](const NetworkInfo& a, const NetworkInfo& b) {
                    return a.rssi > b.rssi;
                });

            // Берем только 20 самых сильных сетей
            size_t numNetworks = min(20, (int)sortedNetworks.size());

            // Сохраняем сети в массив networks
            for(size_t i = 0; i < numNetworks; i++) {
                JsonObject network = networks.add<JsonObject>();
                network["ssid"]    = sortedNetworks[i].ssid;
                network["rssi"]    = sortedNetworks[i].rssi;
                network["channel"] = sortedNetworks[i].channel;
                network["bssid"]   = sortedNetworks[i].bssid;
                network["auth"]    = "Unknown";
                network["chwidth"] = sortedNetworks[i].chwidth;
            }

            // Добавляем информацию о лучших каналах в отдельный объект
            JsonObject channelInfo = root.createNestedObject("channels");  // На том же уровне что и networks
            JsonArray bestChannels = channelInfo.createNestedArray("best");

            // Сортируем каналы по уровню шума (по возрастанию)
            struct ChannelNoise {
                int channel;
                float noise;
                int networks;
            };
            std::vector<ChannelNoise> channels;

            for(int ch = 1; ch <= 14; ch++) {
                if(channelCount[ch-1] > 0) {
                    float averageRSSI = static_cast<float>(channelSignal[ch-1]) / channelCount[ch-1];
                    channels.push_back({ch, averageRSSI, channelCount[ch-1]});
                } else {
                    channels.push_back({ch, -100, 0}); // Пустые каналы помечаем как -100 dBm
                }
            }

            // Сортируем каналы по уровню шума (по возрастанию)
            std::sort(channels.begin(), channels.end(),
                [](const ChannelNoise& a, const ChannelNoise& b) {
                    if(a.networks == 0 && b.networks == 0) return a.channel < b.channel;
                    if(a.networks == 0) return true;
                    if(b.networks == 0) return false;
                    return a.noise > b.noise;
                });

            // Добавляем топ-3 лучших канала
            for(int i = 0; i < std::min(3, (int)channels.size()); i++) {
                JsonObject channelData = bestChannels.add<JsonObject>();
                channelData["channel"] = channels[i].channel;
                channelData["noise"] = channels[i].networks == 0 ? 0 : channels[i].noise;
                channelData["networks"] = channels[i].networks;

                Serial.printf("Best channel %d: Ch %d, Noise: %.1f dBm, Networks: %d\n",
                    i + 1, channels[i].channel, channels[i].noise, channels[i].networks);
            }
        }

        String output;
        serializeJson(doc, output);
        Serial.printf("Sending networks data: %s\n", output.c_str());

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
        response->addHeader("Cache-Control", "no-cache");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    _server->on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        if(!request) return;

        if(!LittleFS.exists("/config/config.json")) {
            request->send(404, "text/plain", "Config not found");
            return;
        }

        File file = LittleFS.open("/config/config.json", "r");
        if(!file) {
            request->send(500, "text/plain", "Failed to read config");
            return;
        }

        // Используем статический буфер для чтения файла
        JsonDocument doc;  // Здесь тоже меняем на JsonDocument
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if(error) {
            request->send(500, "text/plain", "Failed to parse config");
            return;
        }

        String output;
        serializeJson(doc, output);

        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
        if(!response) {
            request->send(500, "text/plain", "Server Error");
            return;
        }

        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    // Исправляем обработчик сохранения настроек
    _server->on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        if(!request) return;

        // Создаем полный JSON документ
        JsonDocument doc;
        doc["name"] = "WiMote"; // Добавляем имя устройства

        JsonObject wifi = doc["wifi"].to<JsonObject>();

        // Получаем и проверяем SSID и пароль
        String ssid = request->arg("ssid");
        String password = request->arg("password"); // Переименовали переменную для соответствия имени поля формы

        if(ssid.length() == 0) {
            // Если SSID пустой, используем значения по умолчанию
            wifi["ssid"] = "example";
            wifi["password"] = "reset";
        } else {
            wifi["ssid"] = ssid;
            if(password.length() > 0) {
                wifi["password"] = password;
            }
        }

        wifi["hostname"] = request->hasArg("hostname") ? request->arg("hostname") : "WiMote";
        wifi["connect_timeout"] = request->hasArg("timeout") ? request->arg("timeout").toInt() : 30000;

        // Сохраняем расширенные настройки
        JsonObject power = wifi["power"].to<JsonObject>();
        power["target_rssi"] = request->hasArg("target_rssi") ? request->arg("target_rssi").toInt() : -60;
        power["min_power"] = request->hasArg("min_power") ? request->arg("min_power").toInt() * 4 : 40;
        power["max_power"] = request->hasArg("max_power") ? request->arg("max_power").toInt() * 4 : 84;

        wifi["channel_width"] = request->hasArg("channel_width") ? request->arg("channel_width").toInt() : 20;
        wifi["power_save"] = request->hasArg("power_save");
        wifi["auto_reconnect"] = request->hasArg("auto_reconnect");

        // Проверяем существование директории
        if(!LittleFS.exists("/config")) {
            if(!LittleFS.mkdir("/config")) {
                request->send(500, "text/plain", "Failed to create config directory");
                return;
            }
        }

        // Сохраняем конфиг
        File file = LittleFS.open("/config/config.json", "w");
        if(!file) {
            request->send(500, "text/plain", "Failed to open config file");
            return;
        }

        // Сериализуем с отступами для читаемости
        serializeJsonPretty(doc, file);
        file.close();

        Serial.println("Config saved successfully");

        // Проверяем сохраненный файл
        file = LittleFS.open("/config/config.json", "r");
        if(file) {
            Serial.println("Saved config contents:");
            while(file.available()) {
                Serial.write(file.read());
            }
            file.close();
        }

        // Отправляем ответ
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Configuration saved. Rebooting...");
        response->addHeader("Connection", "close");
        request->send(response);

        // Даем время на отправку ответа
        delay(500);
        ESP.restart();
    });

    // Обработчик для Android и iOS captive portal
    _server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server->on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server->on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    _server->on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "success");
    });
    _server->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    // Apple Captive Portal
    _server->on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
    _server->on("/library/test/success.html", HTTP_GET, handleCaptivePortal);
    _server->on("/generate_204", HTTP_GET, handleCaptivePortal);
    _server->on("/gen_204", HTTP_GET, handleCaptivePortal);
    _server->on("/ncsi.txt", HTTP_GET, handleCaptivePortal);
    _server->on("/fwlink", HTTP_GET, handleCaptivePortal);
    _server->on("/connectivity-check", HTTP_GET, handleCaptivePortal);
    _server->on("/redirect", HTTP_GET, handleCaptivePortal);
    _server->on("/success.txt", HTTP_GET, handleCaptivePortal);
    _server->on("/connecttest.txt", HTTP_GET, handleCaptivePortal);
    _server->on("/delivery", HTTP_GET, handleCaptivePortal);

    // Android/Chrome OS Captive Portal
    _server->on("/generate_204", HTTP_POST, handleCaptivePortal);
    _server->on("/gen_204", HTTP_POST, handleCaptivePortal);
    _server->on("/generate_204", HTTP_OPTIONS, handleCaptivePortal);
    _server->on("/gen_204", HTTP_OPTIONS, handleCaptivePortal);

    // Windows Captive Portal
    _server->on("/ncsi.txt", HTTP_POST, handleCaptivePortal);
    _server->on("/connecttest.txt", HTTP_GET, handleCaptivePortal);
    _server->on("/redirect", HTTP_POST, handleCaptivePortal);

    // Firefox Captive Portal
    _server->on("/success.txt", HTTP_POST, handleCaptivePortal);
    _server->on("/success.txt", HTTP_OPTIONS, handleCaptivePortal);

    // Обработчик для всех остальных неизвестных запросов
    _server->onNotFound([this](AsyncWebServerRequest *request) { // Добавлен захват 'this'
        if(isCaptivePortalRequest(request)) {
            request->redirect("/");
            return;
        }

        String host = request->host();
        String url = request->url();
        Serial.printf("NotFound: %s%s\n", host.c_str(), url.c_str());

        // Попытка предотвратить зацикливание редиректов
        if(request->hasHeader("X-Captive-Portal")) {
            request->send(404);
            return;
        }

        request->redirect("/");
    });

    // ...existing code for other handlers...

    _server->onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/");
    });

    _server->on("/somePath", HTTP_GET, [this](AsyncWebServerRequest *request) { // Добавлен захват 'this'
        if(isCaptivePortalRequest(request)) {
            // ...existing code...
        }
        // ...existing code...
    });
}

// Новый метод для проверки captive portal запросов
bool WiFiManager::isCaptivePortalRequest(AsyncWebServerRequest *request) {
    if(!request) return false;

    String host = request->host();

    // Если это наш локальный IP - не считаем captive portal запросом
    if(host == "192.168.4.1" || host.startsWith("192.168.4.")) {
        return false;
    }

    // Для всех остальных доменов считаем captive portal запросом
    return (host != "192.168.4.1" &&
            host != request->client()->localIP().toString());
}

void WiFiManager::handleCaptivePortal(AsyncWebServerRequest *request) {
    if(!request) return;

    String host = request->host();
    String url = request->url();

    // Для определенных запросов отправляем страницу конфигурации
    if (host == "192.168.4.1" || host.startsWith("192.168.4.") ||
        url == "/" || url == "/config" || url == "/index.html") {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/www/config.html", "text/html");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
        return;
    }

    // Для запросов проверки подключения возвращаем редирект
    if(host == "detectportal.firefox.com" ||
       host.endsWith(".apple.com") ||
       host.endsWith(".microsoft.com") ||
       host.endsWith(".google.com") ||
       url.indexOf("generate_204") > -1 ||
       url.indexOf("success") > -1) {
        AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
        response->addHeader("Location", "http://192.168.4.1/");
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
        return;
    }

    // В остальных случаях делаем редирект на главную
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", "http://192.168.4.1/");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("X-Captive-Portal", "1");
    request->send(response);
}

void WiFiManager::process() {
    if (_isPortalActive && _dnsServer) {
        _dnsServer->processNextRequest();
    }
}

bool WiFiManager::isConnected() {

    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::startPortal() {
    if (!_isPortalActive) {
        setupCaptivePortal();
        _isPortalActive = true;
    }
}

void WiFiManager::stopPortal() {
    if (_isPortalActive) {
        _server->end();
        _dnsServer->stop();
        _isPortalActive = false;
    }
}
