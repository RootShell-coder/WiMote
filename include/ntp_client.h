#pragma once
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include "esp32/rom/rtc.h"
#include "config.h"

void initNTP();
time_t getEpochTime();
void updateTimeInfo();

extern const char *ntpServer;
extern const long gmtOffset_sec;
extern const int daylightOffset_sec;
