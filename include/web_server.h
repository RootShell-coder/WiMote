#pragma once
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "ntp_client.h"

extern AsyncWebServer server;
void initWebServer();
bool saveConfigFile(const char *json);
