#pragma once

#include <Arduino.h>

namespace appconfig {
constexpr uint8_t kI2CSdaPin = 21;
constexpr uint8_t kI2CSclPin = 22;
constexpr uint8_t kOledI2cAddress = 0x3C;
constexpr uint16_t kOledWidth = 128;
constexpr uint16_t kOledHeight = 64;

constexpr uint8_t kCo2SensorRxPin = 16;
constexpr uint8_t kCo2SensorTxPin = 17;
constexpr uint32_t kCo2SensorBaud = 9600;

constexpr unsigned long kSensorReadIntervalMs = 5000;
constexpr unsigned long kDisplayRefreshIntervalMs = 1000;
constexpr unsigned long kThingSpeakIntervalMs = 30000;
constexpr unsigned long kCustomHttpIntervalMs = 30000;

constexpr char kDefaultDeviceName[] = "ESP32 CO2 Sensor";
constexpr char kWifiApPassword[] = "12345678";

constexpr char kFirmwareVersion[] = "1.0.0";
constexpr char kFirmwareBuildDate[] = __DATE__ " " __TIME__;

constexpr char kDefaultThingSpeakUrl[] = "http://api.thingspeak.com/update";
constexpr char kDefaultCustomHttpMethod[] = "POST";
constexpr char kDefaultCustomHttpContentType[] = "application/json";
constexpr char kDefaultCustomHttpBodyTemplate[] = "{\"ppm\":{ppm}}";
}
