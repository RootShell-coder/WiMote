#pragma once
#include <Arduino.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <ArduinoJson.h>
#include <queue>
#include "timer_manager.h" // Добавляем включение timer_manager.h
#include "driver/rmt.h"
#include "driver/periph_ctrl.h"
#include "soc/rmt_reg.h"

// Смягчаем параметры фильтрации
#define MIN_IR_LENGTH 16        // Уменьшаем минимальную длину
#define MIN_UNKNOWN_LENGTH 32   // Уменьшаем минимальную длину для неизвестного протокола
#define NOISE_FLOOR 100        // Уменьшаем порог шума
#define MIN_SIGNAL_STRENGTH 300 // Уменьшаем минимальную силу сигнала
#define MAX_SIGNAL_VARIANCE 200 // Увеличиваем допустимое отклонение
#define MIN_REPEAT_COUNT 1      // Уменьшаем количество повторов до 1 для режима обучения

#define IR_RECEIVE_PIN 14  // IR приемник на GPIO14
#define IR_TRANSMIT_PIN 27 // IR светодиод на GPIO27
#define IR_BUFFER_SIZE 10
#define MAX_RAW_SAMPLES 100

// Удаляем определения RMT констант
// #define RMT_TX_CHANNEL 0
// #define RMT_RX_CHANNEL 1
// #define RMT_CLK_DIV 100
// #define RMT_TICK_10_US ...

class IRManager {
private:
    IRrecv irrecv;
    IRsend irsend;
    decode_results results;
    decode_results lastResults;  // Сохраняем предыдущий результат
    uint8_t repeatCount;        // Счетчик повторов
    std::queue<String> messageBuffer;
    StaticJsonDocument<512> doc;
    char jsonBuffer[512];
    bool irInitialized;  // Добавляем флаг инициализации
    bool rmt_initialized;
    rmt_channel_t channel;  // Добавляем переменную для хранения канала RMT
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    static constexpr const char* TOPIC_RECEIVED = "ir/received";
    static constexpr const char* TOPIC_UNKNOWN = "ir/unknown";

    static void task(void* parameter);
    bool isValidSignal() const;
    bool processProtocol();
    void addMetadata();
    bool serializeAndPublish(bool isValid);
    bool compareResults() const; // Метод сравнения результатов
    bool processSignal();
    bool handleLearningMode();
    bool handleNormalMode(bool isKnownProtocol);

    // Добавляем переменные для обучения
    bool learning = false;
    unsigned long learningTimeout = 0;
    static const unsigned long LEARNING_TIMEOUT = 10000; // 10 секунд

public:
    IRManager();
    ~IRManager(); // Только объявление деструктора
    bool init();
    void handle();
    void transmit(const char* message);
    String getLastMessage() const;
    void clearBuffer();

    // Добавляем методы для обучения
    bool startLearning();
    bool stopLearning();
    bool isLearning() const { return learning; }
};

extern IRManager irManager;
