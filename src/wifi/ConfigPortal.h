#pragma once

#include <Arduino.h>

namespace wifiportal {
void begin();
void loop();
bool isSetupMode();
void requestReconfigure();
}
