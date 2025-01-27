#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct PowerConfig {
    int target_rssi;
    int min_power;
    int max_power;

    PowerConfig() :
        target_rssi(-60),
        min_power(40),
        max_power(84) {}
};

struct WifiConfig {
    String ssid;
    String password;
    String hostname;
    uint32_t connect_timeout;
    PowerConfig power;
    int channel_width;
    int channel; // Добавлено поле channel
    bool power_save;
    bool auto_reconnect;

    WifiConfig() :
        ssid(""),
        password(""),
        hostname("WiMote"),
        connect_timeout(30000),
        channel_width(20),
        channel(1), // Инициализация поля channel
        power_save(false),
        auto_reconnect(false) {}
};

class Config {
private:
    static Config* _instance;
    String _name;      // Изменено с const char* на String
    WifiConfig _wifi;

    Config();

public:
    static bool init();
    static Config& instance(); // Изменено на неконстантную ссылку
    const char* getName() const;
    const WifiConfig& getWifi() const;
    void setWifi(const WifiConfig& cfg); // Добавлен метод setWifi
    bool save(); // Добавлен метод save
};
