#include "sensors/Bmp280Sensor.h"

#include <Adafruit_BMP280.h>
#include <Wire.h>

#include "app_config.h"
#include "app_i2c_lock.h"
#include "app_state.h"

namespace {
Adafruit_BMP280 bmp280;
constexpr unsigned long kBmpInitRetryMs = 2000;
constexpr uint8_t kBmp280PrimaryI2cAddress = 0x76;
constexpr uint8_t kBmp280SecondaryI2cAddress = 0x77;
}

void Bmp280Sensor::begin(unsigned long startupDelayMs) {
    ready_ = false;
    nextInitAttemptMs_ = millis() + startupDelayMs;
    Serial.println("[BMP280] startup delay before first init: " + String(startupDelayMs) + " ms");

    if (lockAppState()) {
        gAppState.bmpConnected = false;
        gAppState.bmpValid = false;
        unlockAppState();
    }
}

void Bmp280Sensor::loop() {
    unsigned long nowMs = millis();

    if (!ready_ && static_cast<long>(nowMs - nextInitAttemptMs_) >= 0) {
        tryInit("retry");
    }

    float bmpTemperatureC = 0.0f;
    float bmpPressureHpa = 0.0f;
    bool bmpValid = readSample(bmpTemperatureC, bmpPressureHpa);

    if (lockAppState()) {
        gAppState.bmpConnected = ready_;
        gAppState.bmpValid = bmpValid;
        if (bmpValid) {
            gAppState.bmpTemperatureC = bmpTemperatureC;
            gAppState.bmpPressureHpa = bmpPressureHpa;
        }
        unlockAppState();
    }
}

bool Bmp280Sensor::ensureWireInitialized() {
    if (wireInitialized_) {
        return true;
    }

    Wire.begin(appconfig::kI2CSdaPin, appconfig::kI2CSclPin);
    Wire.setClock(100000);
    wireInitialized_ = true;
    return true;
}

bool Bmp280Sensor::tryInit(const char* reason) {
    Serial.println(String("[BMP280] init: begin reason=") + reason);

    if (!applocks::lockI2c(pdMS_TO_TICKS(500))) {
        Serial.println("[BMP280] init: failed, I2C lock timeout");
        ready_ = false;
        nextInitAttemptMs_ = millis() + kBmpInitRetryMs;
        return false;
    }

    ensureWireInitialized();

    bool started = bmp280.begin(kBmp280PrimaryI2cAddress);
    uint8_t activeAddress = kBmp280PrimaryI2cAddress;
    if (!started) {
        started = bmp280.begin(kBmp280SecondaryI2cAddress);
        activeAddress = kBmp280SecondaryI2cAddress;
    }

    if (started) {
        bmp280.setSampling(
            Adafruit_BMP280::MODE_NORMAL,
            Adafruit_BMP280::SAMPLING_X2,
            Adafruit_BMP280::SAMPLING_X16,
            Adafruit_BMP280::FILTER_X16,
            Adafruit_BMP280::STANDBY_MS_500
        );
    }

    applocks::unlockI2c();

    ready_ = started;
    if (!ready_) {
        nextInitAttemptMs_ = millis() + kBmpInitRetryMs;
        Serial.println("[BMP280] init: failed on 0x76 and 0x77");
        return false;
    }

    Serial.println("[BMP280] init: success on address 0x" + String(activeAddress, HEX));
    return true;
}

bool Bmp280Sensor::readSample(float& temperatureC, float& pressureHpa) {
    if (!ready_) {
        return false;
    }

    if (!applocks::lockI2c(pdMS_TO_TICKS(300))) {
        Serial.println("[BMP280] read: I2C lock timeout");
        return false;
    }

    const float temperature = bmp280.readTemperature();
    const float pressurePa = bmp280.readPressure();
    applocks::unlockI2c();

    if (isnan(temperature) || isnan(pressurePa) || pressurePa < 30000.0f || pressurePa > 120000.0f) {
        Serial.println("[BMP280] read: invalid sample");
        ready_ = false;
        nextInitAttemptMs_ = millis() + kBmpInitRetryMs;
        return false;
    }

    temperatureC = temperature;
    pressureHpa = pressurePa / 100.0f;
    return true;
}
