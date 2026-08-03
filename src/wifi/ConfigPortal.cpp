#include "wifi/ConfigPortal.h"

#include <WiFi.h>

#include "app_config.h"
#include "app_state.h"
#include "app_view_models.h"

static bool setupMode = false;
static unsigned long lastReconnectAttemptMs = 0;

static String buildApName(const String &deviceName) {
    String result = deviceName;
    result.replace(" ", "-");
    result += "-Setup";
    return result;
}

static void startAccessPoint(const String &deviceName) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(buildApName(deviceName).c_str(), appconfig::kWifiApPassword);
    setupMode = true;
    if (lockAppState()) {
        gAppState.setupMode = true;
        gAppState.apAddress = WiFi.softAPIP().toString();
        unlockAppState();
    }
}

namespace wifiportal {
void begin() {
    SettingsSnapshot config = getSettingsSnapshot();
    if (lockAppState()) {
        gAppState.wifiSsid = config.wifiSsid;
        unlockAppState();
    }

    WiFi.mode(WIFI_STA);

    if (config.wifiSsid.length() > 0 && config.wifiPassword.length() > 0) {
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
    if (WiFi.status() == WL_CONNECTED) {
        if (lockAppState()) {
            if (!gAppState.wifiConnected) {
                gAppState.wifiConnected = true;
                gAppState.ipAddress = WiFi.localIP().toString();
            }
            unlockAppState();
        }
        return;
    }

    if (lockAppState()) {
        if (gAppState.wifiConnected) {
            gAppState.wifiConnected = false;
            gAppState.ipAddress.clear();
        }
        unlockAppState();
    }

    unsigned long now = millis();
    if (now - lastReconnectAttemptMs > 15000UL) {
        lastReconnectAttemptMs = now;
        WiFi.reconnect();
    }
}

bool isSetupMode() {
    return setupMode;
}
}  // namespace wifiportal
