#include "cloud/CustomHttpProvider.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "app_config.h"
#include "app_view_models.h"

namespace {
String replacePpmToken(String value) {
    RuntimeSnapshot state = getRuntimeSnapshot();
    value.replace("{ppm}", String(state.co2Ppm));
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
    if (!config.customHttpEnabled || config.customHttpUrlTemplate.length() == 0) {
        return false;
    }

    const String method = config.customHttpMethod;
    String url = replacePpmToken(config.customHttpUrlTemplate);
    String body = replacePpmToken(config.customHttpBodyTemplate);

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
