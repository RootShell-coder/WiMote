#pragma once

#include <Arduino.h>

void initTasks();
void loggerTask(void *parameter);
void timeUpdateTask(void *parameter);
void ntpTask(void *parameter); // Добавляем объявление ntpTask

// Удаляем объявление старой функции mqttTask
// void mqttTask(void *parameter);
