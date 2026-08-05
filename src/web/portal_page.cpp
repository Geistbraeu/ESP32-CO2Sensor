#include "web/portal_page.h"

#include <WiFi.h>

#include "app_config.h"
#include "app_view_models.h"
#include "cloud/CloudManager.h"

namespace {
String emptyIfBlank(const String &value) {
    return value.length() > 0 ? value : String("-");
}

String escapeHtml(String value) {
    value.replace("&", "&amp;");
    value.replace("<", "&lt;");
    value.replace(">", "&gt;");
    value.replace("\"", "&quot;");
    value.replace("'", "&#39;");
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

String formatTemperature(float value) {
    return String(value, 1) + " C";
}

String formatHumidity(float value) {
    return String(value, 1) + " %";
}
}  // namespace

namespace webpage {String render() {
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
        html += "<div class=card><div class=card-label>Temperature</div><div class='card-value ok";
        html += state.climateValid ? "" : " warn";
        html += "' id=temp-value>" + String(state.climateValid ? formatTemperature(state.temperatureC) : String("-")) + "</div></div>";
        html += "<div class=card><div class=card-label>Humidity</div><div class='card-value ok";
        html += state.climateValid ? "" : " warn";
        html += "' id=hum-value>" + String(state.climateValid ? formatHumidity(state.humidityPct) : String("-")) + "</div></div>";
        html += "</div>";

        String sensorStatus = "Sensor ready";
        if (state.sensorError.length() > 0) {
            sensorStatus = state.sensorError;
        }
        html += "<p class=hint id=sensor-status style='margin:-6px 0 12px 2px'>" + escapeHtml(sensorStatus) + "</p>";

        html += "<div id=status-message class=status-message style='display:";
        html += state.webMessage.length() > 0 ? "block" : "none";
        html += "'>" + escapeHtml(state.webMessage) + "</div>";

        html += "<div class=tabs-nav>";
        html += "<button class='tab-btn active' onclick=\"switchTab('main',this)\">&#9881; Main</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('wifi',this)\">&#128246; Wi-Fi</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('cloud',this)\">&#9729; Cloud</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('calibration',this)\">&#9878; Calibration</button>";
        html += "<button class='tab-btn' onclick=\"switchTab('firmware',this)\">&#128190; Firmware</button>";
        html += "</div>";

        html += "<div class=tabs-body>";

        html += "<div class='tab-panel active' id=tab-main><div class=stack>";
        html += "<form action=/save method=post><div class=setting-group><label class=setting-label>Device name</label><div class=setting-row><input name=deviceName value='" + escapeHtml(config.deviceName) + "'><button class=btn-set type=submit>Set</button></div></div></form>";
        html += "<form action=/save method=post><div class=setting-group><label class=setting-label>Sensor read interval, ms</label><div class=setting-row><input type=number min=5000 name=sensorReadIntervalMs value='" + String(config.sensorReadIntervalMs) + "'><button class=btn-set type=submit>Set</button></div><p class=hint>Controls how often the sensor task polls the CO2 sensor (minimum 5000 ms).</p></div></form>";
        html += "</div></div>";

        html += "<div class=tab-panel id=tab-wifi><form action=/save method=post><div class=stack><div class=setting-group><label class=setting-label>Wi-Fi SSID</label><input name=wifiSsid value='" + escapeHtml(wifiSsidValue) + "'></div><div class=setting-group><label class=setting-label>Wi-Fi password</label><input type=password name=wifiPassword value='" + escapeHtml(config.wifiPassword) + "'></div><div class=footer-actions><button class=btn-set type=submit>Save Wi-Fi</button></div><p class=hint>Use the access point if the device is offline. AP is named after the device.</p></div></form></div>";

        html += "<div class=tab-panel id=tab-cloud>";

        html += "<div class=section-title>ThingSpeak</div>";
        html += "<p class=cloud-last-sync id=ts-last-sync>" + lastSyncLabel(cloudmanager::lastThingSpeakSyncMs(), nowMs) + "</p>";
        html += "<form action=/save method=post><div class=stack>";
        html += "<div class=setting-group><label class=setting-label>Logging Enabled</label><div class=setting-row><select name=thingSpeakEnabled><option value=0" + String(config.thingSpeakEnabled ? "" : " selected") + ">Disabled</option><option value=1" + String(config.thingSpeakEnabled ? " selected" : "") + ">Enabled</option></select></div></div>";
        html += "<div class=setting-group><label class=setting-label>API Key</label><div class=setting-row><input type=text name=thingSpeakApiKey value='" + escapeHtml(config.thingSpeakApiKey) + "' placeholder='ThingSpeak Write API Key'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Send Interval (seconds, min 15)</label><div class=setting-row><input type=number min=15 name=thingSpeakIntervalSeconds value='" + String(config.thingSpeakIntervalSeconds) + "'></div></div>";
        html += "<div class=footer-actions><button class=btn-set type=submit>Save ThingSpeak</button></div></div></form>";

        html += "<hr class=divider>";
        html += "<div class=section-title>Custom HTTP POST</div>";
        html += "<p class=cloud-last-sync id=http-last-sync>" + lastSyncLabel(cloudmanager::lastCustomHttpSyncMs(), nowMs) + "</p>";
        html += "<form action=/save method=post><div class=stack>";
        html += "<div class=setting-group><label class=setting-label>HTTP POST Enabled</label><div class=setting-row><select name=customHttpEnabled><option value=0" + String(config.customHttpEnabled ? "" : " selected") + ">Disabled</option><option value=1" + String(config.customHttpEnabled ? " selected" : "") + ">Enabled</option></select></div></div>";
        html += "<div class=setting-group><label class=setting-label>HTTP Method</label><div class=setting-row><input type=text name=customHttpMethod value='" + escapeHtml(config.customHttpMethod) + "' placeholder='POST'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Server Address (URL template)</label><div class=setting-row><input type=text name=customHttpUrlTemplate value='" + escapeHtml(config.customHttpUrlTemplate) + "' placeholder='http://192.168.1.100:8080/api/data?ppm={ppm}'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Content Type</label><div class=setting-row><input type=text name=customHttpContentType value='" + escapeHtml(config.customHttpContentType) + "' placeholder='application/json'></div></div>";
        html += "<div class=setting-group><label class=setting-label>JSON Body Template</label><div class=setting-row><input type=text name=customHttpBodyTemplate value='" + escapeHtml(config.customHttpBodyTemplate) + "' placeholder='{\"ppm\":{ppm}}'></div></div>";
        html += "<div class=setting-group><label class=setting-label>Send Interval (seconds, min 15)</label><div class=setting-row><input type=number min=15 name=customHttpIntervalSeconds value='" + String(config.customHttpIntervalSeconds) + "'></div></div>";
        html += "<div class=footer-actions><button class=btn-set type=submit>Save Custom HTTP</button></div><p class=hint>In custom HTTP use <b>{ppm}</b> in URL or body. Intervals are saved per provider.</p></div></form></div>";

        html += "<div class=tab-panel id=tab-calibration><div class=stack>";
        html += "<div class=setting-group><label class=setting-label>Manual Calibration Steps</label>";
        html += "<p class=hint><b>1. Move the device to fresh outdoor air:</b> Place the desktop monitor outside or on an open balcony.</p>";
        html += "<p class=hint><b>2. Let it run for 3-5 minutes.</b></p>";
        html += "<p class=hint><b>3. Press the \"Calibrate\" button.</b></p>";
        html += "</div>";
        html += "<div class=footer-actions><form id=calibrate-form action=/calibrate method=post><button class=btn type=button style='background:linear-gradient(135deg,#f27b7b,#f8c35f);color:#20120e' onclick='confirmCalibration()'>Calibrate</button></form></div>";
        html += "</div></div>";

        html += "<div class=tab-panel id=tab-firmware><div class=stack><div class=setting-group><label class=setting-label>Firmware version</label><div class=setting-row><input id=firmware-version-value value='" + escapeHtml(appconfig::kFirmwareVersion) + "' readonly></div></div><div class=setting-group><label class=setting-label>Build date</label><div class=setting-row><input id=firmware-build-date-value value='" + escapeHtml(appconfig::firmwareBuildDateString()) + "' readonly></div></div><form id=firmware-upload-form action=/update method=post enctype='multipart/form-data'><div class=setting-group><label class=setting-label>Firmware upload</label><input type=file name=update accept='.bin'></div><div class=footer-actions><button class=btn-set type=submit>Upload OTA</button></div><p class=hint>Upload a compiled .bin file. Device will reboot after successful flash.</p></div></form></div>";

        html += "</div>";

        html += R"HTML(<script>
function switchTab(id, button) {
    document.querySelectorAll('.tab-panel').forEach(panel => panel.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(tab => tab.classList.remove('active'));
    document.getElementById('tab-' + id).classList.add('active');
    button.classList.add('active');
}

function confirmCalibration() {
    const message = 'Start calibration now? Make sure the device has been in fresh outdoor air for 3-5 minutes.';
    if (window.confirm(message)) {
        const form = document.getElementById('calibrate-form');
        if (form) form.submit();
    }
}

let firmwareUploadPending = false;
let firmwareUploadApiDownSeen = false;
let firmwareUploadSuccessTimer = null;
const currentFirmwareVersion = document.getElementById('firmware-version-value')?.value || '';
const currentFirmwareBuildDate = document.getElementById('firmware-build-date-value')?.value || '';

function setStatusMessage(text, className) {
    const statusMessage = document.getElementById('status-message');
    if (!statusMessage) return;
    statusMessage.style.display = 'block';
    statusMessage.textContent = text;
    statusMessage.className = 'status-message ' + className;
}

function clearStatusMessage() {
    const statusMessage = document.getElementById('status-message');
    if (!statusMessage) return;
    statusMessage.style.display = 'none';
    statusMessage.textContent = '';
}

function clearFirmwareUploadState() {
    firmwareUploadPending = false;
    firmwareUploadApiDownSeen = false;
    if (firmwareUploadSuccessTimer !== null) {
        window.clearTimeout(firmwareUploadSuccessTimer);
        firmwareUploadSuccessTimer = null;
    }
}

async function uploadFirmware(event) {
    event.preventDefault();

    const form = event.currentTarget;
    const formData = new FormData(form);
    const submitButton = form.querySelector('button[type=submit]');

    if (submitButton) submitButton.disabled = true;

    try {
        const response = await fetch(form.action, {
            method: 'POST',
            body: formData,
            cache: 'no-store'
        });

        const responseText = await response.text();
        if (!response.ok || responseText.trim() !== 'OK') {
            throw new Error(responseText || 'Upload failed');
        }

        firmwareUploadPending = true;
        firmwareUploadApiDownSeen = false;
        if (firmwareUploadSuccessTimer !== null) {
            window.clearTimeout(firmwareUploadSuccessTimer);
        }
        firmwareUploadSuccessTimer = window.setTimeout(() => {
            if (firmwareUploadPending) {
                clearFirmwareUploadState();
                clearStatusMessage();
            }
        }, 10000);
        setStatusMessage('Firmware uploaded. Device is rebooting.', 'success');
    } catch (error) {
        clearFirmwareUploadState();
        setStatusMessage('Firmware upload failed.', 'error');
        console.warn(error);
    } finally {
        if (submitButton) submitButton.disabled = false;
    }
}

async function refreshLiveData() {
    try {
        const response = await fetch('/api', { cache: 'no-store' });
        const data = await response.json();
        const co2Value = document.getElementById('co2-value');
        const tempValue = document.getElementById('temp-value');
        const humValue = document.getElementById('hum-value');
        const wifiValue = document.getElementById('wifi-value');
        const ipValue = document.getElementById('ip-value');
        const cloudValue = document.getElementById('cloud-value');
        const deviceName = document.getElementById('device-name');
        const deviceIp = document.getElementById('device-ip');
        const portalMode = document.getElementById('portal-mode');
        const statusMessage = document.getElementById('status-message');
        const sensorStatus = document.getElementById('sensor-status');
        const tsLastSync = document.getElementById('ts-last-sync');
        const httpLastSync = document.getElementById('http-last-sync');

        const formatLastSync = (lastSyncMs) => {
            if (!lastSyncMs || !data.nowMs || data.nowMs < lastSyncMs) return 'Last sync: -';
            const elapsedSeconds = Math.floor((data.nowMs - lastSyncMs) / 1000);
            if (elapsedSeconds < 60) return `Last sync: ${elapsedSeconds}s ago`;
            return `Last sync: ${Math.floor(elapsedSeconds / 60)}m ago`;
        };

        if (co2Value) {
            co2Value.textContent = (data.co2Ppm || 0) + ' ppm';
            co2Value.className = 'card-value accent' + (data.sensorConnected ? '' : ' warn');
        }
        if (tempValue) {
            tempValue.textContent = data.climateValid ? ((data.temperatureC || 0).toFixed(1) + ' C') : '-';
            tempValue.className = 'card-value ok' + (data.climateValid ? '' : ' warn');
        }
        if (humValue) {
            humValue.textContent = data.climateValid ? ((data.humidityPct || 0).toFixed(1) + ' %') : '-';
            humValue.className = 'card-value ok' + (data.climateValid ? '' : ' warn');
        }
        if (wifiValue) wifiValue.textContent = data.wifiConnected ? 'Connected' : (data.setupMode ? 'Setup AP' : 'Offline');
        if (ipValue) ipValue.textContent = data.ipAddress || data.apAddress || '-';
        if (cloudValue) cloudValue.textContent = data.cloudStatus || 'Idle';
        if (deviceName) deviceName.textContent = data.deviceName || '';
        if (deviceIp) deviceIp.textContent = data.ipAddress || data.apAddress || '-';
        if (portalMode) portalMode.textContent = data.setupMode ? 'AP Setup' : 'Online';
        if (tsLastSync) tsLastSync.textContent = formatLastSync(data.thingSpeakLastSyncMs || 0);
        if (httpLastSync) httpLastSync.textContent = formatLastSync(data.customHttpLastSyncMs || 0);
        if (sensorStatus) {
            if (data.sensorError) {
                sensorStatus.textContent = data.sensorError;
            } else {
                sensorStatus.textContent = 'Sensor ready';
            }
        }

        if (data.webMessage) {
            clearFirmwareUploadState();
            setStatusMessage(data.webMessage, 'success');
        } else if (firmwareUploadPending) {
            const firmwareChanged = currentFirmwareVersion && currentFirmwareVersion !== (data.firmwareVersion || '')
                || currentFirmwareBuildDate && currentFirmwareBuildDate !== (data.firmwareBuildDate || '');

            if (firmwareUploadApiDownSeen || firmwareChanged) {
                clearFirmwareUploadState();
                clearStatusMessage();
            }
        } else {
            clearStatusMessage();
        }
    } catch (error) {
        if (firmwareUploadPending) {
            firmwareUploadApiDownSeen = true;
        }
        console.warn(error);
    }
}

const firmwareUploadForm = document.getElementById('firmware-upload-form');
if (firmwareUploadForm) {
    firmwareUploadForm.addEventListener('submit', uploadFirmware);
}

refreshLiveData();
setInterval(refreshLiveData, 1000);
</script></body></html>)HTML";
    return html;
}


}  // namespace webpage

