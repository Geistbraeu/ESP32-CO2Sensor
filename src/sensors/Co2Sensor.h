#pragma once

#include <Arduino.h>

namespace sensor {
void begin();
void loop();
bool calibrateZero(String &error);
}
