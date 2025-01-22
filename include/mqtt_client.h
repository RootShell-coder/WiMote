#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H
#include <PubSubClient.h>
#include <Arduino.h>

// MQTT Configuration
#ifndef MQTT_RECONNECT_DELAY
#define MQTT_RECONNECT_DELAY 5000
#endif

// Function declarations
void initMQTT();
void handleMQTT();
bool publishMessage(const char *topic, const char *message, bool retain = false);
void subscribeTopic(const char *topic);
bool connectMQTTBroker();
int getMQTTState();

extern PubSubClient mqttClient;

#endif
