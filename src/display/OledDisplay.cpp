#include "display/OledDisplay.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "app_config.h"
#include "app_view_models.h"

namespace {
Adafruit_SSD1306 display(appconfig::kOledWidth, appconfig::kOledHeight, &Wire, -1);
bool displayReady = false;
unsigned long lastDrawMs = 0;

String clipText(const String &value, size_t maxLength) {
    if (value.length() <= maxLength) {
        return value;
    }
    return value.substring(0, maxLength);
}

void drawCenteredText(int y, const String &text, uint8_t size) {
    display.setTextSize(size);
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    int16_t x = static_cast<int16_t>((appconfig::kOledWidth - w) / 2);
    display.setCursor(x, y);
    display.print(text);
}
}  // namespace

namespace displayui {
void begin() {
    Wire.begin(appconfig::kI2CSdaPin, appconfig::kI2CSclPin);
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, appconfig::kOledI2cAddress);
    if (displayReady) {
        display.clearDisplay();
        display.display();
    }
}

void loop() {
    if (!displayReady) {
        return;
    }

    unsigned long now = millis();
    if (now - lastDrawMs < appconfig::kDisplayRefreshIntervalMs) {
        return;
    }
    lastDrawMs = now;

    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(clipText(config.deviceName, 21));

    display.setCursor(0, 8);
    if (state.wifiConnected) {
        display.print("IP: ");
        display.print(state.ipAddress);
    } else if (state.setupMode) {
        display.print("AP: ");
        display.print(state.apAddress);
    } else {
        display.print("WiFi connecting");
    }

    display.drawLine(0, 18, appconfig::kOledWidth - 1, 18, SSD1306_WHITE);

    String ppmText = state.lastValidPpm > 0 ? String(state.co2Ppm) : String("---");
    drawCenteredText(26, ppmText, 4);

    display.setTextSize(1);
    display.setCursor(0, 56);
    if (state.sensorConnected) {
        display.print("ppm");
    } else if (state.sensorError.length() > 0) {
        display.print(clipText(state.sensorError, 21));
    }

    display.display();
}
}  // namespace displayui
