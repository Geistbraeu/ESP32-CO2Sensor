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
    if (WiFi.status() != WL_CONNECTED || state.lastValidPpm == 0) {
        return;
    }

    unsigned long now = millis();
    SettingsData config = settings::get();

    if (config.thingSpeakEnabled && now - lastThingSpeakSendMs >= appconfig::kThingSpeakIntervalMs) {
        lastThingSpeakSendMs = now;
        cloudthingspeak::send();
    }

    if (config.customHttpEnabled && now - lastCustomHttpSendMs >= appconfig::kCustomHttpIntervalMs) {
        lastCustomHttpSendMs = now;
        cloudcustomhttp::send();
    }
}
}  // namespace cloudmanager
