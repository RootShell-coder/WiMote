#include "timer_manager.h"

hw_timer_t* TimerManager::timer = nullptr;
bool TimerManager::initialized = false;
portMUX_TYPE TimerManager::timerMux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t TimerManager::timerSemaphore = NULL;

bool TimerManager::initSemaphore() {
    // Создаем семафор только если он еще не создан
    if (timerSemaphore == NULL) {
        timerSemaphore = xSemaphoreCreateMutex();
        if (timerSemaphore == NULL) {
            Serial.println("Failed to create timer semaphore");
            return false;
        }
        Serial.println("Timer semaphore created successfully");
    }
    return true;
}

bool TimerManager::init() {
    static bool firstInit = true;  // Флаг первой инициализации

    if (!initSemaphore()) {
        return false;
    }

    if (!enterCritical()) {
        Serial.println("Failed to enter critical section during init");
        return false;
    }

    bool result = false;
    do {
        if (!initialized) {
            if (firstInit) {
                // Только при первой инициализации
                firstInit = false;
                if (timer != nullptr) {
                    timerAlarmDisable(timer);
                    timerEnd(timer);
                    timer = nullptr;
                }
            }

            timer = timerBegin(TIMER_NUMBER, TIMER_PRESCALER, TIMER_COUNT_UP);
            if (timer == nullptr) {
                Serial.println("Failed to create timer");
                break;
            }

            timerWrite(timer, 0);
            timerAlarmWrite(timer, 1000000, true);
            initialized = true;
            result = true;
            Serial.println("Timer initialized successfully");
        } else {
            result = true;
        }
    } while(0);

    exitCritical();
    return result;
}

void TimerManager::deinit() {
    if (enterCritical()) {
        if (timer != nullptr) {
            timerEnd(timer);
            timer = nullptr;
        }
        initialized = false;
        exitCritical();
    }
}

bool TimerManager::reinit() {
    if (enterCritical()) {
        deinit();
        bool result = init();
        exitCritical();
        return result;
    }
    return false;
}

void TimerManager::setAlarm(uint64_t alarm_value) {
    if (!enterCritical()) {
        return;
    }

    if (initialized && timer != nullptr) {
        timerAlarmDisable(timer);
        timerWrite(timer, 0);
        timerAlarmWrite(timer, alarm_value, true);  // Исправленный вызов
        timerAlarmEnable(timer);
    }

    exitCritical();
}

void TimerManager::enableInterrupt() {
    if (!enterCritical()) {
        return;
    }

    if (initialized && timer != nullptr) {
        timerAlarmEnable(timer);
    }

    exitCritical();
}

void TimerManager::disableInterrupt() {
    if (!enterCritical()) {
        return;
    }

    if (initialized && timer != nullptr) {
        timerAlarmDisable(timer);
    }

    exitCritical();
}

void TimerManager::cleanup() {
    if (enterCritical()) {
        if (timer != nullptr) {
            timerEnd(timer);
            timer = nullptr;
        }
        initialized = false;
        exitCritical();
    }

    // Очищаем семафор в последнюю очередь
    if (timerSemaphore != NULL) {
        vSemaphoreDelete(timerSemaphore);
        timerSemaphore = NULL;
    }
}

bool TimerManager::isInitialized() {
    return initialized && timer != nullptr;
}

TimerManager timerManager;
