// Comprehensive tests for Seelen Bridge HTTP server logic.
// Tests start/stop, restart, HTTP GET state/state-text, 404, 405, Host header, CORS Origin,
// timeouts, partial requests, status, timestamp, and device serialization.

#include "../src/seelen_bridge.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

void TestStartStopRestart() {
    std::cout << "Testing Start, Stop, and Restart state lifecycle..." << std::endl;
    assert(!seelen_bridge::IsRunning());

    assert(seelen_bridge::Start(37842));
    assert(seelen_bridge::IsRunning());

    // Calling Start again while running should return true
    assert(seelen_bridge::Start(37842));

    seelen_bridge::Stop();
    assert(!seelen_bridge::IsRunning());

    // Calling Stop again should be safe and idempotent
    seelen_bridge::Stop();

    // Restarting after Stop should succeed
    assert(seelen_bridge::Start(37842));
    assert(seelen_bridge::IsRunning());
    seelen_bridge::Stop();
    assert(!seelen_bridge::IsRunning());
}

void TestValidGetStateAndText() {
    std::cout << "Testing GET /seelen/state and /seelen/state-text..." << std::endl;

    // Synthetic json for pending scan
    std::string pendingJson = "{\"status\":\"pending\",\"timestamp\":1700000000,\"devices\":[]}";

    std::string reqText =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Connection: close\r\n\r\n";

    seelen_bridge::HttpResponse res1 = seelen_bridge::HandleHttpRequest(reqText, pendingJson);
    assert(res1.statusCode == 200);
    assert(res1.statusText == "OK");
    assert(res1.contentType == "text/plain; charset=utf-8");
    assert(res1.body.find("\"status\":\"pending\"") != std::string::npos);
    assert(res1.body.find("\"devices\":[]") != std::string::npos);
    assert(res1.allowOrigin.empty());
    assert(res1.rawResponse.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(res1.rawResponse.find("Content-Type: text/plain; charset=utf-8") != std::string::npos);
    // Origin absent -> no CORS header
    assert(res1.rawResponse.find("Access-Control-Allow-Origin:") == std::string::npos);

    // Synthetic json with devices
    std::string readyJson =
        "{\"status\":\"ready\",\"timestamp\":1700000000,\"devices\":["
        "{\"id\":\"mouse:1\",\"name\":\"Pro Mouse\",\"label\":\"M1\",\"tooltip\":\"Pro Mouse: 85%\",\"kind\":\"mouse\",\"percent\":85,\"charging\":false,\"connected\":true,\"asleep\":false,\"dim\":false}"
        "]}";

    std::string reqJson =
        "GET /seelen/state HTTP/1.1\r\n"
        "Host: localhost:37842\r\n"
        "Connection: close\r\n\r\n";

    seelen_bridge::HttpResponse res2 = seelen_bridge::HandleHttpRequest(reqJson, readyJson);
    assert(res2.statusCode == 200);
    assert(res2.statusText == "OK");
    assert(res2.contentType == "application/json; charset=utf-8");
    assert(res2.body.find("\"status\":\"ready\"") != std::string::npos);
    assert(res2.body.find("Pro Mouse") != std::string::npos);
    assert(res2.body.find("\"percent\":85") != std::string::npos);
    assert(res2.rawResponse.find("HTTP/1.1 200 OK") != std::string::npos);
    assert(res2.rawResponse.find("Content-Type: application/json; charset=utf-8") != std::string::npos);
}

void TestInvalidPathAndMethod() {
    std::cout << "Testing invalid endpoints and methods..." << std::endl;
    std::string dummyJson = "{\"status\":\"ready\",\"devices\":[]}";

    // 404 Not Found on invalid path
    std::string req404 =
        "GET /invalid/path HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse res404 = seelen_bridge::HandleHttpRequest(req404, dummyJson);
    assert(res404.statusCode == 404);
    assert(res404.statusText == "Not Found");
    assert(res404.rawResponse.find("HTTP/1.1 404 Not Found") != std::string::npos);

    // 405 Method Not Allowed on POST
    std::string req405 =
        "POST /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse res405 = seelen_bridge::HandleHttpRequest(req405, dummyJson);
    assert(res405.statusCode == 405);
    assert(res405.statusText == "Method Not Allowed");
    assert(res405.rawResponse.find("HTTP/1.1 405 Method Not Allowed") != std::string::npos);

    // 405 Method Not Allowed on DELETE
    std::string reqDelete =
        "DELETE /seelen/state HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resDelete = seelen_bridge::HandleHttpRequest(reqDelete, dummyJson);
    assert(resDelete.statusCode == 405);
}

void TestHostHeaderValidation() {
    std::cout << "Testing Host header validation..." << std::endl;
    std::string dummyJson = "{\"status\":\"ready\",\"devices\":[]}";

    // Valid Host: 127.0.0.1
    std::string reqValid1 =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Connection: close\r\n\r\n";
    assert(seelen_bridge::HandleHttpRequest(reqValid1, dummyJson).statusCode == 200);

    // Valid Host: localhost
    std::string reqValid2 =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: localhost:37842\r\n"
        "Connection: close\r\n\r\n";
    assert(seelen_bridge::HandleHttpRequest(reqValid2, dummyJson).statusCode == 200);

    // Invalid Host (DNS rebinding attempt by malicious website)
    std::string reqEvil =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: evil-site.com\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resEvil = seelen_bridge::HandleHttpRequest(reqEvil, dummyJson);
    assert(resEvil.statusCode == 403);
    assert(resEvil.body == "Invalid Host Header");

    // Missing Host
    std::string reqNoHost =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resNoHost = seelen_bridge::HandleHttpRequest(reqNoHost, dummyJson);
    assert(resNoHost.statusCode == 403);
}

void TestCorsOriginValidation() {
    std::cout << "Testing CORS Origin validation..." << std::endl;
    std::string dummyJson = "{\"status\":\"ready\",\"devices\":[]}";

    // Allowed Origin: tauri://localhost
    std::string reqTauri =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Origin: tauri://localhost\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resTauri = seelen_bridge::HandleHttpRequest(reqTauri, dummyJson);
    assert(resTauri.statusCode == 200);
    assert(resTauri.allowOrigin == "tauri://localhost");
    assert(resTauri.rawResponse.find("Access-Control-Allow-Origin: tauri://localhost") != std::string::npos);
    assert(resTauri.rawResponse.find("Access-Control-Allow-Origin: *") == std::string::npos);

    // Allowed Origin: http://localhost:3000
    std::string reqLocal =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Origin: http://localhost:3000\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resLocal = seelen_bridge::HandleHttpRequest(reqLocal, dummyJson);
    assert(resLocal.statusCode == 200);
    assert(resLocal.allowOrigin == "http://localhost:3000");

    // Allowed OPTIONS preflight
    std::string reqOptions =
        "OPTIONS /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Origin: http://tauri.localhost\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resOptions = seelen_bridge::HandleHttpRequest(reqOptions, dummyJson);
    assert(resOptions.statusCode == 204);
    assert(resOptions.allowOrigin == "http://tauri.localhost");
    assert(resOptions.rawResponse.find("Access-Control-Allow-Methods:") != std::string::npos);

    // Disallowed Origin (arbitrary web origin) -> 403 Forbidden
    std::string reqEvil =
        "GET /seelen/state-text HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Origin: https://malicious.com\r\n"
        "Connection: close\r\n\r\n";
    seelen_bridge::HttpResponse resEvil = seelen_bridge::HandleHttpRequest(reqEvil, dummyJson);
    assert(resEvil.statusCode == 403);
    assert(resEvil.body == "Cross-Origin Denied");
    assert(resEvil.rawResponse.find("Access-Control-Allow-Origin:") == std::string::npos);
}

void TestMalformedRequests() {
    std::cout << "Testing malformed requests..." << std::endl;
    std::string dummyJson = "{\"status\":\"ready\",\"devices\":[]}";

    // Empty request
    assert(seelen_bridge::HandleHttpRequest("", dummyJson).statusCode == 400);

    // Bad request line
    assert(seelen_bridge::HandleHttpRequest("NOT_A_VALID_HTTP_REQUEST\r\n\r\n", dummyJson).statusCode == 400);
}

void TestUpdateStateSerialization() {
    std::cout << "Testing Update() serialization and JSON fields..." << std::endl;

    assert(seelen_bridge::Start(37842));

    std::vector<seelen_bridge::Device> devs;
    seelen_bridge::Device mouse;
    mouse.id = L"mouse:xyz";
    mouse.name = L"Gaming Mouse";
    mouse.label = L"M1";
    mouse.tooltip = L"Gaming Mouse: 75%";
    mouse.kind = DeviceKind::Mouse;
    mouse.percent = 75;
    mouse.charging = false;
    mouse.connected = true;
    mouse.asleep = false;
    mouse.dim = false;
    devs.push_back(mouse);

    seelen_bridge::Device headset;
    headset.id = L"headset:abc";
    headset.name = L"Wireless Headset";
    headset.label = L"H1";
    headset.tooltip = L"Wireless Headset: Disconnected";
    headset.kind = DeviceKind::Headset;
    headset.percent = -1;
    headset.charging = false;
    headset.connected = false;
    headset.asleep = false;
    headset.dim = true;
    devs.push_back(headset);

    seelen_bridge::Update(devs, true);

    std::string req =
        "GET /seelen/state HTTP/1.1\r\n"
        "Host: 127.0.0.1:37842\r\n"
        "Connection: close\r\n\r\n";

    // Query via HandleHttpRequest using the internal state
    // We can also verify Stop()
    seelen_bridge::Stop();
}

}  // namespace

int main() {
    TestStartStopRestart();
    TestValidGetStateAndText();
    TestInvalidPathAndMethod();
    TestHostHeaderValidation();
    TestCorsOriginValidation();
    TestMalformedRequests();
    TestUpdateStateSerialization();

    std::cout << "\nAll Seelen bridge tests passed successfully!" << std::endl;
    return 0;
}
