#pragma once

#include <Arduino.h>
#include "build_info.h"

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
constexpr unsigned long kSensorReadIntervalMinMs = 5000;
constexpr uint16_t kSensorAltitudeDefaultMeters = 520;
constexpr uint16_t kSensorAltitudeMinMeters = 0;
constexpr uint16_t kSensorAltitudeMaxMeters = 3000;
constexpr unsigned long kDisplayRefreshIntervalMs = 1000;
constexpr unsigned long kDisplaySwitchIntervalMs = 3000;
constexpr unsigned long kDisplaySwitchIntervalMinMs = 1000;
constexpr unsigned long kDisplaySwitchIntervalMaxMs = 60000;
constexpr uint16_t kDndStartMinutesDefault = 23U * 60U;
constexpr uint16_t kDndEndMinutesDefault = 7U * 60U;
constexpr uint8_t kNormalBrightnessLevelDefault = 0xCF;
constexpr uint8_t kDndBrightnessLevelDefault = 51;
constexpr uint8_t kBrightnessLevelMin = 0;
constexpr uint8_t kBrightnessLevelMax = 255;
constexpr uint16_t kMinutesPerDay = 24U * 60U;
constexpr uint8_t kOledBaseContrast = 0xCF;
constexpr uint8_t kBuzzerPin = 4;
constexpr uint16_t kBuzzerFrequencyHzDefault = 2000;
constexpr uint32_t kBuzzerToneDurationMsDefault = 200;
constexpr uint32_t kBuzzerPauseDurationMsDefault = 300;
constexpr unsigned long kBuzzerTestDurationMs = 10000;
constexpr unsigned long kThingSpeakIntervalMs = 30000;
constexpr unsigned long kCustomHttpIntervalMs = 30000;

constexpr char kDefaultDeviceName[] = "ESP32 CO2 Sensor";
constexpr char kWifiApPassword[] = "12345678";
constexpr char kTimeZonePosix[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";
constexpr char kNtpServerPrimary[] = "pool.ntp.org";
constexpr char kNtpServerSecondary[] = "time.nist.gov";
constexpr char kNtpServerTertiary[] = "time.google.com";

constexpr const char* kFirmwareVersion = buildinfo::kFirmwareVersion;

inline String firmwareBuildDateString() {
	return String(buildinfo::kFirmwareBuildDate);
}

constexpr char kDefaultThingSpeakUrl[] = "https://api.thingspeak.com/update";
constexpr char kDefaultCustomHttpMethod[] = "POST";
constexpr char kDefaultCustomHttpContentType[] = "application/json";
constexpr char kDefaultCustomHttpBodyTemplate[] = "{\"ppm\":{ppm}}";
}
