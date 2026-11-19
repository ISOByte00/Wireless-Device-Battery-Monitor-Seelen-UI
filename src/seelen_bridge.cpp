#include "seelen_bridge.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <utility>

#include "json_output.h"

namespace seelen_bridge {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
SOCKET g_listener = INVALID_SOCKET;
HANDLE g_thread = nullptr;
bool g_started = false;
std::string g_json = "{\"devices\":[]}";

const char* KindName(DeviceKind kind) {
    switch (kind) {
        case DeviceKind::Mouse: return "mouse";
        case DeviceKind::Headset: return "headset";
        case DeviceKind::Keyboard: return "keyboard";
        case DeviceKind::Gamepad: return "gamepad";
        case DeviceKind::Phone: return "phone";
        case DeviceKind::Microphone: return "microphone";
        default: return "other";
    }
}

std::string MakeJson(const std::vector<Device>& devices) {
    std::string out = "{\"devices\":[";
    for (size_t i = 0; i < devices.size(); i++) {
        if (i) out += ',';
        const Device& device = devices[i];
        out += "{\"id\":\"" + JsonEscape(WideToUtf8(device.id));
        out += "\",\"name\":\"" + JsonEscape(WideToUtf8(device.name));
        out += "\",\"label\":\"" + JsonEscape(WideToUtf8(device.label));
        out += "\",\"tooltip\":\"" + JsonEscape(WideToUtf8(device.tooltip));
        out += "\",\"kind\":\"";
        out += KindName(device.kind);
        out += "\",\"percent\":" + std::to_string(device.percent);
        out += ",\"charging\":";
        out += device.charging ? "true" : "false";
        out += ",\"connected\":";
        out += device.connected ? "true" : "false";
        out += ",\"asleep\":";
        out += device.asleep ? "true" : "false";
        out += ",\"dim\":";
        out += device.dim ? "true" : "false";
        out += '}';
    }
    out += "]}";
    return out;
}

bool SendAll(SOCKET socket, const std::string& text) {
    const char* data = text.data();
    int remaining = (int)text.size();
    while (remaining > 0) {
        int sent = send(socket, data, remaining, 0);
        if (sent <= 0) return false;
        data += sent;
        remaining -= sent;
    }
    return true;
}

DWORD WINAPI ServerThread(LPVOID) {
    for (;;) {
        SOCKET client = accept(g_listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) return 0;

        char request[1024] = {};
        int read = recv(client, request, sizeof(request) - 1, 0);
        std::string requestText(request, read > 0 ? (size_t)read : 0);
        bool isJsonStateRequest = requestText.find("GET /seelen/state HTTP/") == 0;
        bool isTextStateRequest = requestText.find("GET /seelen/state-text HTTP/") == 0;
        bool isStateRequest = isJsonStateRequest || isTextStateRequest;

        std::string body;
        if (isStateRequest) {
            AcquireSRWLockShared(&g_lock);
            body = g_json;
            ReleaseSRWLockShared(&g_lock);
        } else {
            body = "{\"error\":\"not found\"}";
        }

        const char* status = isStateRequest ? "200 OK" : "404 Not Found";
        std::string response = "HTTP/1.1 ";
        response += status;
        response += isTextStateRequest
            ? "\r\nContent-Type: text/plain; charset=utf-8\r\n"
            : "\r\nContent-Type: application/json; charset=utf-8\r\n";
        response += "Access-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: ";
        response += std::to_string(body.size());
        response += "\r\n\r\n";
        response += body;
        SendAll(client, response);
        closesocket(client);
    }
}

}  // namespace

bool Start(unsigned short port) {
    if (g_started) return true;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    BOOL exclusive = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR || listen(listener, 2) == SOCKET_ERROR) {
        closesocket(listener);
        WSACleanup();
        return false;
    }

    g_listener = listener;
    g_thread = CreateThread(nullptr, 0, ServerThread, nullptr, 0, nullptr);
    if (!g_thread) {
        closesocket(g_listener);
        g_listener = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    g_started = true;
    return true;
}

void Stop() {
    if (!g_started) return;
    closesocket(g_listener);
    g_listener = INVALID_SOCKET;
    WaitForSingleObject(g_thread, 2000);
    CloseHandle(g_thread);
    g_thread = nullptr;
    g_started = false;
    WSACleanup();
}

void Update(const std::vector<Device>& devices) {
    if (!g_started) return;
    std::string json = MakeJson(devices);
    AcquireSRWLockExclusive(&g_lock);
    g_json = std::move(json);
    ReleaseSRWLockExclusive(&g_lock);
}

}  // namespace seelen_bridge
