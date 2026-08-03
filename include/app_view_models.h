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
