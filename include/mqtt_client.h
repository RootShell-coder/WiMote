#pragma once
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <queue>

struct MQTTMessage {
    String topic;
    String payload;
    bool retain;
};

class MQTTManager {
private:
    static constexpr int BUFFER_SIZE = 2048;
    static constexpr int KEEP_ALIVE = 60;
    static constexpr const char* STATUS_TOPIC = "status";
    static constexpr const char* IR_TOPIC = "ir/transmitted";
    static constexpr size_t MAX_QUEUE_SIZE = 50;

    WiFiClient wifiClient;
    PubSubClient client;
    unsigned long lastReconnectAttempt = 0;
    static const int RECONNECT_DELAY = 5000;
    int state = -3;
    bool firstConnect = true;
    std::queue<MQTTMessage> messageQueue;

    void setupCallbacks();
    String getFullTopic(const char* topic) const;
    void processQueue();
    String generateClientId() const;
    bool tryConnect(const String& clientId, const String& willTopic);
    void onConnected();
    bool isNetworkReady() const { return WiFi.status() == WL_CONNECTED; }
    void disconnectIfNeeded() { if (client.connected()) client.disconnect(); }
    void reconnectIfNeeded() {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_DELAY) {
            lastReconnectAttempt = now;
            if (connect()) {
                Serial.println("MQTT reconnected");
            }
        }
    }

public:
    MQTTManager();
    void init();
    bool connect();
    void handle();
    bool publish(const char* topic, const char* message, bool retain = false);
    void subscribe(const char* topic, bool noLocal = false);
    int getState() const { return state; }
};

extern MQTTManager mqttManager;
extern StaticJsonDocument<2048> globalDoc;
