#include "cloud/CloudManager.h"

#include <WiFi.h>

#include "app_config.h"
#include "app_state.h"
#include "cloud/CustomHttpProvider.h"
#include "cloud/ThingSpeakProvider.h"
#include "settings/Settings.h"

namespace {
unsigned long lastThingSpeakSendMs = 0;
unsigned long lastCustomHttpSendMs = 0;
}

namespace cloudmanager {
void begin() {
    lastThingSpeakSendMs = 0;
    lastCustomHttpSendMs = 0;
}

void loop() {
    AppState state = snapshotAppState();
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!state.sensorConnected || state.co2Ppm < 250 || state.co2Ppm > 10000) {
        return;
    }

    unsigned long now = millis();
    SettingsData config = settings::get();
    unsigned long thingSpeakIntervalMs = config.thingSpeakIntervalSeconds > 0 ? config.thingSpeakIntervalSeconds * 1000UL : appconfig::kThingSpeakIntervalMs;
    unsigned long customHttpIntervalMs = config.customHttpIntervalSeconds > 0 ? config.customHttpIntervalSeconds * 1000UL : appconfig::kCustomHttpIntervalMs;

    if (config.thingSpeakEnabled && now - lastThingSpeakSendMs >= thingSpeakIntervalMs) {
        if (cloudthingspeak::send()) {
            lastThingSpeakSendMs = now;
        }
    }

    if (config.customHttpEnabled && now - lastCustomHttpSendMs >= customHttpIntervalMs) {
        if (cloudcustomhttp::send()) {
            lastCustomHttpSendMs = now;
        }
    }
}

unsigned long lastThingSpeakSyncMs() {
    return lastThingSpeakSendMs;
}

unsigned long lastCustomHttpSyncMs() {
    return lastCustomHttpSendMs;
}
}  // namespace cloudmanager
