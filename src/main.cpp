#include "app_config.h"
#include "app_i2c_lock.h"
#include "app_state.h"
#include "cloud/CloudManager.h"
#include "display/OledDisplay.h"
#include "sensors/Co2Sensor.h"
#include "settings/Settings.h"
#include "web/web_server.h"
#include "wifi/ConfigPortal.h"

AppState gAppState;

namespace {
constexpr BaseType_t kNetworkCore = 0;
constexpr BaseType_t kDeviceCore = 1;

void networkTask(void *parameter) {
  (void)parameter;
  for (;;) {
    wifiportal::loop();
    webui::loop();
    cloudmanager::loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void sensorTask(void *parameter) {
  (void)parameter;
  for (;;) {
    sensor::loop();
    SettingsData config = settings::get();
    unsigned long delayMs = config.sensorReadIntervalMs;
    if (delayMs < appconfig::kSensorReadIntervalMinMs) {
      delayMs = appconfig::kSensorReadIntervalMinMs;
    }
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  }
}

void displayTask(void *parameter) {
  (void)parameter;
  for (;;) {
    displayui::loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  if (gAppState.dataMutex == nullptr) {
    gAppState.dataMutex = xSemaphoreCreateMutex();
  }
  if (gAppState.settingsMutex == nullptr) {
    gAppState.settingsMutex = xSemaphoreCreateMutex();
  }

  applocks::initI2cMutex();

  pinMode(appconfig::kBuzzerPin, OUTPUT);
  digitalWrite(appconfig::kBuzzerPin, LOW);

  settings::begin();
  wifiportal::begin();
  webui::begin(wifiportal::isSetupMode());
  sensor::begin();
  displayui::begin();
  cloudmanager::begin();

  xTaskCreatePinnedToCore(networkTask, "networkTask", 6144, nullptr, 2, nullptr, kNetworkCore);
  xTaskCreatePinnedToCore(sensorTask, "sensorTask", 6144, nullptr, 2, nullptr, kDeviceCore);
  xTaskCreatePinnedToCore(displayTask, "displayTask", 6144, nullptr, 2, nullptr, kDeviceCore);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}