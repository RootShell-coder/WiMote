#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include "config.h"

class WiFiManager {
public:
    static WiFiManager& instance();
    bool begin();
    void process();
    bool isConnected();
    void startPortal();
    void stopPortal();

private:
    WiFiManager();
    ~WiFiManager();
    bool connect();
    void setupCaptivePortal();
    void setupWebServer();
    bool isCaptivePortalRequest(AsyncWebServerRequest *request);
    static void handleCaptivePortal(AsyncWebServerRequest *request);

    static WiFiManager* _instance;
    AsyncWebServer* _server;
    DNSServer* _dnsServer;
    bool _isPortalActive;
};
