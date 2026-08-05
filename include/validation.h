#pragma once

#include <Arduino.h>
#include <ctype.h>

#include "app_config.h"

namespace Validation {
inline String trim(const String& val) {
    int start = 0;
    int end = val.length() - 1;
    while (start <= end && isspace(static_cast<unsigned char>(val[start]))) start++;
    while (end >= start && isspace(static_cast<unsigned char>(val[end]))) end--;
    return val.substring(start, end + 1);
}

inline bool isValidSensorReadInterval(unsigned long val) {
    return val >= appconfig::kSensorReadIntervalMinMs;
}

inline bool isValidSensorAltitude(unsigned long val) {
    return val >= appconfig::kSensorAltitudeMinMeters && val <= appconfig::kSensorAltitudeMaxMeters;
}

inline bool isValidCloudSendIntervalSeconds(unsigned long val) {
    return val >= 15UL;
}

inline bool parseUnsignedLongStrict(const String& rawValue, unsigned long& outValue) {
    String value = trim(rawValue);
    if (value.length() == 0) {
        return false;
    }

    unsigned long parsed = 0;
    for (size_t i = 0; i < value.length(); ++i) {
        const char ch = value[i];
        if (ch < '0' || ch > '9') {
            return false;
        }

        const unsigned long digit = static_cast<unsigned long>(ch - '0');
        if (parsed > (ULONG_MAX - digit) / 10UL) {
            return false;
        }
        parsed = parsed * 10UL + digit;
    }

    outValue = parsed;
    return true;
}

inline bool parseBoolStrict(const String& rawValue, bool& outValue) {
    String value = trim(rawValue);
    value.toLowerCase();

    if (value == "1" || value == "true" || value == "on" || value == "yes") {
        outValue = true;
        return true;
    }

    if (value == "0" || value == "false" || value == "off" || value == "no") {
        outValue = false;
        return true;
    }

    return false;
}

inline bool isValidHttpMethod(const String& rawMethod) {
    String method = trim(rawMethod);
    method.toUpperCase();
    return method == "GET" || method == "POST" || method == "PUT" || method == "PATCH";
}

inline String normalizeHttpMethod(const String& rawMethod) {
    String method = trim(rawMethod);
    method.toUpperCase();
    if (!isValidHttpMethod(method)) {
        return String(appconfig::kDefaultCustomHttpMethod);
    }
    return method;
}
}  // namespace Validation
