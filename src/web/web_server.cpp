#include "web/web_server.h"
#include "web/portal_page.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdate.h>
#include <WebServer.h>
#include <WiFi.h>

#include "app_config.h"
#include "app_view_models.h"
#include "app_state.h"
#include "cloud/CloudManager.h"
#include "sensors/Co2Sensor.h"
#include "validation.h"
#include "wifi/ConfigPortal.h"

namespace {
WebServer server(80);
DNSServer dnsServer;
bool dnsStarted = false;
bool restartRequested = false;
unsigned long webMessageExpireAtMs = 0;

struct SaveResult {
    bool saved = false;
    bool hasValidationWarnings = false;
    bool restartRequired = false;
    String validationMessage;
    String validationIssuesJson = "[]";
};

void setWebMessage(const String &message, unsigned long ttlMs = 0) {
    if (lockAppState()) {
        gAppState.webMessage = message;
        unlockAppState();
    }
    webMessageExpireAtMs = ttlMs > 0 ? millis() + ttlMs : 0;
}

void clearWebMessageIfExpired() {
    if (webMessageExpireAtMs == 0) {
        return;
    }

    unsigned long nowMs = millis();
    if (static_cast<long>(nowMs - webMessageExpireAtMs) >= 0) {
        if (lockAppState()) {
            gAppState.webMessage.clear();
            unlockAppState();
        }
        webMessageExpireAtMs = 0;
    }
}

String checkedAttr(bool enabled) {
    return enabled ? " checked" : "";
}

String emptyIfBlank(const String &value) {
    return value.length() > 0 ? value : String("-");
}

String trimmedArg(const char *name) {
    String value = server.arg(name);
    value.trim();
    return value;
}

String escapeHtml(String value) {
    value.replace("&", "&amp;");
    value.replace("<", "&lt;");
    value.replace(">", "&gt;");
    value.replace("\"", "&quot;");
    value.replace("'", "&#39;");
    return value;
}

String jsonEscape(String value) {
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\n", " ");
    value.replace("\r", " ");
    return value;
}

void addValidationIssue(SaveResult &result, const String &field, const String &message) {
    if (result.hasValidationWarnings) {
        result.validationMessage += "; ";
    }
    result.validationMessage += message;
    result.hasValidationWarnings = true;

    String entry = "{\"field\":\"" + jsonEscape(field) + "\",\"message\":\"" + jsonEscape(message) + "\"}";
    if (result.validationIssuesJson == "[]") {
        result.validationIssuesJson = "[" + entry + "]";
    } else {
        result.validationIssuesJson.remove(result.validationIssuesJson.length() - 1);
        result.validationIssuesJson += "," + entry + "]";
    }
}

String lastSyncLabel(unsigned long lastSyncMs, unsigned long nowMs) {
    if (lastSyncMs == 0 || nowMs < lastSyncMs) {
        return String("Last sync: -");
    }

    unsigned long elapsedSeconds = (nowMs - lastSyncMs) / 1000UL;
    if (elapsedSeconds < 60UL) {
        return String("Last sync: ") + String(elapsedSeconds) + "s ago";
    }

    unsigned long elapsedMinutes = elapsedSeconds / 60UL;
    return String("Last sync: ") + String(elapsedMinutes) + "m ago";
}

String statusJson() {
    clearWebMessageIfExpired();

    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();
    unsigned long nowMs = millis();
    String json = "{";
    json += "\"deviceName\":\"" + jsonEscape(config.deviceName) + "\",";
    json += "\"wifiConnected\":" + String(state.wifiConnected ? "true" : "false") + ",";
    json += "\"setupMode\":" + String(state.setupMode ? "true" : "false") + ",";
    json += "\"ipAddress\":\"" + jsonEscape(state.ipAddress) + "\",";
    json += "\"apAddress\":\"" + jsonEscape(state.apAddress) + "\",";
    json += "\"co2Ppm\":" + String(state.co2Ppm) + ",";
    json += "\"temperatureC\":" + String(state.temperatureC, 1) + ",";
    json += "\"humidityPct\":" + String(state.humidityPct, 1) + ",";
    json += "\"climateValid\":" + String(state.climateValid ? "true" : "false") + ",";
    json += "\"sensorConnected\":" + String(state.sensorConnected ? "true" : "false") + ",";
    json += "\"sensorError\":\"" + jsonEscape(state.sensorError) + "\",";
    json += "\"cloudStatus\":\"" + jsonEscape(state.cloudStatus) + "\",";
    json += "\"cloudError\":\"" + jsonEscape(state.cloudError) + "\",";
    json += "\"webMessage\":\"" + jsonEscape(state.webMessage) + "\"";
    json += ",\"sensorReadIntervalMs\":" + String(config.sensorReadIntervalMs);
    json += ",\"thingSpeakIntervalSeconds\":" + String(config.thingSpeakIntervalSeconds);
    json += ",\"thingSpeakLastSyncMs\":" + String(cloudmanager::lastThingSpeakSyncMs());
    json += ",\"customHttpIntervalSeconds\":" + String(config.customHttpIntervalSeconds);
    json += ",\"customHttpLastSyncMs\":" + String(cloudmanager::lastCustomHttpSyncMs());
    json += ",\"nowMs\":" + String(nowMs);
    json += ",\"firmwareVersion\":\"" + jsonEscape(appconfig::kFirmwareVersion) + "\"";
    json += ",\"firmwareBuildDate\":\"" + jsonEscape(appconfig::firmwareBuildDateString()) + "\"";
    json += "}";
    return json;
}

String pageHtml() {
    return webpage::render();
}

String buildHostname() {
    SettingsSnapshot config = getSettingsSnapshot();
    String hostname = config.deviceName;
    hostname.replace(" ", "-");
    hostname.replace("_", "-");
    hostname.toLowerCase();
    if (hostname.length() == 0) {
        hostname = "esp32-co2-sensor";
    }
    return hostname;
}

void handleRoot() {
    server.send(200, "text/html; charset=utf-8", pageHtml());
}

void handleNotFound() {
    if (wifiportal::isSetupMode()) {
        handleRoot();
        return;
    }

    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "Redirecting");
}

void handleApiGet() {
    server.send(200, "application/json", statusJson());
}

SaveResult processSaveRequest() {
    SettingsData previous = settings::get();
    SettingsData updated = previous;
    SaveResult result;

    if (server.hasArg("deviceName")) {
        const String newDeviceName = Validation::trim(server.arg("deviceName"));
        if (newDeviceName.length() == 0) {
            updated.deviceName = appconfig::kDefaultDeviceName;
            addValidationIssue(result, "deviceName", "Device name cannot be empty. Default name applied");
        } else {
            updated.deviceName = newDeviceName;
        }

        if (updated.deviceName != previous.deviceName) {
            result.restartRequired = true;
        }
    }

    const bool wifiForm = server.hasArg("wifiSsid") || server.hasArg("wifiPassword");
    if (wifiForm) {
        // Wi-Fi form intentionally sends full credential pair, including empty password for open networks.
        updated.wifiSsid = trimmedArg("wifiSsid");
        updated.wifiPassword = trimmedArg("wifiPassword");

        if (updated.wifiSsid != previous.wifiSsid || updated.wifiPassword != previous.wifiPassword) {
            result.restartRequired = true;
        }
    }

    if (server.hasArg("sensorReadIntervalMs")) {
        unsigned long parsedInterval = 0;
        if (!Validation::parseUnsignedLongStrict(server.arg("sensorReadIntervalMs"), parsedInterval)) {
            addValidationIssue(result, "sensorReadIntervalMs", "Sensor read interval must be a positive integer");
        } else if (Validation::isValidSensorReadInterval(parsedInterval)) {
            updated.sensorReadIntervalMs = parsedInterval;
        } else {
            addValidationIssue(result, "sensorReadIntervalMs", "Sensor read interval must be >= " + String(appconfig::kSensorReadIntervalMinMs) + " ms");
        }
    }

    const bool thingSpeakForm = server.hasArg("thingSpeakApiKey") || server.hasArg("thingSpeakIntervalSeconds") || server.hasArg("thingSpeakEnabled");
    if (thingSpeakForm) {
        bool enabled = updated.thingSpeakEnabled;
        if (server.hasArg("thingSpeakEnabled")) {
            if (!Validation::parseBoolStrict(server.arg("thingSpeakEnabled"), enabled)) {
                addValidationIssue(result, "thingSpeakEnabled", "ThingSpeak enabled flag must be 0 or 1");
            }
        }
        updated.thingSpeakEnabled = enabled;

        updated.thingSpeakApiKey = trimmedArg("thingSpeakApiKey");

        if (server.hasArg("thingSpeakIntervalSeconds")) {
            unsigned long parsedInterval = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("thingSpeakIntervalSeconds"), parsedInterval)) {
                addValidationIssue(result, "thingSpeakIntervalSeconds", "ThingSpeak interval must be a positive integer");
            } else if (Validation::isValidCloudSendIntervalSeconds(parsedInterval)) {
                updated.thingSpeakIntervalSeconds = parsedInterval;
            } else {
                addValidationIssue(result, "thingSpeakIntervalSeconds", "ThingSpeak interval must be >= 15 seconds");
            }
        }

        if (updated.thingSpeakEnabled && updated.thingSpeakApiKey.length() == 0) {
            addValidationIssue(result, "thingSpeakApiKey", "ThingSpeak is enabled but API key is empty");
        }
    }

    const bool customHttpForm = server.hasArg("customHttpUrlTemplate") || server.hasArg("customHttpBodyTemplate") || server.hasArg("customHttpIntervalSeconds") || server.hasArg("customHttpEnabled");
    if (customHttpForm) {
        bool enabled = updated.customHttpEnabled;
        if (server.hasArg("customHttpEnabled")) {
            if (!Validation::parseBoolStrict(server.arg("customHttpEnabled"), enabled)) {
                addValidationIssue(result, "customHttpEnabled", "Custom HTTP enabled flag must be 0 or 1");
            }
        }
        updated.customHttpEnabled = enabled;

        String method = trimmedArg("customHttpMethod");
        if (method.length() == 0) {
            method = appconfig::kDefaultCustomHttpMethod;
        }
        if (Validation::isValidHttpMethod(method)) {
            updated.customHttpMethod = Validation::normalizeHttpMethod(method);
        } else {
            addValidationIssue(result, "customHttpMethod", "Custom HTTP method must be GET, POST, PUT or PATCH");
        }

        updated.customHttpUrlTemplate = trimmedArg("customHttpUrlTemplate");

        updated.customHttpContentType = trimmedArg("customHttpContentType");
        if (updated.customHttpContentType.length() == 0) {
            updated.customHttpContentType = appconfig::kDefaultCustomHttpContentType;
            addValidationIssue(result, "customHttpContentType", "Custom HTTP content type was empty. Default value applied");
        }

        updated.customHttpBodyTemplate = trimmedArg("customHttpBodyTemplate");

        if (server.hasArg("customHttpIntervalSeconds")) {
            unsigned long parsedInterval = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("customHttpIntervalSeconds"), parsedInterval)) {
                addValidationIssue(result, "customHttpIntervalSeconds", "Custom HTTP interval must be a positive integer");
            } else if (Validation::isValidCloudSendIntervalSeconds(parsedInterval)) {
                updated.customHttpIntervalSeconds = parsedInterval;
            } else {
                addValidationIssue(result, "customHttpIntervalSeconds", "Custom HTTP interval must be >= 15 seconds");
            }
        }

        if (updated.customHttpEnabled) {
            if (updated.customHttpUrlTemplate.length() == 0) {
                addValidationIssue(result, "customHttpUrlTemplate", "Custom HTTP is enabled but URL template is empty");
            } else if (!(updated.customHttpUrlTemplate.startsWith("http://") || updated.customHttpUrlTemplate.startsWith("https://"))) {
                addValidationIssue(result, "customHttpUrlTemplate", "Custom HTTP URL template must start with http:// or https://");
            }
        }
    }

    if (updated.deviceName.length() == 0) {
        updated.deviceName = appconfig::kDefaultDeviceName;
    }

    settings::apply(updated);
    settings::save();
    if (result.hasValidationWarnings) {
        setWebMessage("Saved with validation warnings: " + result.validationMessage, 8000);
    } else {
        setWebMessage("Settings saved", 5000);
    }
    result.saved = true;
    if (result.restartRequired) {
        restartRequested = true;
    }

    return result;
}

void handleSave() {
    processSaveRequest();

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Saved");
}

void handleCalibrate() {
    String error;
    if (sensor::calibrateZero(error)) {
        setWebMessage("Calibration complete. Keep sensor in fresh air (~400 ppm) for 2-3 minutes before calibration.", 8000);
    } else {
        setWebMessage("Calibration failed: " + (error.length() > 0 ? error : String("unknown error")), 10000);
    }

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Calibrate");
}

void handleApiPost() {
    SaveResult result = processSaveRequest();
    String json = "{";
    json += "\"ok\":true";
    json += ",\"saved\":" + String(result.saved ? "true" : "false");
    json += ",\"restartRequired\":" + String(result.restartRequired ? "true" : "false");
    json += ",\"hasValidationWarnings\":" + String(result.hasValidationWarnings ? "true" : "false");
    json += ",\"message\":\"" + jsonEscape(result.hasValidationWarnings ? ("Saved with validation warnings: " + result.validationMessage) : String("Settings saved")) + "\"";
    json += ",\"validationIssues\":" + result.validationIssuesJson;
    json += "}";
    server.send(200, "application/json", json);
}

void handleUpdateDone() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    restartRequested = true;
}

void handleUpdateUpload() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        Update.end(true);
    }
}
}  // namespace

namespace webui {
void begin(bool captivePortalEnabled) {
    if (captivePortalEnabled) {
        dnsServer.start(53, "*", WiFi.softAPIP());
        dnsStarted = true;
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api", HTTP_GET, handleApiGet);
    server.on("/api", HTTP_POST, handleApiPost);
    server.on("/status", HTTP_GET, handleApiGet);
    server.on("/generate_204", HTTP_GET, handleRoot);
    server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
    server.on("/ncsi.txt", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.onNotFound(handleNotFound);
    server.begin();

    if (MDNS.begin(buildHostname().c_str())) {
        MDNS.addService("http", "tcp", 80);
    }
}

void loop() {
    server.handleClient();

    const bool setupMode = wifiportal::isSetupMode();
    if (setupMode && !dnsStarted) {
        dnsServer.start(53, "*", WiFi.softAPIP());
        dnsStarted = true;
    } else if (!setupMode && dnsStarted) {
        dnsServer.stop();
        dnsStarted = false;
    }

    if (dnsStarted) {
        dnsServer.processNextRequest();
    }
    if (restartRequested) {
        delay(500);
        ESP.restart();
    }
}
}  // namespace webui




