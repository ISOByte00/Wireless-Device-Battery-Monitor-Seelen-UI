#pragma once

#include <string>
#include <vector>

#include "devices.h"

// Optional, localhost-only state feed for the Seelen UI toolbar plugin.
// It never polls hardware: callers publish the results already obtained by BatteryHub.
namespace seelen_bridge {

struct Device {
    std::wstring id;
    std::wstring name;
    std::wstring label;
    std::wstring tooltip;
    DeviceKind kind = DeviceKind::Other;
    int percent = -1;
    bool charging = false;
    bool connected = true;
    bool asleep = false;
    bool dim = false;
};

bool Start(unsigned short port = 37842);
void Stop();
void Update(const std::vector<Device>& devices);

}  // namespace seelen_bridge
