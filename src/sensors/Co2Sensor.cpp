#include "sensors/Co2Sensor.h"

#include <HardwareSerial.h>

#include "app_config.h"
#include "app_state.h"

namespace {
HardwareSerial co2Serial(2);
unsigned long lastReadMs = 0;

uint8_t checksumForCommand(const uint8_t *frame) {
    uint8_t sum = 0;
    for (int index = 1; index < 8; ++index) {
        sum += frame[index];
    }
    return static_cast<uint8_t>(0xFF - sum + 1);
}

bool readFrame(uint16_t &ppm, String &error) {
    const uint8_t command[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t request[9];
    memcpy(request, command, sizeof(request));
    request[8] = checksumForCommand(request);

    while (co2Serial.available() > 0) {
        co2Serial.read();
    }

    co2Serial.write(request, sizeof(request));
    co2Serial.flush();

    uint8_t response[9] = {0};
    const unsigned long start = millis();
    size_t received = 0;
    while (millis() - start < 500UL && received < sizeof(response)) {
        if (co2Serial.available() > 0) {
            response[received++] = static_cast<uint8_t>(co2Serial.read());
        }
    }

    if (received != sizeof(response)) {
        error = "CO2 sensor timeout";
        return false;
    }

    if (response[0] != 0xFF || response[1] != 0x86) {
        error = "CO2 sensor frame invalid";
        return false;
    }

    uint8_t expectedChecksum = 0;
    for (int index = 1; index < 8; ++index) {
        expectedChecksum += response[index];
    }
    expectedChecksum = static_cast<uint8_t>(0xFF - expectedChecksum + 1);
    if (response[8] != expectedChecksum) {
        error = "CO2 sensor checksum mismatch";
        return false;
    }

    ppm = static_cast<uint16_t>((static_cast<uint16_t>(response[2]) << 8) | response[3]);
    if (ppm == 0 || ppm > 10000) {
        error = "CO2 value out of range";
        return false;
    }

    return true;
}
}  // namespace

namespace sensor {
void begin() {
    co2Serial.begin(appconfig::kCo2SensorBaud, SERIAL_8N1, appconfig::kCo2SensorRxPin, appconfig::kCo2SensorTxPin);
}

void loop() {
    unsigned long now = millis();
    if (now - lastReadMs < appconfig::kSensorReadIntervalMs) {
        return;
    }
    lastReadMs = now;

    uint16_t ppm = 0;
    String error;
    if (readFrame(ppm, error)) {
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
        gAppState.sensorError = error;
        if (gAppState.lastValidPpm > 0) {
            gAppState.co2Ppm = gAppState.lastValidPpm;
        }
        unlockAppState();
    }
}
}  // namespace sensor
