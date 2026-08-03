#include "sensors/Co2Sensor.h"

#include <HardwareSerial.h>
#include <MHZ19.h>

#include "app_config.h"
#include "app_state.h"

namespace {
HardwareSerial co2Serial(2);
MHZ19 myMHZ19;
SemaphoreHandle_t co2SerialMutex = nullptr;
}  // namespace

namespace sensor {
void begin() {
    co2Serial.begin(appconfig::kCo2SensorBaud, SERIAL_8N1, appconfig::kCo2SensorRxPin, appconfig::kCo2SensorTxPin);
    myMHZ19.begin(co2Serial);
    myMHZ19.autoCalibration(false);

    if (co2SerialMutex == nullptr) {
        co2SerialMutex = xSemaphoreCreateMutex();
    }
}

void loop() {
    if (co2SerialMutex != nullptr && xSemaphoreTake(co2SerialMutex, pdMS_TO_TICKS(600)) != pdTRUE) {
        return;
    }

    int ppm = myMHZ19.getCO2();
    bool ok = ppm >= 250 && ppm <= 10000;

    if (co2SerialMutex != nullptr) {
        xSemaphoreGive(co2SerialMutex);
    }

    if (ok) {
        if (lockAppState()) {
            gAppState.sensorConnected = true;
            gAppState.co2Ppm = ppm;
            gAppState.lastValidPpm = ppm;
            gAppState.sensorError.clear();
            unlockAppState();
        }
        return;
    }

    if (lockAppState()) {
        gAppState.sensorConnected = false;
        gAppState.sensorError = "CO2 read failed";
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
