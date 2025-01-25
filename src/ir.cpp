#include "ir.h"
#include "timer_manager.h"  // Перемещаем в начало
#include "mqtt_client.h"
#include "config.h"
#include <IRutils.h>

IRManager irManager;

IRManager::IRManager() : irrecv(IR_RECEIVE_PIN), irsend(IR_TRANSMIT_PIN) {
    irInitialized = false;
}

bool IRManager::init() {
    static bool initialized = false;  // Статическая переменная для отслеживания первой инициализации

    if (!initialized) {
        if (!irInitialized) {
            irsend.begin();
            irInitialized = true;
        }

        irrecv.enableIRIn();
        initialized = true;

        xTaskCreate(
            task,
            "IR",
            config.system.ir_stack,
            this,
            1,
            NULL
        );
    }
    return true;
}

void IRManager::task(void* parameter) {
    auto* manager = static_cast<IRManager*>(parameter);
    const TickType_t xDelay = pdMS_TO_TICKS(10);

    while(true) {
        manager->handle();
        vTaskDelay(xDelay);
    }
}

bool IRManager::compareResults() const {
    // Проверяем базовые параметры
    if (results.decode_type != lastResults.decode_type ||
        results.bits != lastResults.bits) {
        return false;
    }

    // Для известных протоколов сравниваем значения
    if (results.decode_type != decode_type_t::UNKNOWN) {
        return (abs(int64_t(results.value) - int64_t(lastResults.value)) < MAX_SIGNAL_VARIANCE);
    }

    // Для неизвестного протокола сравниваем сырые данные
    if (results.rawlen != lastResults.rawlen) {
        return false;
    }

    // Проверяем каждый сэмпл с учетом допустимого отклонения
    for (uint16_t i = 1; i < results.rawlen; i++) {
        uint32_t current = results.rawbuf[i] * RAWTICK;
        uint32_t last = lastResults.rawbuf[i] * RAWTICK;
        if (abs(int32_t(current - last)) > MAX_SIGNAL_VARIANCE) {
            return false;
        }
    }

    return true;
}

bool IRManager::isValidSignal() const {
    // Базовые проверки
    if (results.overflow) return false;

    // В режиме обучения пропускаем больше сигналов
    if (learning) {
        return results.decode_type != decode_type_t::UNKNOWN ||
               (results.rawlen >= MIN_UNKNOWN_LENGTH);
    }

    // Обычный режим с более строгой проверкой
    if (results.decode_type == decode_type_t::UNKNOWN) {
        if (results.rawlen < MIN_UNKNOWN_LENGTH) return false;
    } else {
        if (results.bits < MIN_IR_LENGTH) return false;
    }

    // Проверка силы сигнала
    uint32_t signalStrength = 0;
    uint32_t maxMark = 0;
    uint32_t markCount = 0;

    for (uint16_t i = 1; i < results.rawlen; i++) {
        uint32_t duration = results.rawbuf[i] * RAWTICK;
        if (i % 2) { // Mark
            maxMark = max(maxMark, duration);
            markCount++;
        }
        signalStrength += duration;
    }

    // Базовые проверки силы сигнала
    if (signalStrength < MIN_SIGNAL_STRENGTH) return false;
    if (maxMark < NOISE_FLOOR) return false;
    if (markCount < 4) return false;

    return true;
}

bool IRManager::processProtocol() {
    const char* protocol = "UNKNOWN";
    bool knownProtocol = true;

    switch (results.decode_type) {
        case decode_type_t::NEC: protocol = "NEC"; break;
        case decode_type_t::SONY: protocol = "SONY"; break;
        case decode_type_t::RC5: protocol = "RC5"; break;
        case decode_type_t::RC6: protocol = "RC6"; break;
        case decode_type_t::DISH: protocol = "DISH"; break;
        case decode_type_t::SHARP: protocol = "SHARP"; break;
        case decode_type_t::JVC: protocol = "JVC"; break;
        case decode_type_t::SAMSUNG: protocol = "SAMSUNG"; break;
        case decode_type_t::LG: protocol = "LG"; break;
        case decode_type_t::DENON: protocol = "DENON"; break;
        case decode_type_t::PANASONIC: protocol = "PANASONIC"; break;
        case decode_type_t::MITSUBISHI: protocol = "MITSUBISHI"; break;
        case decode_type_t::SANYO: protocol = "SANYO"; break;
        case decode_type_t::WHYNTER: protocol = "WHYNTER"; break;
        case decode_type_t::COOLIX: protocol = "COOLIX"; break;
        case decode_type_t::GREE: protocol = "GREE"; break;
        case decode_type_t::HITACHI_AC: protocol = "HITACHI_AC"; break;
        case decode_type_t::KELVINATOR: protocol = "KELVINATOR"; break;
        case decode_type_t::CARRIER_AC: protocol = "CARRIER_AC"; break;
        case decode_type_t::DAIKIN: protocol = "DAIKIN"; break;
        case decode_type_t::MIDEA: protocol = "MIDEA"; break;
        case decode_type_t::HAIER_AC: protocol = "HAIER_AC"; break;
        default:
            knownProtocol = false;
            // Если нет достаточного количества сырых данных, просто выходим
            if (results.rawlen < 4) {  // Минимум 4 значения для формирования сигнала
                return false;
            }

            // Анализ неизвестного сигнала
            uint32_t total_duration = 0;
            uint32_t max_mark = 0;
            uint32_t max_space = 0;
            uint32_t min_mark = UINT32_MAX;
            uint32_t min_space = UINT32_MAX;
            bool hasValidData = false;

            for (uint16_t i = 1; i < results.rawlen; i++) {
                uint32_t duration = results.rawbuf[i] * RAWTICK;
                if (duration > NOISE_FLOOR) {
                    hasValidData = true;
                }
                total_duration += duration;

                if (i % 2) { // Mark (включенное состояние)
                    max_mark = max(max_mark, duration);
                    min_mark = min(min_mark, duration);
                } else { // Space (выключенное состояние)
                    max_space = max(max_space, duration);
                    min_space = min(min_space, duration);
                }
            }

            // Если нет валидных данных или общая длительность слишком мала
            if (!hasValidData || total_duration < MIN_SIGNAL_STRENGTH) {
                return false;
            }

            doc.clear();
            doc["protocol"] = protocol;

            JsonObject analysis = doc.createNestedObject("analysis");
            analysis["total_duration_us"] = total_duration;

            JsonObject marks = analysis.createNestedObject("marks");
            marks["min_us"] = min_mark;
            marks["max_us"] = max_mark;

            JsonObject spaces = analysis.createNestedObject("spaces");
            spaces["min_us"] = min_space;
            spaces["max_us"] = max_space;

            // Добавляем сырые данные
            doc["rawlen"] = results.rawlen;
            auto raw = doc.createNestedArray("raw");
            for(int i = 0; i < min(MAX_RAW_SAMPLES, (int)results.rawlen); i++) {
                raw.add(results.rawbuf[i] * RAWTICK);
            }
            return knownProtocol;
    }

    doc.clear();
    doc["protocol"] = protocol;
    doc["code"] = resultToHexidecimal(&results);
    doc["bits"] = results.bits;
    doc["value"] = uint64ToString(results.value, HEX);
    doc["address"] = uint64ToString(results.address, HEX);
    doc["command"] = uint64ToString(results.command, HEX);

    return knownProtocol;
}

void IRManager::addMetadata() {
    doc["timestamp"] = millis();

    if (doc["protocol"] == "UNKNOWN") {
        doc["rawlen"] = results.rawlen;
        auto raw = doc.createNestedArray("raw");
        for(int i = 0; i < min(MAX_RAW_SAMPLES, (int)results.rawlen); i++) {
            raw.add(results.rawbuf[i] * RAWTICK);
        }
    }
}

bool IRManager::serializeAndPublish(bool isKnownProtocol) {
    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    if (len > 0 && len < sizeof(jsonBuffer)) {
        if (messageBuffer.size() >= IR_BUFFER_SIZE) {
            messageBuffer.pop();
        }
        messageBuffer.push(jsonBuffer);

        bool result = mqttManager.publish(
            isKnownProtocol ? "ir/received" : "ir/unknown", // Публикуем в зависимости от протокола
            jsonBuffer,
            false
        );

        if (!result) {
            Serial.println("Failed to publish IR message");
        }
        return result;
    }
    Serial.println("Failed to serialize IR message");
    return false;
}

bool IRManager::startLearning() {
    if (learning) return false;

    learning = true;
    learningTimeout = millis() + LEARNING_TIMEOUT;
    clearBuffer(); // Очищаем буфер перед началом обучения
    return true;
}

bool IRManager::stopLearning() {
    if (!learning) return false;

    learning = false;
    learningTimeout = 0;
    return true;
}

void IRManager::handle() {
    if (!irrecv.decode(&results)) {
        if (learning && millis() > learningTimeout) {
            stopLearning();
            clearBuffer();
            doc.clear();
            doc["error"] = "Learning timeout";
            serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
            messageBuffer.push(jsonBuffer);
            Serial.println("Learning timeout");
        }
        return;
    }

    processSignal();
    irrecv.resume();
}

bool IRManager::processSignal() {
    if (!isValidSignal()) return false;

    bool isKnownProtocol = processProtocol();
    addMetadata();

    if (learning) {
        return handleLearningMode();
    }

    return handleNormalMode(isKnownProtocol);
}

bool IRManager::handleLearningMode() {
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    Serial.printf("Learned signal: %s\n", jsonBuffer);
    messageBuffer.push(jsonBuffer);
    stopLearning();
    return true;
}

bool IRManager::handleNormalMode(bool isKnownProtocol) {
    if (compareResults()) {
        if (++repeatCount >= MIN_REPEAT_COUNT) {
            bool result = serializeAndPublish(isKnownProtocol);
            repeatCount = 0;
            return result;
        }
    } else {
        repeatCount = 1;
        memcpy(&lastResults, &results, sizeof(decode_results));
    }
    return true;
}

void IRManager::transmit(const char* message) {
    static uint8_t transmitCount = 0;  // Счетчик попыток передачи
    StaticJsonDocument<512> doc;
    bool success = false;
    String command = message;

    if (!timerManager.enterCritical()) {
        Serial.println("Failed to acquire timer lock");
        return;
    }

    do {
        // Проверяем инициализацию только при первой попытке
        if (transmitCount == 0 && !timerManager.isInitialized()) {
            Serial.println("Timer not initialized, reinitializing...");
            if (!timerManager.reinit()) {
                Serial.println("Failed to reinitialize timer!");
                break;
            }
        }

        transmitCount++;  // Увеличиваем счетчик попыток

        // ... остальной код передачи ...

        success = true;
    } while(0);

    timerManager.exitCritical();

    if (success) {
        Serial.println("Successfully sent IR command: " + command);
        transmitCount = 0;  // Сбрасываем счетчик при успехе
    } else {
        Serial.println("Failed to send IR command: " + command);
        if (transmitCount >= 3) {  // Если было 3 неудачные попытки
            transmitCount = 0;
            Serial.println("Reset transmit counter after 3 failures");
        }
    }
}

String IRManager::getLastMessage() const {
    return messageBuffer.empty() ? String() : messageBuffer.back();
}

void IRManager::clearBuffer() {
    while (!messageBuffer.empty()) {
        messageBuffer.pop();
    }
}

IRManager::~IRManager() {
    if (irInitialized) {
        irrecv.disableIRIn();
        irInitialized = false;
    }
}
