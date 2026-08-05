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
constexpr const char *kKeySensorReadInterval = "sensorReadMs";
constexpr const char *kKeySensorAltitude = "sensorAlt";
constexpr const char *kKeyThingSpeakEnabled = "tsEnable";
constexpr const char *kKeyThingSpeakApiKey = "tsKey";
constexpr const char *kKeyThingSpeakInterval = "tsInterval";
constexpr const char *kKeyCustomHttpEnabled = "httpEnable";
constexpr const char *kKeyCustomHttpUrl = "httpUrl";
constexpr const char *kKeyCustomHttpMethod = "httpMethod";
constexpr const char *kKeyCustomHttpContentType = "httpType";
constexpr const char *kKeyCustomHttpBody = "httpBody";
constexpr const char *kKeyCustomHttpInterval = "httpInterval";

String readString(const char *key, const String &fallback) {
    return preferences.getString(key, fallback.c_str());
}

unsigned long normalizeSensorReadInterval(unsigned long value) {
    if (value < appconfig::kSensorReadIntervalMinMs) {
        return appconfig::kSensorReadIntervalMinMs;
    }
    return value;
}

uint16_t normalizeSensorAltitude(unsigned long value) {
    if (value < appconfig::kSensorAltitudeMinMeters) {
        return appconfig::kSensorAltitudeMinMeters;
    }
    if (value > appconfig::kSensorAltitudeMaxMeters) {
        return appconfig::kSensorAltitudeMaxMeters;
    }
    return static_cast<uint16_t>(value);
}

unsigned long normalizeThingSpeakIntervalSeconds(unsigned long value) {
    if (value < 15UL) {
        return 15UL;
    }
    return value;
}

unsigned long normalizeCustomHttpIntervalSeconds(unsigned long value) {
    if (value < 15UL) {
        return 15UL;
    }
    return value;
}

String normalizeCustomHttpMethod(const String &value) {
    String method = value;
    method.trim();
    method.toUpperCase();
    if (method == "GET" || method == "POST" || method == "PUT" || method == "PATCH") {
        return method;
    }
    return String(appconfig::kDefaultCustomHttpMethod);
}

String normalizeContentType(const String &value) {
    String contentType = value;
    contentType.trim();
    if (contentType.length() == 0) {
        return String(appconfig::kDefaultCustomHttpContentType);
    }
    return contentType;
}

void normalizeSettings(SettingsData &value) {
    value.sensorReadIntervalMs = normalizeSensorReadInterval(value.sensorReadIntervalMs);
    value.sensorAltitudeMeters = normalizeSensorAltitude(value.sensorAltitudeMeters);
    value.thingSpeakIntervalSeconds = normalizeThingSpeakIntervalSeconds(value.thingSpeakIntervalSeconds);
    value.customHttpIntervalSeconds = normalizeCustomHttpIntervalSeconds(value.customHttpIntervalSeconds);
    value.customHttpMethod = normalizeCustomHttpMethod(value.customHttpMethod);
    value.customHttpContentType = normalizeContentType(value.customHttpContentType);

    if (value.deviceName.length() == 0) {
        value.deviceName = appconfig::kDefaultDeviceName;
    }
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
    SettingsData normalized = value;
    normalizeSettings(normalized);

    if (gAppState.settingsMutex != nullptr && xSemaphoreTake(gAppState.settingsMutex, portMAX_DELAY) == pdTRUE) {
        currentSettings = normalized;
        xSemaphoreGive(gAppState.settingsMutex);
        return;
    }

    currentSettings = normalized;
}

bool load() {
    if (!preferences.begin(kNamespace, true)) {
        currentSettings = SettingsData();
        return false;
    }

    currentSettings.deviceName = readString(kKeyDeviceName, appconfig::kDefaultDeviceName);
    currentSettings.wifiSsid = readString(kKeyWifiSsid, "");
    currentSettings.wifiPassword = readString(kKeyWifiPassword, "");
    currentSettings.sensorReadIntervalMs = preferences.getULong(kKeySensorReadInterval, appconfig::kSensorReadIntervalMs);
    currentSettings.sensorAltitudeMeters = static_cast<uint16_t>(preferences.getULong(kKeySensorAltitude, appconfig::kSensorAltitudeDefaultMeters));
    currentSettings.thingSpeakEnabled = preferences.getBool(kKeyThingSpeakEnabled, false);
    currentSettings.thingSpeakApiKey = readString(kKeyThingSpeakApiKey, "");
    currentSettings.thingSpeakIntervalSeconds = preferences.getULong(kKeyThingSpeakInterval, appconfig::kThingSpeakIntervalMs / 1000UL);
    currentSettings.customHttpEnabled = preferences.getBool(kKeyCustomHttpEnabled, false);
    currentSettings.customHttpUrlTemplate = readString(kKeyCustomHttpUrl, "");
    currentSettings.customHttpMethod = readString(kKeyCustomHttpMethod, appconfig::kDefaultCustomHttpMethod);
    currentSettings.customHttpContentType = readString(kKeyCustomHttpContentType, appconfig::kDefaultCustomHttpContentType);
    currentSettings.customHttpBodyTemplate = readString(kKeyCustomHttpBody, appconfig::kDefaultCustomHttpBodyTemplate);
    currentSettings.customHttpIntervalSeconds = preferences.getULong(kKeyCustomHttpInterval, appconfig::kCustomHttpIntervalMs / 1000UL);

    preferences.end();

    // Legacy builds used 180000 ms warm-up timing; treat it as stale polling interval.
    if (currentSettings.sensorReadIntervalMs == 0 || currentSettings.sensorReadIntervalMs == 180000UL) {
        currentSettings.sensorReadIntervalMs = appconfig::kSensorReadIntervalMs;
    }

    normalizeSettings(currentSettings);

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

    normalizeSettings(snapshot);

    preferences.putString(kKeyDeviceName, snapshot.deviceName);
    preferences.putString(kKeyWifiSsid, snapshot.wifiSsid);
    preferences.putString(kKeyWifiPassword, snapshot.wifiPassword);
    preferences.putULong(kKeySensorReadInterval, snapshot.sensorReadIntervalMs);
    preferences.putULong(kKeySensorAltitude, snapshot.sensorAltitudeMeters);
    preferences.putBool(kKeyThingSpeakEnabled, snapshot.thingSpeakEnabled);
    preferences.putString(kKeyThingSpeakApiKey, snapshot.thingSpeakApiKey);
    preferences.putULong(kKeyThingSpeakInterval, snapshot.thingSpeakIntervalSeconds);
    preferences.putBool(kKeyCustomHttpEnabled, snapshot.customHttpEnabled);
    preferences.putString(kKeyCustomHttpUrl, snapshot.customHttpUrlTemplate);
    preferences.putString(kKeyCustomHttpMethod, snapshot.customHttpMethod);
    preferences.putString(kKeyCustomHttpContentType, snapshot.customHttpContentType);
    preferences.putString(kKeyCustomHttpBody, snapshot.customHttpBodyTemplate);
    preferences.putULong(kKeyCustomHttpInterval, snapshot.customHttpIntervalSeconds);

    preferences.end();
    return true;
}
}  // namespace settings
