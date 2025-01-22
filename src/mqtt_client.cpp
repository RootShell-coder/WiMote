#include <Arduino.h>
#include <WiFi.h>
#include "mqtt_client.h"
#include <PubSubClient.h>
#include "config.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool connectMQTTBroker()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  String clientId = config.mqtt.clientID;
  clientId += "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  bool connected = false;

  if (config.mqtt.user.length() > 0)
  {
    connected = mqttClient.connect(clientId.c_str(),
                                   config.mqtt.user.c_str(),
                                   config.mqtt.password.c_str());
  }
  else
  {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (connected)
  {
    String baseTopic = config.mqtt.base_topic + config.mqtt.clientID + "/#";
    subscribeTopic(baseTopic.c_str());
  }
  return connected;
}

void callback(char *topic, byte *payload, unsigned int length)
{
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
}

void initMQTT()
{
  mqttClient.setClient(espClient);
  mqttClient.setServer(config.mqtt.host.c_str(), config.mqtt.port);
  mqttClient.setCallback(callback);
}

void handleMQTT()
{
  static unsigned long lastReconnectAttempt = 0;

  if (WiFi.status() != WL_CONNECTED)
  {
    if (mqttClient.connected())
      mqttClient.disconnect();
    return;
  }

  if (!mqttClient.connected())
  {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > MQTT_RECONNECT_DELAY)
    {
      lastReconnectAttempt = now;
      if (connectMQTTBroker())
      {
        Serial.println("MQTT reconnected");
      }
    }
  }
  else
  {
    mqttClient.loop();
  }
}

bool publishMessage(const char *topic, const char *message)
{
  String fullTopic = config.mqtt.base_topic + config.mqtt.clientID + "/" + topic;
  return mqttClient.publish(fullTopic.c_str(), message);
}

bool publishMessage(const char *topic, const char *message, bool retain)
{
  String fullTopic = config.mqtt.base_topic + config.mqtt.clientID + "/" + topic;
  return mqttClient.publish(fullTopic.c_str(), message, retain);
}

void subscribeTopic(const char *topic)
{
  mqttClient.subscribe(topic);
}

int getMQTTState()
{
  return mqttClient.state();
}
