#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace applocks {
void initI2cMutex();
bool lockI2c(TickType_t timeoutTicks = pdMS_TO_TICKS(300));
void unlockI2c();
}  // namespace applocks
