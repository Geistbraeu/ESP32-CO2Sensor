#pragma once

#include <Arduino.h>

#include "app_config.h"

struct SettingsData {
    String deviceName = appconfig::kDefaultDeviceName;
    String wifiSsid;
    String wifiPassword;

    bool thingSpeakEnabled = false;
    String thingSpeakApiKey;
    String thingSpeakUrl = appconfig::kDefaultThingSpeakUrl;

    bool customHttpEnabled = false;
    String customHttpUrlTemplate;
    String customHttpMethod = appconfig::kDefaultCustomHttpMethod;
    String customHttpContentType = appconfig::kDefaultCustomHttpContentType;
    String customHttpBodyTemplate = appconfig::kDefaultCustomHttpBodyTemplate;

    bool hasWifiCredentials() const {
        return wifiSsid.length() > 0 && wifiPassword.length() > 0;
    }
};
