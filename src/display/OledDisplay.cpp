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

enum class WifiIconState {
    Connected,
    AccessPoint,
    Offline,
};

String clipTextToWidth(const String &value, uint8_t size, int16_t maxWidth) {
    String result = value;
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    while (result.length() > 0) {
        display.setTextSize(size);
        display.getTextBounds(result, 0, 0, &x1, &y1, &w, &h);
        if (w <= static_cast<uint16_t>(maxWidth)) {
            return result;
        }
        result.remove(result.length() - 1);
    }

    return result;
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

void drawRightAlignedText(int y, const String &text, uint8_t size, int16_t rightMargin = 0) {
    display.setTextSize(size);
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
    int16_t x = static_cast<int16_t>(appconfig::kOledWidth - rightMargin - w);
    if (x < 0) {
        x = 0;
    }
    display.setCursor(x, y);
    display.print(text);
}

void drawWifiIcon(int x, int y, WifiIconState state) {
    constexpr int iconWidth = 16;
    constexpr int iconHeight = 16;
    const int centerX = x + (iconWidth / 2);
    const int centerY = y + 8;

    display.drawCircle(centerX, centerY, 7, SSD1306_WHITE);
    display.drawCircle(centerX, centerY, 5, SSD1306_WHITE);
    display.drawCircle(centerX, centerY, 3, SSD1306_WHITE);
    display.fillRect(x, centerY + 1, iconWidth, iconHeight - (centerY - y) - 1, SSD1306_BLACK);

    if (state == WifiIconState::Offline) {
        display.drawLine(x + 1, y + 2, x + iconWidth - 2, y + iconHeight - 2, SSD1306_WHITE);
        display.drawLine(x + 1, y + 4, x + iconWidth - 4, y + iconHeight - 4, SSD1306_WHITE);
        return;
    }

    display.fillCircle(centerX, y + 13, 1, SSD1306_WHITE);

    if (state == WifiIconState::AccessPoint) {
        display.drawLine(centerX, y + 11, centerX, y + 15, SSD1306_WHITE);
        display.drawPixel(centerX, y + 10, SSD1306_WHITE);
    }
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
    String title = clipTextToWidth(config.deviceName, 1, 90);
    display.setCursor(0, 4);
    display.print(title);

    WifiIconState wifiIconState = WifiIconState::Offline;
    if (state.wifiConnected) {
        wifiIconState = WifiIconState::Connected;
    } else if (state.setupMode) {
        wifiIconState = WifiIconState::AccessPoint;
    }
    drawWifiIcon(111, 0, wifiIconState);

    if (wifiIconState == WifiIconState::AccessPoint) {
        display.setTextSize(1);
        display.setCursor(96, 11);
        display.print("AP");
    }

    display.drawLine(0, 15, appconfig::kOledWidth - 1, 15, SSD1306_WHITE);

    String ppmValue = state.lastValidPpm > 0 ? String(state.co2Ppm) : String("---");
    String ppmText = ppmValue + " ppm";
    drawCenteredText(22, ppmText, 3);

    String ipText;
    if (state.wifiConnected) {
        ipText = state.ipAddress;
    } else if (state.setupMode) {
        ipText = state.apAddress;
    } else {
        ipText = String("OFF");
    }
    ipText = clipTextToWidth(ipText, 1, appconfig::kOledWidth - 2);
    drawRightAlignedText(56, ipText, 1, 2);

    display.display();
}
}  // namespace displayui
