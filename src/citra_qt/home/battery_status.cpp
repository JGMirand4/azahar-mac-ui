// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include "citra_qt/home/battery_status.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#endif

namespace Home {

#ifdef __APPLE__

bool QueryBattery(int& percent, bool& charging) {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (info == nullptr) {
        return false;
    }
    CFArrayRef sources = IOPSCopyPowerSourcesList(info);
    if (sources == nullptr) {
        CFRelease(info);
        return false;
    }

    bool found = false;
    const CFIndex count = CFArrayGetCount(sources);
    for (CFIndex i = 0; i < count && !found; ++i) {
        CFDictionaryRef description =
            IOPSGetPowerSourceDescription(info, CFArrayGetValueAtIndex(sources, i));
        if (description == nullptr) {
            continue;
        }

        const auto type =
            static_cast<CFStringRef>(CFDictionaryGetValue(description, CFSTR(kIOPSTypeKey)));
        if (type == nullptr ||
            CFStringCompare(type, CFSTR(kIOPSInternalBatteryType), 0) != kCFCompareEqualTo) {
            continue;
        }

        int current = 0;
        int maximum = 0;
        const auto current_ref = static_cast<CFNumberRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSCurrentCapacityKey)));
        const auto maximum_ref = static_cast<CFNumberRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSMaxCapacityKey)));
        if (current_ref == nullptr || maximum_ref == nullptr ||
            !CFNumberGetValue(current_ref, kCFNumberIntType, &current) ||
            !CFNumberGetValue(maximum_ref, kCFNumberIntType, &maximum) || maximum <= 0) {
            continue;
        }

        percent = (current * 100) / maximum;
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }

        const auto charging_ref = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(description, CFSTR(kIOPSIsChargingKey)));
        charging = charging_ref != nullptr && CFBooleanGetValue(charging_ref);
        found = true;
    }

    CFRelease(sources);
    CFRelease(info);
    return found;
}

#else

bool QueryBattery(int& percent, bool& charging) {
    percent = 0;
    charging = false;
    return false;
}

#endif

} // namespace Home
