#pragma once
#include <Arduino.h>

void mqttTask(void *parameter);
void publishSystemInfo();
void publishWiFiInfo();
