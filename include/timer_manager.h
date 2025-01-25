#pragma once
#include <Arduino.h>

class TimerManager {
private:
    static hw_timer_t* timer;
    static bool initialized;
    static portMUX_TYPE timerMux;
    static SemaphoreHandle_t timerSemaphore;
    static const uint8_t TIMER_NUMBER = 3;  // Используем timer 3
    static const uint16_t TIMER_PRESCALER = 80;  // 80MHz / 80 = 1MHz (1μs resolution)
    static const bool TIMER_COUNT_UP = true;
    static const TickType_t MUTEX_TIMEOUT = pdMS_TO_TICKS(100);

public:
    static bool init();
    static void deinit();
    static hw_timer_t* getTimer() { return timer; }
    static bool setTimer(hw_timer_t* newTimer);
    static void setAlarm(uint64_t alarm_value);
    static void enableInterrupt();
    static void disableInterrupt();
    static void cleanup();
    static bool isInitialized();
    static bool reinit();
    static bool initSemaphore();

    // Обновленные методы для работы с критической секцией
    static bool enterCritical() {
        if (timerSemaphore == NULL) {
            return false;
        }
        return xSemaphoreTake(timerSemaphore, MUTEX_TIMEOUT) == pdTRUE;
    }
    static void exitCritical() {
        if (timerSemaphore != NULL) {
            xSemaphoreGive(timerSemaphore);
        }
    }
};

extern TimerManager timerManager;
