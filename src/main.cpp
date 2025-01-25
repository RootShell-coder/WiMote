#include <Arduino.h>
#include "tasks.h"
#include "wifi_client.h"
#include "ntp_client.h"
#include "config.h"
#include "web_server.h"
#include "mqtt_client.h"
#include "mqtt_task.h"
#include "ir.h"
#include "timer_manager.h"

// Предварительные объявления функций
void updateSystemInfo(JsonObject& systemObj);
void initializeStatus(JsonObject& wifiObj, JsonObject& timeObj, JsonObject& mqttObj);
void setupGlobalDocument();
void initializeSubsystems();
void createTasks();
void initializeSystem();

StaticJsonDocument<2048> globalDoc;

void updateSystemInfo(JsonObject& systemObj) {
    systemObj["heap_free"] = ESP.getFreeHeap();
    systemObj["heap_size"] = ESP.getHeapSize();
    systemObj["cpu_freq_mhz"] = getCpuFrequencyMhz();
    systemObj["sdk_version"] = ESP.getSdkVersion();
    systemObj["flash_size"] = ESP.getFlashChipSize() / 1024;
}

void initializeStatus(JsonObject& wifiObj, JsonObject& timeObj, JsonObject& mqttObj) {
    wifiObj["status"] = "initializing";
    wifiObj["last_update"] = 0;

    timeObj["synchronized"] = false;
    timeObj["epoch"] = 0;
    timeObj["uptime"] = 0;

    mqttObj["state"] = -3;
    mqttObj["status"] = "not connected";
}

void setupGlobalDocument() {
    globalDoc.clear();
    JsonObject systemObj = globalDoc.createNestedObject("system");
    JsonObject wifiObj = globalDoc.createNestedObject("wifi");
    JsonObject timeObj = globalDoc.createNestedObject("time");
    JsonObject mqttObj = globalDoc.createNestedObject("mqtt");

    updateSystemInfo(systemObj);
    initializeStatus(wifiObj, timeObj, mqttObj);
}

void initializeSystem() {
    if (!loadConfig()) {
        Serial.println("Configuration failed to load. Halting.");
        while (1);
    }

    setupGlobalDocument();
    initializeSubsystems();
    createTasks();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    initializeSystem();
}

void initializeSubsystems() {
    Serial.println("Initializing timer...");

    // Инициализируем семафор перед использованием
    if (!timerManager.initSemaphore()) {
        Serial.println("Failed to initialize timer semaphore!");
        while(1);
    }

    if (!timerManager.init()) {
        Serial.println("Timer initialization failed permanently!");
        while(1);
    }

    // Убираем повторные попытки инициализации
    initWiFi();
    mqttManager.init();
    irManager.init();
}

void createTasks() {
    xTaskCreate(mqttPublishTask, "MQTTPublish", config.system.task_stack, NULL, 1, NULL);
    xTaskCreate(mqttReceiveTask, "MQTTReceive", config.system.task_stack, NULL, 1, NULL);
    initTasks();
}

void loop() {
    vTaskDelete(NULL);
}
