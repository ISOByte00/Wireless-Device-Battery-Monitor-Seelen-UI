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
bool IsRunning();
void Update(const std::vector<Device>& devices, bool isInitialScanDone = true);

struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::string contentType;
    std::string allowOrigin;
    std::string body;
    std::string rawResponse;
};

HttpResponse HandleHttpRequest(const std::string& rawRequest, const std::string& currentJson);

}  // namespace seelen_bridge
