#include "cloud/CustomHttpProvider.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "app_config.h"
#include "app_view_models.h"

namespace {
String replaceTemplateTokens(String value) {
    RuntimeSnapshot state = getRuntimeSnapshot();
    value.replace("{ppm}", String(state.lastValidPpm));
    value.replace("{temp}", String(state.temperatureC, 1));
    value.replace("{hum}", String(state.humidityPct, 1));
    return value;
}

bool beginHttpClient(HTTPClient &http, const String &url, WiFiClient &client, WiFiClientSecure &secureClient) {
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        return http.begin(secureClient, url);
    }
    return http.begin(client, url);
}
}  // namespace

namespace cloudcustomhttp {
bool send() {
    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();
    if (!config.customHttpEnabled || config.customHttpUrlTemplate.length() == 0) {
        return false;
    }

    if (!state.climateValid || (state.temperatureC == 0.0f && state.humidityPct == 0.0f)) {
        return false;
    }

    const String method = config.customHttpMethod;
    String url = replaceTemplateTokens(config.customHttpUrlTemplate);
    String body = replaceTemplateTokens(config.customHttpBodyTemplate);

    HTTPClient http;
    WiFiClient client;
    WiFiClientSecure secureClient;
    if (!beginHttpClient(http, url, client, secureClient)) {
        if (lockAppState()) {
            gAppState.cloudError = "Custom HTTP begin failed";
            unlockAppState();
        }
        return false;
    }

    int code = -1;
    String upperMethod = method;
    upperMethod.toUpperCase();

    if (upperMethod == "GET") {
        code = http.GET();
    } else if (upperMethod == "PUT") {
        http.addHeader("Content-Type", config.customHttpContentType);
        code = http.PUT(body);
    } else if (upperMethod == "PATCH") {
        http.addHeader("Content-Type", config.customHttpContentType);
        code = http.sendRequest("PATCH", body);
    } else {
        http.addHeader("Content-Type", config.customHttpContentType);
        code = http.POST(body);
    }

    http.end();

    if (code > 0) {
        if (lockAppState()) {
            gAppState.cloudStatus = "Custom HTTP: " + String(code);
            gAppState.cloudError.clear();
            unlockAppState();
        }
        return true;
    }

    if (lockAppState()) {
        gAppState.cloudError = "Custom HTTP request failed";
        unlockAppState();
    }
    return false;
}
}  // namespace cloudcustomhttp
