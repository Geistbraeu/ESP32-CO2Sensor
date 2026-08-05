#pragma once

#include <Arduino.h>
#include <time.h>

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP 0
#endif

#ifndef FW_VERSION
#define FW_VERSION "0.0.0"
#endif

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
constexpr unsigned long kDisplayRefreshIntervalMs = 1000;
constexpr unsigned long kThingSpeakIntervalMs = 30000;
constexpr unsigned long kCustomHttpIntervalMs = 30000;

constexpr char kDefaultDeviceName[] = "ESP32 CO2 Sensor";
constexpr char kWifiApPassword[] = "12345678";

constexpr char kFirmwareVersion[] = FW_VERSION;
constexpr uint32_t kFirmwareBuildTimestamp = static_cast<uint32_t>(BUILD_TIMESTAMP);

inline String firmwareBuildDateString() {
	if (kFirmwareBuildTimestamp == 0U) {
		return String("-");
	}

	time_t buildTime = static_cast<time_t>(kFirmwareBuildTimestamp);
	struct tm timeInfo;
	if (gmtime_r(&buildTime, &timeInfo) == nullptr) {
		return String("-");
	}

	char tail[32];
	if (strftime(tail, sizeof(tail), "%b  %Y %H:%M:%S", &timeInfo) == 0) {
		return String("-");
	}

	return String(timeInfo.tm_mday) + " " + String(tail);
}

constexpr char kDefaultThingSpeakUrl[] = "https://api.thingspeak.com/update";
constexpr char kDefaultCustomHttpMethod[] = "POST";
constexpr char kDefaultCustomHttpContentType[] = "application/json";
constexpr char kDefaultCustomHttpBodyTemplate[] = "{\"ppm\":{ppm}}";
}
