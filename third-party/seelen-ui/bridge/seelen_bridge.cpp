#include "seelen_bridge.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "json_output.h"

namespace seelen_bridge {
namespace {

constexpr size_t kMaxRequestBytes = 4096;
constexpr DWORD kSocketTimeoutMs = 2000;

SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic<bool> g_running{false};
std::atomic<SOCKET> g_listener{INVALID_SOCKET};
std::atomic<SOCKET> g_activeClient{INVALID_SOCKET};
HANDLE g_thread = nullptr;
std::string g_json = "{\"status\":\"pending\",\"devices\":[]}";

long long NowMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

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

std::string MakeJson(const std::vector<Device>& devices, const Settings& settings,
                     bool isInitialScanDone) {
    std::string out = "{\"status\":\"";
    out += (isInitialScanDone ? "ready" : "pending");
    out += "\",\"language\":\"";
    out += JsonEscape(WideToUtf8(settings.language));
    out += "\",\"timestamp\":";
    out += std::to_string(settings.lastScanTimestamp);
    out += ",\"settings\":{\"displayMode\":\"";
    out += settings.widgetMode ? "widget" : "tray";
    out += "\",\"stacked\":";
    out += settings.stacked ? "true" : "false";
    out += ",\"monochrome\":";
    out += settings.monochrome ? "true" : "false";
    out += ",\"seelenIntegration\":";
    out += settings.seelenIntegration ? "true" : "false";
    out += ",\"pollIntervalMs\":" + std::to_string(settings.pollIntervalMs);
    out += "},\"devices\":[";
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

std::string WithHeartbeat(const std::string& json) {
    if (json.empty() || json.back() != '}') return json;
    std::string out = json;
    out.pop_back();
    out += ",\"heartbeat\":" + std::to_string(NowMilliseconds()) + '}';
    return out;
}

bool SendAll(SOCKET socket, const std::string& text) {
    const char* data = text.data();
    int remaining = static_cast<int>(text.size());
    while (remaining > 0) {
        int sent = send(socket, data, remaining, 0);
        if (sent <= 0) return false;
        data += sent;
        remaining -= sent;
    }
    return true;
}

std::string ToLower(std::string str) {
    for (char& c : str) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return str;
}

bool StartsWith(const std::string& str, const std::string& prefix) {
    return str.rfind(prefix, 0) == 0;
}

bool IsAllowedOrigin(const std::string& origin) {
    if (origin.empty()) return true;  // non-browser/native client
    std::string o = ToLower(origin);

    return o == "http://tauri.localhost" || o == "https://tauri.localhost";
}

bool IsValidHost(const std::string& host) {
    if (host.empty()) return false;
    std::string h = ToLower(host);
    // Strip trailing whitespace
    while (!h.empty() && (h.back() == ' ' || h.back() == '\r' || h.back() == '\t')) h.pop_back();

    if (h == "127.0.0.1" || StartsWith(h, "127.0.0.1:")) return true;
    if (h == "localhost" || StartsWith(h, "localhost:")) return true;
    return false;
}

void ProcessClient(SOCKET client) {
    DWORD timeout = kSocketTimeoutMs;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    std::string rawRequest;
    char buffer[1024];
    bool headerEndFound = false;

    while (rawRequest.size() < kMaxRequestBytes) {
        int read = recv(client, buffer, sizeof(buffer), 0);
        if (read <= 0) break;
        rawRequest.append(buffer, read);
        if (rawRequest.find("\r\n\r\n") != std::string::npos || rawRequest.find("\n\n") != std::string::npos) {
            headerEndFound = true;
            break;
        }
    }

    if (!headerEndFound && rawRequest.size() >= kMaxRequestBytes) {
        std::string err = "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        SendAll(client, err);
        return;
    }

    if (rawRequest.empty()) return;

    std::string currentJson;
    AcquireSRWLockShared(&g_lock);
    currentJson = g_json;
    ReleaseSRWLockShared(&g_lock);

    HttpResponse response = HandleHttpRequest(rawRequest, currentJson);
    SendAll(client, response.rawResponse);
}

DWORD WINAPI ServerThread(LPVOID) {
    while (g_running.load()) {
        SOCKET listener = g_listener.load();
        if (listener == INVALID_SOCKET) break;

        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!g_running.load()) break;
            continue;
        }

        g_activeClient.store(client);
        ProcessClient(client);
        g_activeClient.store(INVALID_SOCKET);
        closesocket(client);
    }
    return 0;
}

}  // namespace

HttpResponse HandleHttpRequest(const std::string& rawRequest, const std::string& currentJson) {
    HttpResponse res;

    // Parse request line
    size_t lineEnd = rawRequest.find("\r\n");
    if (lineEnd == std::string::npos) lineEnd = rawRequest.find('\n');
    if (lineEnd == std::string::npos) {
        res.statusCode = 400;
        res.statusText = "Bad Request";
        res.rawResponse = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        return res;
    }

    std::string requestLine = rawRequest.substr(0, lineEnd);
    size_t firstSpace = requestLine.find(' ');
    size_t secondSpace = (firstSpace != std::string::npos) ? requestLine.find(' ', firstSpace + 1) : std::string::npos;
    if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
        res.statusCode = 400;
        res.statusText = "Bad Request";
        res.rawResponse = "HTTP/1.1 400 Bad Request\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        return res;
    }

    std::string method = requestLine.substr(0, firstSpace);
    std::string path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

    // Extract headers (Host and Origin)
    std::string host;
    std::string origin;
    size_t pos = lineEnd;
    while (pos < rawRequest.size()) {
        if (rawRequest[pos] == '\r') pos++;
        if (pos < rawRequest.size() && rawRequest[pos] == '\n') pos++;
        if (pos >= rawRequest.size()) break;
        if (rawRequest[pos] == '\r' || rawRequest[pos] == '\n') break;  // End of headers

        size_t nextEnd = rawRequest.find('\n', pos);
        if (nextEnd == std::string::npos) nextEnd = rawRequest.size();
        std::string headerLine = rawRequest.substr(pos, nextEnd - pos);
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();

        size_t colon = headerLine.find(':');
        if (colon != std::string::npos) {
            std::string headerName = ToLower(headerLine.substr(0, colon));
            std::string headerVal = headerLine.substr(colon + 1);
            size_t start = headerVal.find_first_not_of(" \t");
            if (start != std::string::npos) headerVal = headerVal.substr(start);
            size_t end = headerVal.find_last_not_of(" \t\r\n");
            if (end != std::string::npos) headerVal = headerVal.substr(0, end + 1);

            if (headerName == "host") host = headerVal;
            else if (headerName == "origin") origin = headerVal;
        }
        pos = nextEnd;
    }

    // Host header validation (Loopback DNS-Rebinding protection)
    if (!IsValidHost(host)) {
        res.statusCode = 403;
        res.statusText = "Forbidden";
        res.body = "Invalid Host Header";
        res.rawResponse = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\nContent-Length: " +
                          std::to_string(res.body.size()) + "\r\n\r\n" + res.body;
        return res;
    }

    // Origin header validation (CORS Security)
    bool hasOrigin = !origin.empty();
    if (hasOrigin && !IsAllowedOrigin(origin)) {
        res.statusCode = 403;
        res.statusText = "Forbidden";
        res.body = "Cross-Origin Denied";
        res.rawResponse = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\nContent-Length: " +
                          std::to_string(res.body.size()) + "\r\n\r\n" + res.body;
        return res;
    }

    // Only the toolbar's GET requests are supported.
    if (method != "GET") {
        res.statusCode = 405;
        res.statusText = "Method Not Allowed";
        res.rawResponse = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        return res;
    }

    bool isJsonStateRequest = (path == "/seelen/state");
    bool isTextStateRequest = (path == "/seelen/state-text");
    bool isStateRequest = isJsonStateRequest || isTextStateRequest;

    if (!isStateRequest) {
        res.statusCode = 404;
        res.statusText = "Not Found";
        res.contentType = "application/json; charset=utf-8";
        res.body = "{\"error\":\"not found\"}";
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Type: application/json; charset=utf-8\r\n";
        if (hasOrigin) {
            res.allowOrigin = origin;
            response += "Access-Control-Allow-Origin: " + origin + "\r\n";
            response += "Vary: Origin\r\n";
        }
        response += "Connection: close\r\nContent-Length: " + std::to_string(res.body.size()) + "\r\n\r\n" + res.body;
        res.rawResponse = std::move(response);
        return res;
    }

    res.statusCode = 200;
    res.statusText = "OK";
    res.contentType = isTextStateRequest ? "text/plain; charset=utf-8" : "application/json; charset=utf-8";
    res.body = WithHeartbeat(currentJson);

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + res.contentType + "\r\n";
    if (hasOrigin) {
        res.allowOrigin = origin;
        response += "Access-Control-Allow-Origin: " + origin + "\r\n";
        response += "Vary: Origin\r\n";
    }
    response += "Cache-Control: no-store, no-cache, must-revalidate\r\nConnection: close\r\nContent-Length: ";
    response += std::to_string(res.body.size());
    response += "\r\n\r\n";
    response += res.body;

    res.rawResponse = std::move(response);
    return res;
}

bool Start(unsigned short port) {
    if (g_running.load()) return true;
    if (g_thread) return false;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    BOOL exclusive = TRUE;
    if (setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR) {
        closesocket(listener);
        WSACleanup();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listener, 4) == SOCKET_ERROR) {
        closesocket(listener);
        WSACleanup();
        return false;
    }

    g_listener.store(listener);
    g_running.store(true);
    g_thread = CreateThread(nullptr, 0, ServerThread, nullptr, 0, nullptr);
    if (!g_thread) {
        g_running.store(false);
        closesocket(listener);
        g_listener.store(INVALID_SOCKET);
        WSACleanup();
        return false;
    }

    return true;
}

void Stop() {
    bool wasRunning = g_running.exchange(false);
    if (!wasRunning && !g_thread) return;

    if (wasRunning) {
        SOCKET listener = g_listener.exchange(INVALID_SOCKET);
        if (listener != INVALID_SOCKET) {
            closesocket(listener);
        }
    }

    SOCKET active = g_activeClient.load();
    if (active != INVALID_SOCKET) {
        shutdown(active, SD_BOTH);
    }

    if (!g_thread) return;
    DWORD wait = WaitForSingleObject(g_thread, 3000);
    if (wait != WAIT_OBJECT_0) return;
    CloseHandle(g_thread);
    g_thread = nullptr;
    WSACleanup();
}

bool IsRunning() {
    return g_running.load();
}

void Update(const std::vector<Device>& devices, const Settings& settings,
            bool isInitialScanDone) {
    std::string json = MakeJson(devices, settings, isInitialScanDone);
    AcquireSRWLockExclusive(&g_lock);
    g_json = std::move(json);
    ReleaseSRWLockExclusive(&g_lock);
}

}  // namespace seelen_bridge
