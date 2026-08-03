#include "sensors/Co2Sensor.h"

#include <HardwareSerial.h>
#include <MHZ19.h>

#include "app_config.h"
#include "app_state.h"

namespace {
HardwareSerial co2Serial(2);
MHZ19 myMHZ19;
SemaphoreHandle_t co2SerialMutex = nullptr;
unsigned long warmupEndsAtMs = 0;

bool isWarmupActive(unsigned long nowMs) {
    return static_cast<long>(warmupEndsAtMs - nowMs) > 0;
}

uint16_t warmupRemainingSec(unsigned long nowMs) {
    if (!isWarmupActive(nowMs)) {
        return 0;
    }
    unsigned long remainingMs = warmupEndsAtMs - nowMs;
    return static_cast<uint16_t>((remainingMs + 999UL) / 1000UL);
}
}  // namespace

namespace sensor {
void begin() {
    co2Serial.begin(appconfig::kCo2SensorBaud, SERIAL_8N1, appconfig::kCo2SensorRxPin, appconfig::kCo2SensorTxPin);
    myMHZ19.begin(co2Serial);
    myMHZ19.autoCalibration(false);
    warmupEndsAtMs = millis() + appconfig::kSensorWarmupMs;

    if (co2SerialMutex == nullptr) {
        co2SerialMutex = xSemaphoreCreateMutex();
    }

    if (lockAppState()) {
        gAppState.sensorWarmingUp = true;
        gAppState.sensorWarmupRemainingSec = warmupRemainingSec(millis());
        gAppState.sensorError = "Sensor warming up";
        unlockAppState();
    }
}

void loop() {
    if (co2SerialMutex != nullptr && xSemaphoreTake(co2SerialMutex, pdMS_TO_TICKS(600)) != pdTRUE) {
        return;
    }

    int ppm = myMHZ19.getCO2();
    bool ok = ppm >= 250 && ppm <= 10000;
    unsigned long nowMs = millis();
    bool warmingUp = isWarmupActive(nowMs);
    uint16_t remainingSec = warmupRemainingSec(nowMs);

    if (co2SerialMutex != nullptr) {
        xSemaphoreGive(co2SerialMutex);
    }

    if (ok) {
        if (lockAppState()) {
            gAppState.sensorConnected = true;
            gAppState.sensorWarmingUp = warmingUp;
            gAppState.sensorWarmupRemainingSec = remainingSec;
            gAppState.co2Ppm = ppm;
            if (warmingUp) {
                gAppState.sensorError = "Sensor warming up";
            } else {
                gAppState.lastValidPpm = ppm;
                gAppState.sensorError.clear();
            }
            unlockAppState();
        }
        return;
    }

    if (lockAppState()) {
        gAppState.sensorConnected = false;
        gAppState.sensorWarmingUp = warmingUp;
        gAppState.sensorWarmupRemainingSec = remainingSec;
        gAppState.sensorError = warmingUp ? String("Sensor warming up") : String("CO2 read failed");
        if (gAppState.lastValidPpm > 0) {
            gAppState.co2Ppm = gAppState.lastValidPpm;
        }
        unlockAppState();
    }
}

bool calibrateZero(String &error) {
    if (co2SerialMutex != nullptr && xSemaphoreTake(co2SerialMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        error = "CO2 sensor is busy";
        return false;
    }

    myMHZ19.calibrate();

    if (co2SerialMutex != nullptr) {
        xSemaphoreGive(co2SerialMutex);
    }

    error = "";
    return true;
}
}  // namespace sensor
