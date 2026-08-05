#include "app_i2c_lock.h"

namespace {
SemaphoreHandle_t gI2cMutex = nullptr;
}

namespace applocks {
void initI2cMutex() {
    if (gI2cMutex == nullptr) {
        gI2cMutex = xSemaphoreCreateMutex();
    }
}

bool lockI2c(TickType_t timeoutTicks) {
    return gI2cMutex != nullptr && xSemaphoreTake(gI2cMutex, timeoutTicks) == pdTRUE;
}

void unlockI2c() {
    if (gI2cMutex != nullptr) {
        xSemaphoreGive(gI2cMutex);
    }
}
}  // namespace applocks
