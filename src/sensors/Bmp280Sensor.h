#pragma once

class Bmp280Sensor {
public:
    void begin(unsigned long startupDelayMs);
    void loop();

private:
    bool ensureWireInitialized();
    bool tryInit(const char* reason);
    bool readSample(float& temperatureC, float& pressureHpa);

    bool ready_ = false;
    bool wireInitialized_ = false;
    unsigned long nextInitAttemptMs_ = 0;
};
