#include <Arduino.h>
#include <WiFi.h>
#include "mqtt_client.h"
#include <PubSubClient.h>
#include "config.h"
#include "ir.h"
#include <queue>
#include <ArduinoJson.h>

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
        String expectedSetTopic = mqttManager.getFullTopic("ir/transmitted/set");
        String expectedTopic = mqttManager.getFullTopic("ir/transmitted");

        if (topicStr == expectedSetTopic) {
            if (firstConnect) {
                firstConnect = false;
                return;
            }

            // Сохраняем оригинальное сообщение
            String originalMessage;
            for(unsigned int i = 0; i < length; i++) {
                originalMessage += (char)payload[i];
            }

            // Пропускаем пустые сообщения
            if (originalMessage.length() == 0) {
                return;
            }

            // Очищаем retained сообщение в топике set
            mqttManager.publish("ir/transmitted/set", "", true);

            // Отправляем IR команду
            Serial.printf("Transmitting IR command: %s\n", originalMessage.c_str());
            irManager.transmit(originalMessage.c_str());

            // Публикуем оригинальное сообщение в ir/transmitted
            mqttManager.publish("ir/transmitted", originalMessage.c_str(), true);
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
    subscribe("ir/transmitted/set");
    publish("status", "online", true);

    // Публикуем формат ожидаемого сообщения
    const char* format_help = "Expected format: {\"protocol\":\"NEC|RC5|RC6|SAMSUNG\",\"value\":\"0xFFFFFFFF\",\"bits\":32}";
    publish("ir/transmitted/format", format_help, true);

    Serial.println("MQTT: Connected successfully");
}
