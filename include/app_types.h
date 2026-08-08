#pragma once

#include <Arduino.h>

#include "app_config.h"

struct SettingsData {
    String deviceName = appconfig::kDefaultDeviceName;
    String wifiSsid;
    String wifiPassword;
    uint32_t sensorReadIntervalMs = appconfig::kSensorReadIntervalMs;
    uint16_t sensorAltitudeMeters = appconfig::kSensorAltitudeDefaultMeters;
    uint32_t displaySwitchIntervalMs = appconfig::kDisplaySwitchIntervalMs;
    uint32_t buzzerFrequencyHz = appconfig::kBuzzerFrequencyHzDefault;
    uint32_t buzzerToneDurationMs = appconfig::kBuzzerToneDurationMsDefault;
    uint32_t buzzerPauseDurationMs = appconfig::kBuzzerPauseDurationMsDefault;

    bool thingSpeakEnabled = false;
    String thingSpeakApiKey;
    uint32_t thingSpeakIntervalSeconds = appconfig::kThingSpeakIntervalMs / 1000UL;

    bool customHttpEnabled = false;
    String customHttpUrlTemplate;
    String customHttpMethod = appconfig::kDefaultCustomHttpMethod;
    String customHttpContentType = appconfig::kDefaultCustomHttpContentType;
    String customHttpBodyTemplate = appconfig::kDefaultCustomHttpBodyTemplate;
    uint32_t customHttpIntervalSeconds = appconfig::kCustomHttpIntervalMs / 1000UL;

};
