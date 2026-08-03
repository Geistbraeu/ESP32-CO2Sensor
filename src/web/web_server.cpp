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
#include "wifi/ConfigPortal.h"

namespace {
WebServer server(80);
DNSServer dnsServer;
bool dnsStarted = false;
bool restartRequested = false;
unsigned long webMessageExpireAtMs = 0;

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

unsigned long parseUnsignedLongArg(const String &value, unsigned long fallback) {
    unsigned long parsed = static_cast<unsigned long>(value.toInt());
    return parsed > 0 ? parsed : fallback;
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
    json += "\"sensorConnected\":" + String(state.sensorConnected ? "true" : "false") + ",";
    json += "\"sensorWarmingUp\":" + String(state.sensorWarmingUp ? "true" : "false") + ",";
    json += "\"sensorWarmupRemainingSec\":" + String(state.sensorWarmupRemainingSec) + ",";
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

void handleSave() {
    SettingsData updated = settings::get();
    if (server.hasArg("deviceName")) {
        updated.deviceName = trimmedArg("deviceName");
    }
    const bool wifiForm = server.hasArg("wifiSsid") || server.hasArg("wifiPassword");
    if (wifiForm) {
        // Wi-Fi form intentionally sends full credential pair, including empty password for open networks.
        updated.wifiSsid = trimmedArg("wifiSsid");
        updated.wifiPassword = trimmedArg("wifiPassword");
    }
    if (server.hasArg("sensorReadIntervalMs")) {
        updated.sensorReadIntervalMs = parseUnsignedLongArg(trimmedArg("sensorReadIntervalMs"), appconfig::kSensorReadIntervalMs);
    }

    const bool thingSpeakForm = server.hasArg("thingSpeakApiKey") || server.hasArg("thingSpeakIntervalSeconds") || server.hasArg("thingSpeakEnabled");
    if (thingSpeakForm) {
        updated.thingSpeakEnabled = server.hasArg("thingSpeakEnabled");
        updated.thingSpeakApiKey = trimmedArg("thingSpeakApiKey");
        updated.thingSpeakIntervalSeconds = parseUnsignedLongArg(trimmedArg("thingSpeakIntervalSeconds"), appconfig::kThingSpeakIntervalMs / 1000UL);
        if (updated.thingSpeakIntervalSeconds < 15UL) {
            updated.thingSpeakIntervalSeconds = 15UL;
        }
    }

    const bool customHttpForm = server.hasArg("customHttpUrlTemplate") || server.hasArg("customHttpBodyTemplate") || server.hasArg("customHttpIntervalSeconds") || server.hasArg("customHttpEnabled");
    if (customHttpForm) {
        updated.customHttpEnabled = server.hasArg("customHttpEnabled");
        updated.customHttpMethod = trimmedArg("customHttpMethod");
        updated.customHttpUrlTemplate = trimmedArg("customHttpUrlTemplate");
        updated.customHttpContentType = trimmedArg("customHttpContentType");
        updated.customHttpBodyTemplate = trimmedArg("customHttpBodyTemplate");
        updated.customHttpIntervalSeconds = parseUnsignedLongArg(trimmedArg("customHttpIntervalSeconds"), appconfig::kCustomHttpIntervalMs / 1000UL);
        if (updated.customHttpIntervalSeconds < 15UL) {
            updated.customHttpIntervalSeconds = 15UL;
        }
    }

    if (updated.deviceName.length() == 0) {
        updated.deviceName = appconfig::kDefaultDeviceName;
    }

    settings::apply(updated);
    settings::save();
    setWebMessage("Settings saved", 5000);
    restartRequested = true;

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
    handleSave();
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




