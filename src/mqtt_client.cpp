#include <Arduino.h>
#include <WiFi.h>
#include "mqtt_client.h"
#include <PubSubClient.h>
#include "config.h"
#include "ir.h"
#include <queue>

MQTTManager mqttManager;

MQTTManager::MQTTManager() : client(wifiClient) {}

void MQTTManager::init() {
    client.setBufferSize(2048);
    client.setKeepAlive(60);
    client.setServer(config.mqtt.host.c_str(), config.mqtt.port);
    setupCallbacks();
}

void MQTTManager::setupCallbacks() {
    static bool firstConnect = true;

    client.setCallback([](char* topic, byte* payload, unsigned int length) {
        String topicStr = String(topic);
        String expectedTopic = mqttManager.getFullTopic("ir/transmitted");

        if (topicStr == expectedTopic) {
            // Пропускаем первое сообщение после подключения (retained)
            if (firstConnect) {
                firstConnect = false;
                return;
            }

            char message[length + 1];
            memcpy(message, payload, length);
            message[length] = '\0';

            Serial.printf("Received IR command: %s\n", message);
            irManager.transmit(message);
        }
    });
}

bool MQTTManager::connect() {
    if (!isNetworkReady()) return false;

    String clientId = generateClientId();
    String willTopic = getFullTopic("status");

    disconnectIfNeeded();

    if (tryConnect(clientId, willTopic)) {
        onConnected();
        return true;
    }

    return false;
}

bool MQTTManager::publish(const char* topic, const char* message, bool retain) {
    if (!client.connected()) {
        if (messageQueue.size() < MAX_QUEUE_SIZE) {
            messageQueue.push({String(topic), String(message), retain});
        }
        return false;
    }

    return client.publish(getFullTopic(topic).c_str(), message, retain);
}

void MQTTManager::processQueue() {
    while (!messageQueue.empty() && client.connected()) {
        const auto& msg = messageQueue.front();
        if (publish(msg.topic.c_str(), msg.payload.c_str(), msg.retain)) {
            messageQueue.pop();
        } else {
            break;
        }
    }
}

void MQTTManager::handle() {
    if (!isNetworkReady()) {
        disconnectIfNeeded();
        return;
    }

    if (!client.connected()) {
        reconnectIfNeeded();
    } else {
        client.loop();
        processQueue();
    }
}

String MQTTManager::getFullTopic(const char* topic) const {
    return config.mqtt.base_topic + config.mqtt.clientID + "/" + topic;
}

void MQTTManager::subscribe(const char* topic, bool noLocal) {
    String fullTopic = getFullTopic(topic);
    client.subscribe(fullTopic.c_str());
}

// Для обратной совместимости
int getMQTTState() {
    return mqttManager.getState();
}

String MQTTManager::generateClientId() const {
    String clientId = config.mqtt.clientID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    clientId.toLowerCase();
    return clientId;
}

bool MQTTManager::tryConnect(const String& clientId, const String& willTopic) {
    return config.mqtt.user.length() > 0 ?
        client.connect(clientId.c_str(), config.mqtt.user.c_str(), config.mqtt.password.c_str(),
                      willTopic.c_str(), 0, true, "offline") :
        client.connect(clientId.c_str(), willTopic.c_str(), 0, true, "offline");
}

void MQTTManager::onConnected() {
    state = 0;
    firstConnect = true;
    subscribe("ir/transmitted");
    publish("status", "online", true);
    Serial.println("MQTT: Connected successfully");
}
