#pragma once

#include <Arduino.h>

#include "app_types.h"

namespace settings {
void begin();
SettingsData get();
void apply(const SettingsData &value);
bool load();
bool save();
}
