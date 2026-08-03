#include "web/web_server.h"

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
    json += ",\"firmwareBuildDate\":\"" + jsonEscape(appconfig::kFirmwareBuildDate) + "\"";
    json += "}";
    return json;
}

String pageHtml() {
    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();
    unsigned long nowMs = millis();
    String wifiSsidValue = config.wifiSsid;
    if (wifiSsidValue.length() == 0 && WiFi.status() == WL_CONNECTED) {
        wifiSsidValue = WiFi.SSID();
    }
    String html;
        html.reserve(22000);
        html += R"HTML(<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">)HTML";
        html += R"HTML(<title>ESP32 CO2 Air Portal</title><style>:root{--bg:#07111e;--surface:#111a2d;--surface2:#17233b;--border:#263553;--accent:#59c3ff;--accent2:#7cdbb2;--ok:#48d597;--warn:#f8c35f;--danger:#f27b7b;--text:#e8f2ff;--muted:#90a4c2;--radius:16px;--radius-sm:10px}*{box-sizing:border-box;margin:0;padding:0}body{background:radial-gradient(circle at top,#10213a 0,#07111e 45%,#050b14 100%);color:var(--text);font-family:Segoe UI,system-ui,-apple-system,sans-serif;min-height:100vh;padding:18px}.container{max-width:980px;margin:0 auto}.header{background:linear-gradient(135deg,#14233f 0,#10192d 100%);border:1px solid var(--border);border-radius:var(--radius);padding:20px 22px;margin-bottom:16px;display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}.header-left h1{font-size:1.35rem;font-weight:800;margin-bottom:4px}.title-gradient{background:linear-gradient(90deg,var(--accent),var(--accent2));-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}.header-left p{color:var(--muted);font-size:.86rem}.header-badge{padding:8px 12px;border-radius:999px;background:rgba(89,195,255,.12);border:1px solid rgba(89,195,255,.25);color:#bfe9ff;font-weight:700;font-size:.83rem}.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:10px;margin-bottom:16px}.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-sm);padding:14px 16px}.card-label{font-size:.7rem;text-transform:uppercase;letter-spacing:.08em;color:var(--muted);margin-bottom:4px}.card-value{font-size:1.25rem;font-weight:800}.card-value.accent{color:var(--accent)}.card-value.ok{color:var(--ok)}.card-value.warn{color:var(--warn)}.card-value.danger{color:var(--danger)}.status-message{margin-bottom:16px;padding:10px 12px;border-radius:var(--radius-sm);border:1px solid transparent;font-size:.88rem;font-weight:600}.status-message.success{background:rgba(72,213,151,.12);border-color:rgba(72,213,151,.3);color:#a7f0ca}.status-message.error{background:rgba(242,123,123,.12);border-color:rgba(242,123,123,.3);color:#ffb0b0}.tabs-nav{display:flex;background:var(--surface);border:1px solid var(--border);border-radius:var(--radius) var(--radius) 0 0;overflow:hidden;flex-wrap:wrap}.tab-btn{flex:1;min-width:120px;padding:13px 14px;background:transparent;border:none;color:var(--muted);font-size:.88rem;font-weight:700;cursor:pointer;transition:background .2s,color .2s,border-color .2s;border-bottom:2px solid transparent;display:flex;align-items:center;justify-content:center;gap:6px}.tab-btn:hover{background:var(--surface2);color:var(--text)}.tab-btn.active{color:var(--accent);border-bottom-color:var(--accent);background:var(--surface2)}.tabs-body{background:var(--surface);border:1px solid var(--border);border-top:none;border-radius:0 0 var(--radius) var(--radius);padding:20px;margin-bottom:16px}.tab-panel{display:none}.tab-panel.active{display:block}.setting-group{margin-bottom:14px}.setting-label{display:block;font-size:.78rem;text-transform:uppercase;letter-spacing:.07em;color:var(--muted);margin-bottom:6px}.setting-row{display:flex;gap:8px;align-items:stretch}.setting-row input,.setting-row select,.setting-group input,.setting-group select{flex:1;width:100%;background:#0a1322;border:1px solid var(--border);border-radius:var(--radius-sm);color:var(--text);padding:10px 12px;font-size:.92rem;outline:none;transition:border-color .2s}.setting-row input:focus,.setting-row select:focus,.setting-group input:focus,.setting-group select:focus{border-color:var(--accent)}.setting-row select option,.setting-group select option{background:var(--surface)}.btn-set,.btn{background:linear-gradient(135deg,var(--accent),var(--accent2));color:#06111d;border:none;border-radius:var(--radius-sm);padding:10px 18px;font-size:.86rem;font-weight:800;cursor:pointer;white-space:nowrap;transition:opacity .15s,transform .1s;text-decoration:none;display:inline-block}.btn:hover,.btn-set:hover{opacity:.88}.btn:active,.btn-set:active{transform:scale(.98)}.divider{border:none;border-top:1px solid var(--border);margin:16px 0}.section-title{font-size:.72rem;text-transform:uppercase;letter-spacing:.1em;color:var(--accent2);margin-bottom:4px;font-weight:800}.cloud-last-sync{font-size:.72rem;color:var(--muted);margin-bottom:12px}.muted{color:var(--muted);font-size:.9rem}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}.stack{display:grid;gap:12px}.small{font-size:.82rem}.hint{margin-top:8px;color:var(--muted);font-size:.82rem}.footer-actions{display:flex;gap:10px;flex-wrap:wrap}.pill{display:inline-flex;align-items:center;gap:6px;padding:6px 10px;border-radius:999px;background:#0a1322;border:1px solid var(--border);color:#cde7ff;font-size:.78rem}.air-icon{font-size:1.4rem;line-height:1}</style></head><body><div class="container">)HTML";
        html += "<div class=header><div class=header-left><h1><span class=air-icon>&#128168;</span> <span class=title-gradient>CO2 Air Portal</span></h1><p><span id=device-name>" + escapeHtml(config.deviceName) + "</span> &nbsp;&#183;&nbsp; <span id=device-ip>" + escapeHtml(emptyIfBlank(state.ipAddress.length() > 0 ? state.ipAddress : state.apAddress)) + "</span></p></div><div class=header-badge id=portal-mode>" + String(state.setupMode ? "AP Setup" : "Online") + "</div></div>";
        html += "<div class=cards><div class=card><div class=card-label>CO2</div><div class=card-value accent";
        html += state.sensorConnected ? "" : " warn";
        html += " id=co2-value>" + String(state.co2Ppm > 0 ? state.co2Ppm : state.lastValidPpm) + " ppm</div></div>";
        html += "</div>";

        html += "<div class=footer-actions style='margin-bottom:16px'><form id=calibrate-form action=/calibrate method=post><button class=btn type=button style='background:linear-gradient(135deg,#f27b7b,#f8c35f);color:#20120e' onclick='confirmCalibration()'>Calibrate sensor</button></form></div>";

        html += "<div id=status-message class=status-message style='display:";
        html += state.webMessage.length() > 0 ? "block" : "none";
        html += "'>" + escapeHtml(state.webMessage) + "</div>";

        html += "<div class=tabs-nav>";
        html += "<button class='tab-btn active' onclick=\"switchTab('main',this)\">&#9881; Main</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('wifi',this)\">&#128246; Wi-Fi</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('cloud',this)\">&#9729; Cloud</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('firmware',this)\">&#128190; Firmware</button>";
        html += "</div>";

        html += "<div class=tabs-body>";

        html += "<div class='tab-panel active' id=tab-main><div class=stack>";
        html += "<form action=/api method=post><div class=setting-group><label class=setting-label>Device name</label><div class=setting-row><input name=deviceName value='" + escapeHtml(config.deviceName) + "'><button class=btn-set type=submit>Set</button></div></div></form>";
        html += "<form action=/api method=post><div class=setting-group><label class=setting-label>Sensor read interval, ms</label><div class=setting-row><input type=number min=100 name=sensorReadIntervalMs value='" + String(config.sensorReadIntervalMs) + "'><button class=btn-set type=submit>Set</button></div><p class=hint>Controls how often the sensor task polls the CO2 sensor.</p></div></form>";
        html += "</div></div>";

        html += "<div class=tab-panel id=tab-wifi><form action=/api method=post><div class=stack><div class=setting-group><label class=setting-label>Wi-Fi SSID</label><input name=wifiSsid value='" + escapeHtml(wifiSsidValue) + "'></div><div class=setting-group><label class=setting-label>Wi-Fi password</label><input type=password name=wifiPassword value='" + escapeHtml(config.wifiPassword) + "'></div><div class=footer-actions><button class=btn-set type=submit>Save Wi-Fi</button></div><p class=hint>Use the access point if the device is offline. AP is named after the device.</p></div></form></div>";

        html += "<div class=tab-panel id=tab-cloud>";

        html += "<div class=section-title>ThingSpeak</div>";
        html += "<p class=cloud-last-sync id=ts-last-sync>" + lastSyncLabel(cloudmanager::lastThingSpeakSyncMs(), nowMs) + "</p>";
        html += "<form action=/api method=post><div class=stack>";
        html += "<div class=setting-group><label class=setting-label>Logging Enabled</label><div class=setting-row><select name=thingSpeakEnabled><option value=0" + String(config.thingSpeakEnabled ? "" : " selected") + ">Disabled</option><option value=1" + String(config.thingSpeakEnabled ? " selected" : "") + ">Enabled</option></select></div></div>";
        html += "<div class=setting-group><label class=setting-label>API Key</label><div class=setting-row><input type=text name=thingSpeakApiKey value='" + escapeHtml(config.thingSpeakApiKey) + "' placeholder='ThingSpeak Write API Key'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Send Interval (seconds, min 15)</label><div class=setting-row><input type=number min=15 name=thingSpeakIntervalSeconds value='" + String(config.thingSpeakIntervalSeconds) + "'></div></div>";
        html += "<div class=footer-actions><button class=btn-set type=submit>Save ThingSpeak</button></div></div></form>";

        html += "<hr class=divider>";
        html += "<div class=section-title>Custom HTTP POST</div>";
        html += "<p class=cloud-last-sync id=http-last-sync>" + lastSyncLabel(cloudmanager::lastCustomHttpSyncMs(), nowMs) + "</p>";
        html += "<form action=/api method=post><div class=stack>";
        html += "<div class=setting-group><label class=setting-label>HTTP POST Enabled</label><div class=setting-row><select name=customHttpEnabled><option value=0" + String(config.customHttpEnabled ? "" : " selected") + ">Disabled</option><option value=1" + String(config.customHttpEnabled ? " selected" : "") + ">Enabled</option></select></div></div>";
        html += "<div class=setting-group><label class=setting-label>HTTP Method</label><div class=setting-row><input type=text name=customHttpMethod value='" + escapeHtml(config.customHttpMethod) + "' placeholder='POST'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Server Address (URL template)</label><div class=setting-row><input type=text name=customHttpUrlTemplate value='" + escapeHtml(config.customHttpUrlTemplate) + "' placeholder='http://192.168.1.100:8080/api/data?ppm={ppm}'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Content Type</label><div class=setting-row><input type=text name=customHttpContentType value='" + escapeHtml(config.customHttpContentType) + "' placeholder='application/json'></div></div>";
        html += "<div class=setting-group><label class=setting-label>JSON Body Template</label><div class=setting-row><input type=text name=customHttpBodyTemplate value='" + escapeHtml(config.customHttpBodyTemplate) + "' placeholder='{\"ppm\":{ppm}}'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Send Interval (seconds, min 15)</label><div class=setting-row><input type=number min=15 name=customHttpIntervalSeconds value='" + String(config.customHttpIntervalSeconds) + "'></div></div>";
        html += "<div class=footer-actions><button class=btn-set type=submit>Save Custom HTTP</button></div><p class=hint>In custom HTTP use <b>{ppm}</b> in URL or body. Intervals are saved per provider.</p></div></form></div>";

        html += "<div class=tab-panel id=tab-firmware><div class=stack><div class=setting-group><label class=setting-label>Firmware version</label><div class=setting-row><input value='" + escapeHtml(appconfig::kFirmwareVersion) + "' readonly></div></div><div class=setting-group><label class=setting-label>Build date</label><div class=setting-row><input value='" + escapeHtml(appconfig::kFirmwareBuildDate) + "' readonly></div></div><form action=/update method=post enctype='multipart/form-data'><div class=setting-group><label class=setting-label>Firmware upload</label><input type=file name=update accept='.bin'></div><div class=footer-actions><button class=btn-set type=submit>Upload OTA</button></div><p class=hint>Upload a compiled .bin file. Device will reboot after successful flash.</p></div></form></div>";

        html += "</div>";

        html += R"HTML(<script>
function switchTab(id, button) {
    document.querySelectorAll('.tab-panel').forEach(panel => panel.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(tab => tab.classList.remove('active'));
    document.getElementById('tab-' + id).classList.add('active');
    button.classList.add('active');
}

function confirmCalibration() {
    const message = 'Confirm CO2 zero calibration? Use only in fresh outdoor air (~400 ppm).';
    if (window.confirm(message)) {
        const form = document.getElementById('calibrate-form');
        if (form) form.submit();
    }
}

async function refreshLiveData() {
    try {
        const response = await fetch('/api', { cache: 'no-store' });
        const data = await response.json();
        const co2Value = document.getElementById('co2-value');
        const wifiValue = document.getElementById('wifi-value');
        const ipValue = document.getElementById('ip-value');
        const cloudValue = document.getElementById('cloud-value');
        const deviceName = document.getElementById('device-name');
        const deviceIp = document.getElementById('device-ip');
        const portalMode = document.getElementById('portal-mode');
        const statusMessage = document.getElementById('status-message');
        const tsLastSync = document.getElementById('ts-last-sync');
        const httpLastSync = document.getElementById('http-last-sync');

        const formatLastSync = (lastSyncMs) => {
            if (!lastSyncMs || !data.nowMs || data.nowMs < lastSyncMs) return 'Last sync: -';
            const elapsedSeconds = Math.floor((data.nowMs - lastSyncMs) / 1000);
            if (elapsedSeconds < 60) return `Last sync: ${elapsedSeconds}s ago`;
            return `Last sync: ${Math.floor(elapsedSeconds / 60)}m ago`;
        };

        if (co2Value) co2Value.textContent = (data.co2Ppm || 0) + ' ppm';
        if (wifiValue) wifiValue.textContent = data.wifiConnected ? 'Connected' : (data.setupMode ? 'Setup AP' : 'Offline');
        if (ipValue) ipValue.textContent = data.ipAddress || data.apAddress || '-';
        if (cloudValue) cloudValue.textContent = data.cloudStatus || 'Idle';
        if (deviceName) deviceName.textContent = data.deviceName || '';
        if (deviceIp) deviceIp.textContent = data.ipAddress || data.apAddress || '-';
        if (portalMode) portalMode.textContent = data.setupMode ? 'AP Setup' : 'Online';
        if (tsLastSync) tsLastSync.textContent = formatLastSync(data.thingSpeakLastSyncMs || 0);
        if (httpLastSync) httpLastSync.textContent = formatLastSync(data.customHttpLastSyncMs || 0);

        if (statusMessage && data.webMessage) {
            statusMessage.style.display = 'block';
            statusMessage.textContent = data.webMessage;
            statusMessage.className = 'status-message success';
        }
    } catch (error) {
        console.warn(error);
    }
}

refreshLiveData();
setInterval(refreshLiveData, 1000);
</script></body></html>)HTML";
    return html;
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
    if (lockAppState()) {
        gAppState.webMessage = "Settings saved";
        unlockAppState();
    }
    restartRequested = true;

    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Saved");
}

void handleCalibrate() {
    String error;
    if (sensor::calibrateZero(error)) {
        if (lockAppState()) {
            gAppState.webMessage = "Calibration command sent. Keep sensor in fresh air (~400 ppm) for 20 minutes.";
            unlockAppState();
        }
    } else {
        if (lockAppState()) {
            gAppState.webMessage = "Calibration failed: " + (error.length() > 0 ? error : String("unknown error"));
            unlockAppState();
        }
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

