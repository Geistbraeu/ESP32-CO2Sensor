#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct AppState {
    bool wifiConnected = false;
    bool setupMode = false;
    String wifiSsid;
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

    SemaphoreHandle_t dataMutex = nullptr;
    SemaphoreHandle_t settingsMutex = nullptr;
};

extern AppState gAppState;

inline bool lockAppState(TickType_t timeoutTicks = portMAX_DELAY) {
    return gAppState.dataMutex != nullptr && xSemaphoreTake(gAppState.dataMutex, timeoutTicks) == pdTRUE;
}

inline void unlockAppState() {
    if (gAppState.dataMutex != nullptr) {
        xSemaphoreGive(gAppState.dataMutex);
    }
}

inline AppState snapshotAppState(TickType_t timeoutTicks = pdMS_TO_TICKS(20)) {
    AppState snapshot;
    if (lockAppState(timeoutTicks)) {
        snapshot = gAppState;
        unlockAppState();
        snapshot.dataMutex = nullptr;
        snapshot.settingsMutex = nullptr;
    }
    return snapshot;
}
