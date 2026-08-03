#include "settings/Settings.h"

#include <Preferences.h>

#include "app_state.h"

namespace {
Preferences preferences;
SettingsData currentSettings;
bool started = false;

constexpr const char *kNamespace = "co2cfg";
constexpr const char *kKeyDeviceName = "deviceName";
constexpr const char *kKeyWifiSsid = "wifiSsid";
constexpr const char *kKeyWifiPassword = "wifiPass";
constexpr const char *kKeyThingSpeakEnabled = "tsEnable";
constexpr const char *kKeyThingSpeakApiKey = "tsKey";
constexpr const char *kKeyThingSpeakUrl = "tsUrl";
constexpr const char *kKeyCustomHttpEnabled = "httpEnable";
constexpr const char *kKeyCustomHttpUrl = "httpUrl";
constexpr const char *kKeyCustomHttpMethod = "httpMethod";
constexpr const char *kKeyCustomHttpContentType = "httpType";
constexpr const char *kKeyCustomHttpBody = "httpBody";

String readString(const char *key, const String &fallback) {
    return preferences.getString(key, fallback.c_str());
}
}  // namespace

namespace settings {
void begin() {
    if (gAppState.settingsMutex == nullptr) {
        gAppState.settingsMutex = xSemaphoreCreateMutex();
    }

    if (!started) {
        started = load();
        if (!started) {
            currentSettings = SettingsData();
            started = true;
        }
    }
}

SettingsData get() {
    if (gAppState.settingsMutex != nullptr && xSemaphoreTake(gAppState.settingsMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        SettingsData snapshot = currentSettings;
        xSemaphoreGive(gAppState.settingsMutex);
        return snapshot;
    }

    return currentSettings;
}

void apply(const SettingsData &value) {
    if (gAppState.settingsMutex != nullptr && xSemaphoreTake(gAppState.settingsMutex, portMAX_DELAY) == pdTRUE) {
        currentSettings = value;
        xSemaphoreGive(gAppState.settingsMutex);
        return;
    }

    currentSettings = value;
}

bool load() {
    if (!preferences.begin(kNamespace, true)) {
        currentSettings = SettingsData();
        return false;
    }

    currentSettings.deviceName = readString(kKeyDeviceName, appconfig::kDefaultDeviceName);
    currentSettings.wifiSsid = readString(kKeyWifiSsid, "");
    currentSettings.wifiPassword = readString(kKeyWifiPassword, "");
    currentSettings.thingSpeakEnabled = preferences.getBool(kKeyThingSpeakEnabled, false);
    currentSettings.thingSpeakApiKey = readString(kKeyThingSpeakApiKey, "");
    currentSettings.thingSpeakUrl = readString(kKeyThingSpeakUrl, appconfig::kDefaultThingSpeakUrl);
    currentSettings.customHttpEnabled = preferences.getBool(kKeyCustomHttpEnabled, false);
    currentSettings.customHttpUrlTemplate = readString(kKeyCustomHttpUrl, "");
    currentSettings.customHttpMethod = readString(kKeyCustomHttpMethod, appconfig::kDefaultCustomHttpMethod);
    currentSettings.customHttpContentType = readString(kKeyCustomHttpContentType, appconfig::kDefaultCustomHttpContentType);
    currentSettings.customHttpBodyTemplate = readString(kKeyCustomHttpBody, appconfig::kDefaultCustomHttpBodyTemplate);

    preferences.end();

    if (currentSettings.deviceName.length() == 0) {
        currentSettings.deviceName = appconfig::kDefaultDeviceName;
    }

    return true;
}

bool save() {
    SettingsData snapshot;
    if (gAppState.settingsMutex != nullptr && xSemaphoreTake(gAppState.settingsMutex, portMAX_DELAY) == pdTRUE) {
        snapshot = currentSettings;
        xSemaphoreGive(gAppState.settingsMutex);
    } else {
        snapshot = currentSettings;
    }

    if (!preferences.begin(kNamespace, false)) {
        return false;
    }

    preferences.putString(kKeyDeviceName, snapshot.deviceName);
    preferences.putString(kKeyWifiSsid, snapshot.wifiSsid);
    preferences.putString(kKeyWifiPassword, snapshot.wifiPassword);
    preferences.putBool(kKeyThingSpeakEnabled, snapshot.thingSpeakEnabled);
    preferences.putString(kKeyThingSpeakApiKey, snapshot.thingSpeakApiKey);
    preferences.putString(kKeyThingSpeakUrl, snapshot.thingSpeakUrl);
    preferences.putBool(kKeyCustomHttpEnabled, snapshot.customHttpEnabled);
    preferences.putString(kKeyCustomHttpUrl, snapshot.customHttpUrlTemplate);
    preferences.putString(kKeyCustomHttpMethod, snapshot.customHttpMethod);
    preferences.putString(kKeyCustomHttpContentType, snapshot.customHttpContentType);
    preferences.putString(kKeyCustomHttpBody, snapshot.customHttpBodyTemplate);

    preferences.end();
    return true;
}
}  // namespace settings
