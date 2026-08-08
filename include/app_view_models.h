#pragma once

#include <Arduino.h>

#include "app_state.h"
#include "settings/Settings.h"

struct RuntimeSnapshot {
    bool wifiConnected = false;
    bool setupMode = false;
    String ipAddress;
    String apAddress;
    bool sensorConnected = false;
    uint16_t co2Ppm = 0;
    uint16_t lastValidPpm = 0;
    float temperatureC = 0.0f;
    float humidityPct = 0.0f;
    bool climateValid = false;
    bool bmpConnected = false;
    float bmpTemperatureC = 0.0f;
    float bmpPressureHpa = 0.0f;
    bool bmpValid = false;
    String sensorError;
    String cloudStatus;
    String cloudError;
    String webMessage;
};

struct SettingsSnapshot {
    String deviceName;
    String wifiSsid;
    String wifiPassword;
    uint32_t sensorReadIntervalMs = appconfig::kSensorReadIntervalMs;
    uint16_t sensorAltitudeMeters = appconfig::kSensorAltitudeDefaultMeters;
    uint32_t displaySwitchIntervalMs = appconfig::kDisplaySwitchIntervalMs;
    bool dndEnabled = false;
    uint16_t dndStartMinutes = appconfig::kDndStartMinutesDefault;
    uint16_t dndEndMinutes = appconfig::kDndEndMinutesDefault;
    uint8_t normalBrightnessLevel = appconfig::kNormalBrightnessLevelDefault;
    uint8_t dndBrightnessLevel = appconfig::kDndBrightnessLevelDefault;
    uint32_t buzzerFrequencyHz = appconfig::kBuzzerFrequencyHzDefault;
    uint32_t buzzerToneDurationMs = appconfig::kBuzzerToneDurationMsDefault;
    uint32_t buzzerPauseDurationMs = appconfig::kBuzzerPauseDurationMsDefault;
    bool thingSpeakEnabled = false;
    String thingSpeakApiKey;
    uint32_t thingSpeakIntervalSeconds = appconfig::kThingSpeakIntervalMs / 1000UL;
    bool customHttpEnabled = false;
    String customHttpUrlTemplate;
    String customHttpMethod;
    String customHttpContentType;
    String customHttpBodyTemplate;
    uint32_t customHttpIntervalSeconds = appconfig::kCustomHttpIntervalMs / 1000UL;
};

inline RuntimeSnapshot getRuntimeSnapshot(TickType_t timeoutTicks = pdMS_TO_TICKS(20)) {
    AppState state = snapshotAppState(timeoutTicks);
    RuntimeSnapshot snapshot;
    snapshot.wifiConnected = state.wifiConnected;
    snapshot.setupMode = state.setupMode;
    snapshot.ipAddress = state.ipAddress;
    snapshot.apAddress = state.apAddress;
    snapshot.sensorConnected = state.sensorConnected;
    snapshot.co2Ppm = state.co2Ppm;
    snapshot.lastValidPpm = state.lastValidPpm;
    snapshot.temperatureC = state.temperatureC;
    snapshot.humidityPct = state.humidityPct;
    snapshot.climateValid = state.climateValid;
    snapshot.bmpConnected = state.bmpConnected;
    snapshot.bmpTemperatureC = state.bmpTemperatureC;
    snapshot.bmpPressureHpa = state.bmpPressureHpa;
    snapshot.bmpValid = state.bmpValid;
    snapshot.sensorError = state.sensorError;
    snapshot.cloudStatus = state.cloudStatus;
    snapshot.cloudError = state.cloudError;
    snapshot.webMessage = state.webMessage;
    return snapshot;
}

inline SettingsSnapshot getSettingsSnapshot() {
    SettingsData config = settings::get();
    SettingsSnapshot snapshot;
    snapshot.deviceName = config.deviceName;
    snapshot.wifiSsid = config.wifiSsid;
    snapshot.wifiPassword = config.wifiPassword;
    snapshot.sensorReadIntervalMs = config.sensorReadIntervalMs;
    snapshot.sensorAltitudeMeters = config.sensorAltitudeMeters;
    snapshot.displaySwitchIntervalMs = config.displaySwitchIntervalMs;
    snapshot.dndEnabled = config.dndEnabled;
    snapshot.dndStartMinutes = config.dndStartMinutes;
    snapshot.dndEndMinutes = config.dndEndMinutes;
    snapshot.normalBrightnessLevel = config.normalBrightnessLevel;
    snapshot.dndBrightnessLevel = config.dndBrightnessLevel;
    snapshot.buzzerFrequencyHz = config.buzzerFrequencyHz;
    snapshot.buzzerToneDurationMs = config.buzzerToneDurationMs;
    snapshot.buzzerPauseDurationMs = config.buzzerPauseDurationMs;
    snapshot.thingSpeakEnabled = config.thingSpeakEnabled;
    snapshot.thingSpeakApiKey = config.thingSpeakApiKey;
    snapshot.thingSpeakIntervalSeconds = config.thingSpeakIntervalSeconds;
    snapshot.customHttpEnabled = config.customHttpEnabled;
    snapshot.customHttpUrlTemplate = config.customHttpUrlTemplate;
    snapshot.customHttpMethod = config.customHttpMethod;
    snapshot.customHttpContentType = config.customHttpContentType;
    snapshot.customHttpBodyTemplate = config.customHttpBodyTemplate;
    snapshot.customHttpIntervalSeconds = config.customHttpIntervalSeconds;
    return snapshot;
}
