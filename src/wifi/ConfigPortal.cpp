#include "wifi/ConfigPortal.h"

#include <WiFi.h>

#include "app_config.h"
#include "app_state.h"
#include "app_view_models.h"

static bool setupMode = false;
static bool offlineMode = false;
static unsigned long apStartedMs = 0;

constexpr unsigned long kSetupApWindowMs = 5UL * 60UL * 1000UL;

static String buildApName(const String &deviceName) {
    String result = deviceName;
    result.replace(" ", "-");
    result += "-Setup";
    return result;
}

static void startAccessPoint(const String &deviceName) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(buildApName(deviceName).c_str());
    setupMode = true;
    offlineMode = false;
    apStartedMs = millis();
    if (lockAppState()) {
        gAppState.setupMode = true;
        gAppState.wifiConnected = false;
        gAppState.ipAddress.clear();
        gAppState.apAddress = WiFi.softAPIP().toString();
        unlockAppState();
    }
}

static void enterOfflineMode() {
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    setupMode = false;
    offlineMode = true;

    if (lockAppState()) {
        gAppState.setupMode = false;
        gAppState.wifiConnected = false;
        gAppState.ipAddress.clear();
        gAppState.apAddress.clear();
        gAppState.webMessage = "Offline mode: Wi-Fi disabled";
        unlockAppState();
    }
}

namespace wifiportal {
void begin() {
    SettingsSnapshot config = getSettingsSnapshot();
    setupMode = false;
    offlineMode = false;
    apStartedMs = 0;

    if (lockAppState()) {
        gAppState.wifiSsid = config.wifiSsid;
        unlockAppState();
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, false);
    WiFi.setHostname(config.deviceName.c_str());

    if (config.wifiSsid.length() > 0) {
        WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000UL) {
            delay(250);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (lockAppState()) {
            gAppState.wifiConnected = true;
            gAppState.setupMode = false;
            gAppState.ipAddress = WiFi.localIP().toString();
            gAppState.apAddress.clear();
            unlockAppState();
        }
        return;
    }

    startAccessPoint(config.deviceName);
    if (lockAppState()) {
        gAppState.wifiConnected = false;
        gAppState.ipAddress.clear();
        unlockAppState();
    }
}

void loop() {
    if (offlineMode) {
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (lockAppState()) {
            if (!gAppState.wifiConnected) {
                gAppState.wifiConnected = true;
                gAppState.ipAddress = WiFi.localIP().toString();
            }
            if (gAppState.setupMode) {
                gAppState.setupMode = false;
                gAppState.apAddress.clear();
            }
            unlockAppState();
        }
        return;
    }

    if (setupMode) {
        unsigned long now = millis();
        if (now - apStartedMs >= kSetupApWindowMs) {
            enterOfflineMode();
        }
        return;
    }

    enterOfflineMode();
}

bool isSetupMode() {
    return setupMode;
}
}  // namespace wifiportal
