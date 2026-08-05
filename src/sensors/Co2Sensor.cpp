#include "sensors/Co2Sensor.h"

#include <SensirionErrors.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>

#include "app_config.h"
#include "app_i2c_lock.h"
#include "app_state.h"
#include "settings/Settings.h"

namespace {
SensirionI2cScd4x scd4x;
SemaphoreHandle_t sensorMutex = nullptr;
bool sensorReady = false;
bool wireInitialized = false;
unsigned long loopCounter = 0;
unsigned long nextInitAttemptMs = 0;
uint16_t appliedAltitudeMeters = appconfig::kSensorAltitudeDefaultMeters;
bool altitudeApplied = false;
constexpr unsigned long kInitRetryMs = 2000;
constexpr unsigned long kStartupDelayMs = 5000;

String decodeSensirionError(int16_t errorCode) {
    char message[96] = {0};
    errorToString(static_cast<uint16_t>(errorCode), message, sizeof(message));
    return String(message);
}

int probeI2cAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission();
}

bool tryInitSensor(const char* reason) {
    Serial.println(String("[SCD40] init: begin reason=") + reason);

    if (!applocks::lockI2c(pdMS_TO_TICKS(500))) {
        sensorReady = false;
        Serial.println("[SCD40] init: failed, I2C lock timeout");
        return false;
    }

    if (!wireInitialized) {
        Wire.begin(appconfig::kI2CSdaPin, appconfig::kI2CSclPin);
        Wire.setClock(100000);
        wireInitialized = true;
    }

    int probe62 = probeI2cAddress(SCD40_I2C_ADDR_62);
    int probe3c = probeI2cAddress(appconfig::kOledI2cAddress);
    Serial.println("[SCD40] init: probe 0x62=" + String(probe62) + " probe 0x3C=" + String(probe3c));

    scd4x.begin(Wire, SCD40_I2C_ADDR_62);
    int16_t stopError = scd4x.stopPeriodicMeasurement();
    delay(500);
    int16_t reinitError = scd4x.reinit();
    delay(30);
    SettingsData settingsSnapshot = settings::get();
    uint16_t targetAltitudeMeters = settingsSnapshot.sensorAltitudeMeters;
    int16_t altitudeError = 0;
    if (reinitError == 0) {
        altitudeError = scd4x.setSensorAltitude(targetAltitudeMeters);
    } else {
        altitudeError = reinitError;
    }
    int16_t startError = scd4x.startPeriodicMeasurement();
    int16_t lowPowerStartError = 0;
    if (startError != 0) {
        lowPowerStartError = scd4x.startLowPowerPeriodicMeasurement();
        if (lowPowerStartError == 0) {
            startError = 0;
        }
    }

    sensorReady = (startError == 0) && (altitudeError == 0);
    if (sensorReady) {
        appliedAltitudeMeters = targetAltitudeMeters;
        altitudeApplied = true;
    }
    applocks::unlockI2c();

    Serial.println("[SCD40] init: stop status=" + String(stopError) + " msg='" + decodeSensirionError(stopError) + "'");
    Serial.println("[SCD40] init: reinit status=" + String(reinitError) + " msg='" + decodeSensirionError(reinitError) + "'");
    Serial.println("[SCD40] init: setSensorAltitude(" + String(targetAltitudeMeters) + ") status=" + String(altitudeError) +
                   " msg='" + decodeSensirionError(altitudeError) + "'");
    Serial.println("[SCD40] init: startPeriodicMeasurement status=" + String(startError) +
                   " sensorReady=" + String(sensorReady ? 1 : 0) +
                   " msg='" + decodeSensirionError(startError) + "'");
    if (lowPowerStartError != 0) {
        Serial.println("[SCD40] init: startLowPowerPeriodicMeasurement status=" + String(lowPowerStartError) +
                       " msg='" + decodeSensirionError(lowPowerStartError) + "'");
    }

    if (!sensorReady) {
        nextInitAttemptMs = millis() + kInitRetryMs;
    }

    return sensorReady;
}

bool applyRuntimeAltitude(uint16_t altitudeMeters) {
    if (sensorMutex != nullptr && xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1500)) != pdTRUE) {
        Serial.println("[SCD40] altitude: sensor mutex timeout");
        return false;
    }

    if (!applocks::lockI2c(pdMS_TO_TICKS(1000))) {
        Serial.println("[SCD40] altitude: I2C lock timeout");
        if (sensorMutex != nullptr) {
            xSemaphoreGive(sensorMutex);
        }
        return false;
    }

    int16_t stopError = scd4x.stopPeriodicMeasurement();
    delay(200);
    int16_t altitudeError = 0;
    if (stopError == 0) {
        altitudeError = scd4x.setSensorAltitude(altitudeMeters);
    } else {
        altitudeError = stopError;
    }
    int16_t startError = scd4x.startPeriodicMeasurement();

    applocks::unlockI2c();
    if (sensorMutex != nullptr) {
        xSemaphoreGive(sensorMutex);
    }

    Serial.println("[SCD40] altitude: set=" + String(altitudeMeters) +
                   " stop=" + String(stopError) +
                   " setStatus=" + String(altitudeError) +
                   " start=" + String(startError));

    const bool ok = (stopError == 0) && (altitudeError == 0) && (startError == 0);
    if (ok) {
        appliedAltitudeMeters = altitudeMeters;
        altitudeApplied = true;
        return true;
    }

    return false;
}

void logLoopStatus(const char* phase, int16_t statusError, bool dataReady, bool readOk,
                   uint16_t ppm, float temperature, float humidity, bool ppmValid) {
    String msg = "[SCD40] loop#" + String(loopCounter) +
                 " phase=" + String(phase) +
                 " status=" + String(statusError) +
                 " ready=" + String(dataReady ? 1 : 0) +
                 " readOk=" + String(readOk ? 1 : 0) +
                 " ppm=" + String(ppm) +
                 " t=" + String(temperature, 1) +
                 " h=" + String(humidity, 1) +
                 " valid=" + String(ppmValid ? 1 : 0);
    Serial.println(msg);
}
}  // namespace

namespace sensor {
void begin() {
    unsigned long nowMs = millis();
    nextInitAttemptMs = nowMs + kStartupDelayMs;
    Serial.println("[SCD40] startup delay before first init: " + String(kStartupDelayMs) + " ms");

    bool started = false;
    altitudeApplied = false;
    appliedAltitudeMeters = appconfig::kSensorAltitudeDefaultMeters;

    if (sensorMutex == nullptr) {
        sensorMutex = xSemaphoreCreateMutex();
    }

    if (lockAppState()) {
        gAppState.sensorConnected = started;
        gAppState.sensorError = started ? String("") : String("SCD40 startup delay");
        unlockAppState();
    }
}

void loop() {
    ++loopCounter;

    SettingsData settingsSnapshot = settings::get();
    if (sensorReady && (!altitudeApplied || settingsSnapshot.sensorAltitudeMeters != appliedAltitudeMeters)) {
        if (!applyRuntimeAltitude(settingsSnapshot.sensorAltitudeMeters)) {
            if (lockAppState()) {
                gAppState.sensorError = "SCD40 altitude compensation update failed";
                unlockAppState();
            }
        }
    }

    if (!sensorReady) {
        unsigned long nowMs = millis();
        if (static_cast<long>(nextInitAttemptMs - nowMs) > 0) {
            unsigned long waitMs = nextInitAttemptMs - nowMs;
            Serial.println("[SCD40] loop#" + String(loopCounter) + " phase=startup-wait ms=" + String(waitMs));
            if (lockAppState()) {
                gAppState.sensorConnected = false;
                gAppState.climateValid = false;
                gAppState.sensorError = "SCD40 startup delay";
                unlockAppState();
            }
            return;
        }

        if (static_cast<long>(nowMs - nextInitAttemptMs) >= 0) {
            bool started = tryInitSensor("retry");
            if (started && lockAppState()) {
                gAppState.sensorConnected = true;
                gAppState.sensorError.clear();
                unlockAppState();
                return;
            }
        }

        Serial.println("[SCD40] loop#" + String(loopCounter) + " phase=not-ready");
        if (lockAppState()) {
            gAppState.sensorConnected = false;
            gAppState.climateValid = false;
            gAppState.sensorError = "SCD40 init failed";
            unlockAppState();
        }
        return;
    }

    if (sensorMutex != nullptr && xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(600)) != pdTRUE) {
        Serial.println("[SCD40] loop#" + String(loopCounter) + " phase=sensor-mutex-timeout");
        return;
    }

    if (!applocks::lockI2c(pdMS_TO_TICKS(400))) {
        Serial.println("[SCD40] loop#" + String(loopCounter) + " phase=i2c-lock-timeout");
        if (sensorMutex != nullptr) {
            xSemaphoreGive(sensorMutex);
        }
        return;
    }

    bool dataReady = false;
    int16_t statusError = scd4x.getDataReadyStatus(dataReady);
    uint16_t ppm = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    int16_t readError = 0;
    bool readOk = false;
    if (statusError == 0 && dataReady) {
        readError = scd4x.readMeasurement(ppm, temperature, humidity);
        readOk = readError == 0;
    }
    applocks::unlockI2c();

    if (sensorMutex != nullptr) {
        xSemaphoreGive(sensorMutex);
    }

    if (statusError == 0 && !dataReady) {
        logLoopStatus("no-data", statusError, dataReady, readOk, ppm, temperature, humidity, false);
        if (lockAppState()) {
            gAppState.sensorConnected = true;
            gAppState.sensorError.clear();
            unlockAppState();
        }
        return;
    }

    bool ppmValid = readOk && ppm >= 250 && ppm <= 10000;
    logLoopStatus("read", readError != 0 ? readError : statusError, dataReady, readOk, ppm, temperature, humidity, ppmValid);

    if (readError != 0) {
        Serial.println("[SCD40] loop#" + String(loopCounter) + " read error msg='" + decodeSensirionError(readError) + "'");
    } else if (statusError != 0) {
        Serial.println("[SCD40] loop#" + String(loopCounter) + " status error msg='" + decodeSensirionError(statusError) + "'");
    }

    if (ppmValid) {
        if (lockAppState()) {
            gAppState.sensorConnected = true;
            gAppState.co2Ppm = ppm;
            gAppState.temperatureC = temperature;
            gAppState.humidityPct = humidity;
            gAppState.climateValid = true;
            gAppState.lastValidPpm = ppm;
            gAppState.sensorError.clear();
            unlockAppState();
        }
        return;
    }

    if (lockAppState()) {
        gAppState.sensorConnected = false;
        gAppState.climateValid = false;
        gAppState.sensorError = statusError != 0 ? String("SCD40 communication error") : String("CO2 read failed");
        if (gAppState.lastValidPpm > 0) {
            gAppState.co2Ppm = gAppState.lastValidPpm;
        }
        unlockAppState();
    }
}

bool calibrateZero(String &error) {
    Serial.println("[SCD40] calibrate: begin");

    if (!sensorReady) {
        Serial.println("[SCD40] calibrate: sensor not initialized");
        error = "SCD40 is not initialized";
        return false;
    }

    if (sensorMutex != nullptr && xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(1500)) != pdTRUE) {
        Serial.println("[SCD40] calibrate: sensor mutex timeout");
        error = "CO2 sensor is busy";
        return false;
    }

    if (!applocks::lockI2c(pdMS_TO_TICKS(1000))) {
        Serial.println("[SCD40] calibrate: I2C lock timeout");
        if (sensorMutex != nullptr) {
            xSemaphoreGive(sensorMutex);
        }
        error = "I2C bus busy";
        return false;
    }

    int16_t stopError = scd4x.stopPeriodicMeasurement();
    delay(500);

    const uint16_t targetCO2 = 400;
    uint16_t frcCorrection = 0;
    int16_t frcError = 0;
    if (stopError == 0) {
        frcError = scd4x.performForcedRecalibration(targetCO2, frcCorrection);
    } else {
        frcError = stopError;
    }

    int16_t startError = scd4x.startPeriodicMeasurement();
    applocks::unlockI2c();

    if (sensorMutex != nullptr) {
        xSemaphoreGive(sensorMutex);
    }

    Serial.println("[SCD40] calibrate: stopError=" + String(stopError) +
                   " frcError=" + String(frcError) +
                   " frcCorrection=" + String(frcCorrection) +
                   " startError=" + String(startError));

    if (stopError != 0) {
        error = "Calibration failed: unable to stop periodic measurement";
        return false;
    }

    if (frcError != 0) {
        error = "Calibration failed: forced recalibration command error";
        return false;
    }

    if (frcCorrection == 0xFFFF) {
        error = "Calibration failed: unstable readings. Keep device in fresh air for 3-5 minutes and retry";
        return false;
    }

    const int16_t correctionPpm = static_cast<int16_t>(frcCorrection) - 0x8000;
    Serial.println("[SCD40] calibrate: success correction=" + String(correctionPpm) + " ppm");

    if (startError != 0) {
        sensorReady = false;
        nextInitAttemptMs = millis() + kInitRetryMs;
        error = "Calibration applied, but sensor restart failed. Reinitialization scheduled";
        return false;
    }

    sensorReady = true;
    error = "";
    return true;
}
}  // namespace sensor
