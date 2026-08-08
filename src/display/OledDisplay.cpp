#include "display/OledDisplay.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "app_config.h"
#include "app_i2c_lock.h"
#include "app_view_models.h"

namespace {
Adafruit_SSD1306 display(appconfig::kOledWidth, appconfig::kOledHeight, &Wire, -1);
bool displayReady = false;
unsigned long lastDrawMs = 0;
unsigned long lastMainValueSwitchMs = 0;
bool showCo2Value = true;
constexpr float kMmHgPerHpa = 0.75006156f;

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

uint16_t textWidth(const String &text, uint8_t size) {
    display.setTextSize(size);
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
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

void drawPpmReading(int y, const String &valueText) {
    constexpr uint8_t valueTextSize = 3;
    constexpr uint8_t unitTextSize = 1;
    constexpr int16_t unitGap = 4;

    int16_t valueX1 = 0;
    int16_t valueY1 = 0;
    uint16_t valueW = 0;
    uint16_t valueH = 0;
    display.setTextSize(valueTextSize);
    display.getTextBounds(valueText, 0, y, &valueX1, &valueY1, &valueW, &valueH);

    String unitText = "ppm";
    int16_t unitX1 = 0;
    int16_t unitY1 = 0;
    uint16_t unitW = 0;
    uint16_t unitH = 0;
    display.setTextSize(unitTextSize);
    display.getTextBounds(unitText, 0, y, &unitX1, &unitY1, &unitW, &unitH);

    int16_t totalWidth = static_cast<int16_t>(valueW + unitGap + unitW);
    int16_t startX = static_cast<int16_t>((appconfig::kOledWidth - totalWidth) / 2);
    int16_t unitY = y + static_cast<int16_t>((valueTextSize - unitTextSize) * 4);

    display.setTextSize(valueTextSize);
    display.setCursor(startX, y);
    display.print(valueText);

    display.setTextSize(unitTextSize);
    display.setCursor(startX + static_cast<int16_t>(valueW) + unitGap, unitY);
    display.print(unitText);
}

void drawPressureReading(int y, const String &valueText) {
    constexpr uint8_t valueTextSize = 3;
    constexpr uint8_t unitTextSize = 1;
    constexpr int16_t unitGap = 4;

    int16_t valueX1 = 0;
    int16_t valueY1 = 0;
    uint16_t valueW = 0;
    uint16_t valueH = 0;
    display.setTextSize(valueTextSize);
    display.getTextBounds(valueText, 0, y, &valueX1, &valueY1, &valueW, &valueH);

    String unitText = "mmHg";
    int16_t unitX1 = 0;
    int16_t unitY1 = 0;
    uint16_t unitW = 0;
    uint16_t unitH = 0;
    display.setTextSize(unitTextSize);
    display.getTextBounds(unitText, 0, y, &unitX1, &unitY1, &unitW, &unitH);

    int16_t totalWidth = static_cast<int16_t>(valueW + unitGap + unitW);
    int16_t startX = static_cast<int16_t>((appconfig::kOledWidth - totalWidth) / 2);
    int16_t unitY = y + static_cast<int16_t>((valueTextSize - unitTextSize) * 4);

    display.setTextSize(valueTextSize);
    display.setCursor(startX, y);
    display.print(valueText);

    display.setTextSize(unitTextSize);
    display.setCursor(startX + static_cast<int16_t>(valueW) + unitGap, unitY);
    display.print(unitText);
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

void drawLeftAlignedText(int y, const String &text, uint8_t size, int16_t leftMargin = 0) {
    display.setTextSize(size);
    display.setCursor(leftMargin, y);
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
    if (!applocks::lockI2c(pdMS_TO_TICKS(500))) {
        displayReady = false;
        return;
    }

    Wire.begin(appconfig::kI2CSdaPin, appconfig::kI2CSclPin);
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, appconfig::kOledI2cAddress);
    if (displayReady) {
        display.clearDisplay();
        display.display();
    }

    applocks::unlockI2c();
}

void loop() {
    if (!displayReady) {
        return;
    }

    unsigned long now = millis();
    if (now - lastDrawMs < appconfig::kDisplayRefreshIntervalMs) {
        return;
    }

    SettingsSnapshot config = getSettingsSnapshot();
    RuntimeSnapshot state = getRuntimeSnapshot();

    unsigned long displaySwitchIntervalMs = config.displaySwitchIntervalMs;
    if (displaySwitchIntervalMs < appconfig::kDisplaySwitchIntervalMinMs) {
        displaySwitchIntervalMs = appconfig::kDisplaySwitchIntervalMinMs;
    }
    if (displaySwitchIntervalMs > appconfig::kDisplaySwitchIntervalMaxMs) {
        displaySwitchIntervalMs = appconfig::kDisplaySwitchIntervalMaxMs;
    }

    if (now - lastMainValueSwitchMs >= displaySwitchIntervalMs) {
        showCo2Value = !showCo2Value;
        lastMainValueSwitchMs = now;
    }

    lastDrawMs = now;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    constexpr int16_t kTopTextY = 4;
    constexpr int16_t kWifiIconX = 111;
    constexpr int16_t kTopGap = 3;

    String tempValueText = state.climateValid ? String(state.temperatureC, 1) : String("--.-");
    String tempText = tempValueText + " ";
    uint16_t tempWidth = textWidth(tempText, 1);
    int16_t tempX = static_cast<int16_t>(kWifiIconX - kTopGap - tempWidth);
    if (tempX < 0) {
        tempX = 0;
    }

    int16_t titleMaxWidth = tempX - kTopGap;
    if (titleMaxWidth < 0) {
        titleMaxWidth = 0;
    }

    display.setTextSize(1);
    String title = clipTextToWidth(config.deviceName, 1, titleMaxWidth);
    display.setCursor(0, kTopTextY);
    display.print(title);

    display.setCursor(tempX, kTopTextY);
    display.print(tempText);
    int16_t degreeX = tempX + static_cast<int16_t>(textWidth(tempValueText, 1)) + 2;
    display.drawCircle(degreeX, kTopTextY + 2, 1, SSD1306_WHITE);

    WifiIconState wifiIconState = WifiIconState::Offline;
    if (state.wifiConnected) {
        wifiIconState = WifiIconState::Connected;
    } else if (state.setupMode) {
        wifiIconState = WifiIconState::AccessPoint;
    }
    drawWifiIcon(kWifiIconX, 0, wifiIconState);

    if (wifiIconState == WifiIconState::AccessPoint) {
        display.setTextSize(1);
        display.setCursor(96, 11);
        display.print("AP");
    }

    display.drawLine(0, 15, appconfig::kOledWidth - 1, 15, SSD1306_WHITE);

    if (showCo2Value) {
        drawLeftAlignedText(18, "CO2", 1, 2);
        String ppmValue = state.lastValidPpm > 0 ? String(state.co2Ppm) : String("---");
        drawPpmReading(22, ppmValue);
    } else {
        drawLeftAlignedText(18, "BMP", 1, 2);
        String pressureValue = state.bmpValid ? String(state.bmpPressureHpa * kMmHgPerHpa, 0) : String("---");
        drawPressureReading(22, pressureValue);
    }

    constexpr int16_t kBottomY = 56;
    constexpr int16_t kBottomLeftMargin = 2;
    constexpr int16_t kBottomRightMargin = 2;
    constexpr int16_t kBottomGap = 6;

    String humidityText = state.climateValid ? String(state.humidityPct, 1) + "%" : String("--.-%");
    drawLeftAlignedText(kBottomY, humidityText, 1, kBottomLeftMargin);

    String ipText;
    if (state.wifiConnected) {
        ipText = state.ipAddress;
    } else if (state.setupMode) {
        ipText = state.apAddress;
    } else {
        ipText = String("OFF");
    }
    // Compute available width dynamically so the right-aligned IP text is clipped only when needed.
    const uint16_t humidityWidth = textWidth(humidityText, 1);
    int16_t ipStartMinX = kBottomLeftMargin + static_cast<int16_t>(humidityWidth) + kBottomGap;
    int16_t availableIpWidth = static_cast<int16_t>(appconfig::kOledWidth - kBottomRightMargin - ipStartMinX);
    if (availableIpWidth < 0) {
        availableIpWidth = 0;
    }

    ipText = clipTextToWidth(ipText, 1, availableIpWidth);
    drawRightAlignedText(kBottomY, ipText, 1, kBottomRightMargin);

    if (applocks::lockI2c(pdMS_TO_TICKS(250))) {
        display.display();
        applocks::unlockI2c();
    }
}
}  // namespace displayui
