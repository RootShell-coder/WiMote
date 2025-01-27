#include "config.h"
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#define SPIFFS LittleFS

Config* Config::_instance = nullptr;

Config::Config() :
    _name("WiMote"),
    _wifi() {}

bool Config::init() {
    if(_instance == nullptr) {
        _instance = new Config();
    }

    if(!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return false;
    }

    File file = LittleFS.open("/config/config.json", "r");
    if(!file) {
        Serial.println("Failed to open config file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if(error) {
        Serial.println("Failed to parse config file");
        return false;
    }

    if(doc["name"].is<const char*>()) {
        _instance->_name = doc["name"].as<String>();
    }

    if(doc["wifi"].is<JsonObject>()) {
        JsonObject wifi = doc["wifi"];
        _instance->_wifi.ssid = wifi["ssid"].as<String>();
        // Заменяем устаревший метод containsKey на современный синтаксис
        if(wifi["password"].is<const char*>() && wifi["password"].as<String>().length() > 0) {
            _instance->_wifi.password = wifi["password"].as<String>();
        }
        _instance->_wifi.hostname = wifi["hostname"].as<String>();
        _instance->_wifi.connect_timeout = wifi["connect_timeout"] | 30000;
        _instance->_wifi.channel_width = wifi["channel_width"] | 20;
        _instance->_wifi.power_save = wifi["power_save"] | false;
        _instance->_wifi.auto_reconnect = wifi["auto_reconnect"] | false;

        if(wifi["power"].is<JsonObject>()) {
            JsonObject power = wifi["power"];
            _instance->_wifi.power.target_rssi = power["target_rssi"] | -60;
            _instance->_wifi.power.min_power = power["min_power"] | 40;
            _instance->_wifi.power.max_power = power["max_power"] | 84;
        }

        Serial.printf("Loaded WiFi config - SSID: %s, Power: %d-%d dBm\n",
            _instance->_wifi.ssid.c_str(),
            _instance->_wifi.power.min_power/4,
            _instance->_wifi.power.max_power/4);
    }

    return true;
}

Config& Config::instance() { // Изменено на неконстантную ссылку
    if(_instance == nullptr) {
        _instance = new Config();
    }
    return *_instance;
}

const char* Config::getName() const {
    return _name.c_str();
}

const WifiConfig& Config::getWifi() const {
    return _wifi;
}

void Config::setWifi(const WifiConfig& cfg) {
    _wifi = cfg;
}

bool Config::save() {
    // Сохраняем конфигурацию в файл
    File file = LittleFS.open("/config/config.json", "w");
    if (!file) {
        return false;
    }

    StaticJsonDocument<512> doc;
    doc["name"] = "WiMote";
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["ssid"] = _wifi.ssid;
    wifi["password"] = _wifi.password;
    wifi["hostname"] = _wifi.hostname;
    wifi["connect_timeout"] = _wifi.connect_timeout;
    JsonObject power = wifi.createNestedObject("power");
    power["target_rssi"] = _wifi.power.target_rssi;
    power["min_power"] = _wifi.power.min_power;
    power["max_power"] = _wifi.power.max_power;
    wifi["channel_width"] = _wifi.channel_width;
    wifi["channel"] = _wifi.channel;
    wifi["power_save"] = _wifi.power_save;
    wifi["auto_reconnect"] = _wifi.auto_reconnect;

    if (serializeJsonPretty(doc, file) == 0) {
        file.close();
        return false;
    }
    file.close();
    return true;
}
