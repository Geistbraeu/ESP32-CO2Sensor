#include "web/web_server.h"
#include "web/portal_page.h"

#include <time.h>

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
bool networkReinitRequested = false;
bool mdnsReinitRequested = false;
unsigned long webMessageExpireAtMs = 0;
bool buzzerTestActive = false;
bool buzzerToneActive = false;
unsigned long buzzerTestStartedAtMs = 0;
unsigned long buzzerTransitionAtMs = 0;

void stopBuzzerTest() {
    const bool wasToneActive = buzzerToneActive;
    buzzerTestActive = false;
    buzzerToneActive = false;
    buzzerTestStartedAtMs = 0;
    buzzerTransitionAtMs = 0;
    if (wasToneActive) {
        noTone(appconfig::kBuzzerPin);
    } else {
        digitalWrite(appconfig::kBuzzerPin, LOW);
    }
    Serial.println("[Buzzer] stopped");
}

void updateBuzzerTestState() {
    if (!buzzerTestActive) {
        return;
    }

    SettingsData config = settings::get();
    const unsigned long nowMs = millis();
    if (buzzerTestStartedAtMs == 0) {
        buzzerTestStartedAtMs = nowMs;
    }

    if (nowMs - buzzerTestStartedAtMs >= appconfig::kBuzzerTestDurationMs) {
        stopBuzzerTest();
        return;
    }

    if (buzzerTransitionAtMs == 0) {
        buzzerToneActive = true;
        buzzerTransitionAtMs = nowMs;
        tone(appconfig::kBuzzerPin, static_cast<unsigned int>(config.buzzerFrequencyHz));
        Serial.printf("[Buzzer] tone on, freq=%u Hz\n", static_cast<unsigned int>(config.buzzerFrequencyHz));
        return;
    }

    if (buzzerToneActive) {
        if (nowMs - buzzerTransitionAtMs >= config.buzzerToneDurationMs) {
            noTone(appconfig::kBuzzerPin);
            buzzerToneActive = false;
            buzzerTransitionAtMs = nowMs;
            Serial.println("[Buzzer] tone off");
        }
    } else if (nowMs - buzzerTransitionAtMs >= config.buzzerPauseDurationMs) {
        tone(appconfig::kBuzzerPin, static_cast<unsigned int>(config.buzzerFrequencyHz));
        buzzerToneActive = true;
        buzzerTransitionAtMs = nowMs;
        Serial.printf("[Buzzer] tone on, freq=%u Hz\n", static_cast<unsigned int>(config.buzzerFrequencyHz));
    }
}

struct SaveResult {
    bool saved = false;
    bool hasValidationWarnings = false;
    bool networkReinitRequired = false;
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

bool parseMinuteOfDayFromTime(String rawValue, uint16_t &minutesOut) {
    rawValue.trim();
    if (rawValue.length() != 5 || rawValue[2] != ':') {
        return false;
    }

    const char h0 = rawValue[0];
    const char h1 = rawValue[1];
    const char m0 = rawValue[3];
    const char m1 = rawValue[4];
    if (h0 < '0' || h0 > '9' || h1 < '0' || h1 > '9' || m0 < '0' || m0 > '9' || m1 < '0' || m1 > '9') {
        return false;
    }

    const uint8_t hours = static_cast<uint8_t>((h0 - '0') * 10 + (h1 - '0'));
    const uint8_t minutes = static_cast<uint8_t>((m0 - '0') * 10 + (m1 - '0'));
    if (hours > 23 || minutes > 59) {
        return false;
    }

    minutesOut = static_cast<uint16_t>(hours * 60U + minutes);
    return true;
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

String localTimeString(bool &syncedOut) {
    const time_t now = time(nullptr);
    if (now <= 24UL * 60UL * 60UL) {
        syncedOut = false;
        return String("Time: not synced");
    }

    struct tm localTimeInfo;
    if (localtime_r(&now, &localTimeInfo) == nullptr) {
        syncedOut = false;
        return String("Time: unavailable");
    }

    char buffer[16];
    if (strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTimeInfo) == 0) {
        syncedOut = false;
        return String("Time: unavailable");
    }

    syncedOut = true;
    return String(buffer);
}

String statusJson() {
    clearWebMessageIfExpired();

    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();
    unsigned long nowMs = millis();
    bool timeSynced = false;
    const String localTime = localTimeString(timeSynced);
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
    json += "\"bmpConnected\":" + String(state.bmpConnected ? "true" : "false") + ",";
    json += "\"bmpTemperatureC\":" + String(state.bmpTemperatureC, 1) + ",";
    json += "\"bmpPressureHpa\":" + String(state.bmpPressureHpa, 1) + ",";
    json += "\"bmpValid\":" + String(state.bmpValid ? "true" : "false") + ",";
    json += "\"sensorConnected\":" + String(state.sensorConnected ? "true" : "false") + ",";
    json += "\"sensorError\":\"" + jsonEscape(state.sensorError) + "\",";
    json += "\"cloudStatus\":\"" + jsonEscape(state.cloudStatus) + "\",";
    json += "\"cloudError\":\"" + jsonEscape(state.cloudError) + "\",";
    json += "\"webMessage\":\"" + jsonEscape(state.webMessage) + "\"";
    json += ",\"sensorReadIntervalMs\":" + String(config.sensorReadIntervalMs);
    json += ",\"sensorAltitudeMeters\":" + String(config.sensorAltitudeMeters);
    json += ",\"displaySwitchIntervalMs\":" + String(config.displaySwitchIntervalMs);
    json += ",\"dndEnabled\":" + String(config.dndEnabled ? "true" : "false");
    json += ",\"dndStartMinutes\":" + String(config.dndStartMinutes);
    json += ",\"dndEndMinutes\":" + String(config.dndEndMinutes);
    json += ",\"normalBrightnessLevel\":" + String(config.normalBrightnessLevel);
    json += ",\"dndBrightnessLevel\":" + String(config.dndBrightnessLevel);
    json += ",\"thingSpeakIntervalSeconds\":" + String(config.thingSpeakIntervalSeconds);
    json += ",\"thingSpeakLastSyncMs\":" + String(cloudmanager::lastThingSpeakSyncMs());
    json += ",\"customHttpIntervalSeconds\":" + String(config.customHttpIntervalSeconds);
    json += ",\"customHttpLastSyncMs\":" + String(cloudmanager::lastCustomHttpSyncMs());
    json += ",\"nowMs\":" + String(nowMs);
    json += ",\"timeSynced\":" + String(timeSynced ? "true" : "false");
    json += ",\"localTime\":\"" + jsonEscape(localTime) + "\"";
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
            result.networkReinitRequired = true;
            mdnsReinitRequested = true;
        }
    }

    const bool wifiForm = server.hasArg("wifiSsid") || server.hasArg("wifiPassword");
    if (wifiForm) {
        // Wi-Fi form intentionally sends full credential pair, including empty password for open networks.
        updated.wifiSsid = trimmedArg("wifiSsid");
        updated.wifiPassword = trimmedArg("wifiPassword");

        if (updated.wifiSsid != previous.wifiSsid || updated.wifiPassword != previous.wifiPassword) {
            result.networkReinitRequired = true;
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

    if (server.hasArg("sensorAltitudeMeters")) {
        unsigned long parsedAltitude = 0;
        if (!Validation::parseUnsignedLongStrict(server.arg("sensorAltitudeMeters"), parsedAltitude)) {
            addValidationIssue(result, "sensorAltitudeMeters", "Sensor altitude must be a non-negative integer");
        } else if (Validation::isValidSensorAltitude(parsedAltitude)) {
            updated.sensorAltitudeMeters = static_cast<uint16_t>(parsedAltitude);
        } else {
            addValidationIssue(result, "sensorAltitudeMeters", "Sensor altitude must be between " +
                                                     String(appconfig::kSensorAltitudeMinMeters) + " and " +
                                                     String(appconfig::kSensorAltitudeMaxMeters) + " meters");
        }
    }

    if (server.hasArg("displaySwitchIntervalMs")) {
        unsigned long parsedDisplaySwitchInterval = 0;
        if (!Validation::parseUnsignedLongStrict(server.arg("displaySwitchIntervalMs"), parsedDisplaySwitchInterval)) {
            addValidationIssue(result, "displaySwitchIntervalMs", "Display switch interval must be a positive integer");
        } else if (Validation::isValidDisplaySwitchInterval(parsedDisplaySwitchInterval)) {
            updated.displaySwitchIntervalMs = parsedDisplaySwitchInterval;
        } else {
            addValidationIssue(
                result,
                "displaySwitchIntervalMs",
                "Display switch interval must be between " +
                    String(appconfig::kDisplaySwitchIntervalMinMs) + " and " +
                    String(appconfig::kDisplaySwitchIntervalMaxMs) + " ms");
        }
    }

    const bool dndForm = server.hasArg("dndEnabled") || server.hasArg("dndStartMinutes") || server.hasArg("dndEndMinutes") || server.hasArg("dndStartTime") || server.hasArg("dndEndTime") || server.hasArg("normalBrightnessLevel") || server.hasArg("dndBrightnessLevel") || server.hasArg("normalBrightnessPercent") || server.hasArg("dndBrightnessPercent");
    if (dndForm) {
        bool dndEnabled = updated.dndEnabled;
        if (server.hasArg("dndEnabled")) {
            if (!Validation::parseBoolStrict(server.arg("dndEnabled"), dndEnabled)) {
                addValidationIssue(result, "dndEnabled", "DnD enabled flag must be 0 or 1");
            }
        }
        updated.dndEnabled = dndEnabled;

        if (server.hasArg("dndStartTime")) {
            uint16_t parsedStartMinutes = 0;
            if (!parseMinuteOfDayFromTime(server.arg("dndStartTime"), parsedStartMinutes)) {
                addValidationIssue(result, "dndStartTime", "DnD start time must be in HH:MM format");
            } else {
                updated.dndStartMinutes = parsedStartMinutes;
            }
        }

        if (server.hasArg("dndEndTime")) {
            uint16_t parsedEndMinutes = 0;
            if (!parseMinuteOfDayFromTime(server.arg("dndEndTime"), parsedEndMinutes)) {
                addValidationIssue(result, "dndEndTime", "DnD end time must be in HH:MM format");
            } else {
                updated.dndEndMinutes = parsedEndMinutes;
            }
        }

        if (server.hasArg("dndStartMinutes")) {
            unsigned long parsedStartMinutes = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("dndStartMinutes"), parsedStartMinutes)) {
                addValidationIssue(result, "dndStartMinutes", "DnD start must be a positive integer");
            } else if (Validation::isValidDndMinuteOfDay(parsedStartMinutes)) {
                updated.dndStartMinutes = static_cast<uint16_t>(parsedStartMinutes);
            } else {
                addValidationIssue(result, "dndStartMinutes", "DnD start must be in 0..1439 minutes");
            }
        }

        if (server.hasArg("dndEndMinutes")) {
            unsigned long parsedEndMinutes = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("dndEndMinutes"), parsedEndMinutes)) {
                addValidationIssue(result, "dndEndMinutes", "DnD end must be a positive integer");
            } else if (Validation::isValidDndMinuteOfDay(parsedEndMinutes)) {
                updated.dndEndMinutes = static_cast<uint16_t>(parsedEndMinutes);
            } else {
                addValidationIssue(result, "dndEndMinutes", "DnD end must be in 0..1439 minutes");
            }
        }

        if (server.hasArg("normalBrightnessLevel")) {
            unsigned long parsedBrightnessLevel = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("normalBrightnessLevel"), parsedBrightnessLevel)) {
                addValidationIssue(result, "normalBrightnessLevel", "Normal brightness must be an integer in 0..255");
            } else if (Validation::isValidBrightnessLevel(parsedBrightnessLevel)) {
                updated.normalBrightnessLevel = static_cast<uint8_t>(parsedBrightnessLevel);
            } else {
                addValidationIssue(result, "normalBrightnessLevel", "Normal brightness must be between 0 and 255");
            }
        }

        if (server.hasArg("normalBrightnessPercent")) {
            unsigned long parsedBrightnessPercent = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("normalBrightnessPercent"), parsedBrightnessPercent)) {
                addValidationIssue(result, "normalBrightnessPercent", "Legacy normal brightness must be an integer percent");
            } else if (parsedBrightnessPercent <= 100UL) {
                updated.normalBrightnessLevel = static_cast<uint8_t>((static_cast<uint16_t>(appconfig::kOledBaseContrast) * parsedBrightnessPercent) / 100UL);
            } else {
                addValidationIssue(result, "normalBrightnessPercent", "Legacy normal brightness must be between 0 and 100%");
            }
        }

        if (server.hasArg("dndBrightnessLevel")) {
            unsigned long parsedBrightnessLevel = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("dndBrightnessLevel"), parsedBrightnessLevel)) {
                addValidationIssue(result, "dndBrightnessLevel", "DnD brightness must be an integer in 0..255");
            } else if (Validation::isValidBrightnessLevel(parsedBrightnessLevel)) {
                updated.dndBrightnessLevel = static_cast<uint8_t>(parsedBrightnessLevel);
            } else {
                addValidationIssue(result, "dndBrightnessLevel", "DnD brightness must be between 0 and 255");
            }
        }

        if (server.hasArg("dndBrightnessPercent")) {
            unsigned long parsedBrightnessPercent = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("dndBrightnessPercent"), parsedBrightnessPercent)) {
                addValidationIssue(result, "dndBrightnessPercent", "Legacy DnD brightness must be an integer percent");
            } else if (parsedBrightnessPercent <= 100UL) {
                updated.dndBrightnessLevel = static_cast<uint8_t>((static_cast<uint16_t>(appconfig::kOledBaseContrast) * parsedBrightnessPercent) / 100UL);
            } else {
                addValidationIssue(result, "dndBrightnessPercent", "Legacy DnD brightness must be between 0 and 100%");
            }
        }
    }

    const bool buzzerForm = server.hasArg("buzzerFrequencyHz") || server.hasArg("buzzerToneDurationMs") || server.hasArg("buzzerPauseDurationMs");
    if (buzzerForm) {
        if (server.hasArg("buzzerFrequencyHz")) {
            unsigned long parsedFrequency = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("buzzerFrequencyHz"), parsedFrequency)) {
                addValidationIssue(result, "buzzerFrequencyHz", "Buzzer frequency must be a positive integer");
            } else if (Validation::isValidBuzzerFrequencyHz(parsedFrequency)) {
                updated.buzzerFrequencyHz = parsedFrequency;
            } else {
                addValidationIssue(result, "buzzerFrequencyHz", "Buzzer frequency must be between 100 and 5000 Hz");
            }
        }

        if (server.hasArg("buzzerToneDurationMs")) {
            unsigned long parsedToneDuration = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("buzzerToneDurationMs"), parsedToneDuration)) {
                addValidationIssue(result, "buzzerToneDurationMs", "Buzzer tone duration must be a positive integer");
            } else if (Validation::isValidBuzzerDurationMs(parsedToneDuration)) {
                updated.buzzerToneDurationMs = parsedToneDuration;
            } else {
                addValidationIssue(result, "buzzerToneDurationMs", "Buzzer tone duration must be between 50 and 5000 ms");
            }
        }

        if (server.hasArg("buzzerPauseDurationMs")) {
            unsigned long parsedPauseDuration = 0;
            if (!Validation::parseUnsignedLongStrict(server.arg("buzzerPauseDurationMs"), parsedPauseDuration)) {
                addValidationIssue(result, "buzzerPauseDurationMs", "Buzzer pause duration must be a positive integer");
            } else if (Validation::isValidBuzzerDurationMs(parsedPauseDuration)) {
                updated.buzzerPauseDurationMs = parsedPauseDuration;
            } else {
                addValidationIssue(result, "buzzerPauseDurationMs", "Buzzer pause duration must be between 50 and 5000 ms");
            }
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
    if (result.networkReinitRequired) {
        networkReinitRequested = true;
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
        setWebMessage("Calibration completed successfully.", 8000);
    } else {
        setWebMessage("Calibration failed: " + (error.length() > 0 ? error : String("unknown error")), 10000);
    }

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Calibrate");
}

void handleBuzzerTest() {
    SettingsData config = settings::get();
    if (config.buzzerFrequencyHz == 0) {
        setWebMessage("Buzzer settings are invalid. Please save valid values first.", 8000);
    } else {
        buzzerTestActive = true;
        buzzerToneActive = true;
        buzzerTestStartedAtMs = millis();
        buzzerTransitionAtMs = buzzerTestStartedAtMs;
        tone(appconfig::kBuzzerPin, static_cast<unsigned int>(config.buzzerFrequencyHz));
        Serial.printf("[Buzzer] started, freq=%u Hz\n", static_cast<unsigned int>(config.buzzerFrequencyHz));
        setWebMessage("Buzzer test started.", 5000);
    }

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Buzzer test");
}

void handleBuzzerStop() {
    stopBuzzerTest();
    setWebMessage("Buzzer test stopped.", 5000);

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Buzzer stop");
}

void handleApiPost() {
    SaveResult result = processSaveRequest();
    String json = "{";
    json += "\"ok\":true";
    json += ",\"saved\":" + String(result.saved ? "true" : "false");
    json += ",\"restartRequired\":false";
    json += ",\"networkReinitRequired\":" + String(result.networkReinitRequired ? "true" : "false");
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
    server.on("/buzzer-test", HTTP_POST, handleBuzzerTest);
    server.on("/buzzer-stop", HTTP_POST, handleBuzzerStop);
    server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
    server.onNotFound(handleNotFound);
    server.begin();

    if (MDNS.begin(buildHostname().c_str())) {
        MDNS.addService("http", "tcp", 80);
    }
}

void loop() {
    updateBuzzerTestState();
    server.handleClient();

    if (networkReinitRequested) {
        networkReinitRequested = false;
        wifiportal::requestReconfigure();
    }

    if (mdnsReinitRequested) {
        mdnsReinitRequested = false;
        MDNS.end();
        if (MDNS.begin(buildHostname().c_str())) {
            MDNS.addService("http", "tcp", 80);
        }
    }

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




