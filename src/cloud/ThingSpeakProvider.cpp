#include "cloud/ThingSpeakProvider.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "app_view_models.h"

namespace {
bool beginHttpClient(HTTPClient &http, const String &url, WiFiClient &client, WiFiClientSecure &secureClient) {
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        return http.begin(secureClient, url);
    }
    return http.begin(client, url);
}
}  // namespace

namespace cloudthingspeak {
bool send() {
    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();
    if (!config.thingSpeakEnabled || config.thingSpeakApiKey.length() == 0) {
        return false;
    }

    String url = config.thingSpeakUrl;
    if (url.indexOf('?') < 0) {
        url += "?";
    } else if (!url.endsWith("&") && !url.endsWith("?")) {
        url += "&";
    }
    url += "api_key=" + config.thingSpeakApiKey + "&field1=" + String(state.co2Ppm);

    HTTPClient http;
    WiFiClient client;
    WiFiClientSecure secureClient;
    if (!beginHttpClient(http, url, client, secureClient)) {
        if (lockAppState()) {
            gAppState.cloudError = "ThingSpeak begin failed";
            unlockAppState();
        }
        return false;
    }

    int code = http.GET();
    http.end();

    if (code > 0) {
        if (lockAppState()) {
            gAppState.cloudStatus = "ThingSpeak: " + String(code);
            gAppState.cloudError.clear();
            unlockAppState();
        }
        return true;
    }

    if (lockAppState()) {
        gAppState.cloudError = "ThingSpeak request failed";
        unlockAppState();
    }
    return false;
}
}  // namespace cloudthingspeak
