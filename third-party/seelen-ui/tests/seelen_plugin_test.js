// Automated test suite for Seelen UI BatteryHub template.js
// Tests JSON parsing, validation, fallback handling, edge cases, boundaries, and rendering.

const fs = require("fs");
const path = require("path");
const assert = require("assert");

const templatePath = path.join(__dirname, "../plugin/plugin/template.js");
const templateCode = fs.readFileSync(templatePath, "utf-8");

// Mock environment for Seelen UI
function runTemplate(batteryHub) {
  const icon = (name) => ({ type: "icon", name });
  const Button = (props) => ({ type: "button", ...props });
  const Group = (props) => ({ type: "group", ...props });

  const fn = new Function("batteryHub", "icon", "Button", "Group", templateCode);
  return fn(batteryHub, icon, Button, Group);
}

console.log("Running Seelen Plugin template tests...\n");

// 1. Non-string inputs
assert.strictEqual(runTemplate(null), "Veri bekleniyor", "null input should return Veri bekleniyor");
assert.strictEqual(runTemplate(undefined), "Veri bekleniyor", "undefined input should return Veri bekleniyor");
assert.strictEqual(runTemplate(""), "Veri bekleniyor", "empty string should return Veri bekleniyor");
assert.strictEqual(runTemplate("   "), "Veri bekleniyor", "whitespace string should return Veri bekleniyor");

// 2. Malformed JSON
assert.strictEqual(runTemplate("{ bad json"), "Veri okunamadı", "Malformed JSON should return Veri okunamadı");
assert.strictEqual(runTemplate("null"), "Veri okunamadı", "null JSON should return Veri okunamadı");
assert.strictEqual(runTemplate("123"), "Veri okunamadı", "numeric JSON should return Veri okunamadı");
assert.strictEqual(runTemplate('"hello"'), "Veri okunamadı", "quoted string JSON should return Veri okunamadı");
assert.strictEqual(runTemplate("[]"), "Veri okunamadı", "array JSON should return Veri okunamadı");

// 3. Status pending
assert.strictEqual(runTemplate(JSON.stringify({ status: "pending", devices: [] })), "Veri bekleniyor", "status pending should return Veri bekleniyor");

// 4. Empty devices
assert.strictEqual(runTemplate(JSON.stringify({ devices: [] })), "Cihaz yok", "Empty devices array should return Cihaz yok");
assert.strictEqual(runTemplate(JSON.stringify({ devices: null })), "Cihaz yok", "null devices should return Cihaz yok");
assert.strictEqual(runTemplate(JSON.stringify({})), "Cihaz yok", "missing devices should return Cihaz yok");

// 5. Corrupted devices in array (fault tolerance)
const mixedPayload = JSON.stringify({
  devices: [
    null,
    "not an object",
    123,
    [],
    { id: "valid-1", name: "Mouse", kind: "mouse", percent: 80, connected: true }
  ]
});
const mixedResult = runTemplate(mixedPayload);
assert.strictEqual(mixedResult.type, "group", "Group should be returned when at least one valid device exists");
assert.strictEqual(mixedResult.content.length, 1, "Only 1 valid device should be extracted from corrupted list");
assert.strictEqual(mixedResult.content[0].content[1], "80%", "Percent should be 80%");

// 6. Battery level boundary values
function testPercent(percent, expectedColor, expectedText) {
  const payload = JSON.stringify({
    devices: [{ id: "d1", name: "Test Device", kind: "mouse", percent, connected: true, asleep: false, charging: false }]
  });
  const res = runTemplate(payload);
  assert.strictEqual(res.type, "group");
  const btn = res.content[0];
  assert.strictEqual(btn.style.color, expectedColor, `Percent ${percent} expected color ${expectedColor}, got ${btn.style.color}`);
  assert.strictEqual(btn.content[1], expectedText, `Percent ${percent} expected text ${expectedText}, got ${btn.content[1]}`);
}

// 0% -> red
testPercent(0, "#ef4444", "0%");
// 19% -> red
testPercent(19, "#ef4444", "19%");
// 20% -> yellow
testPercent(20, "#eab308", "20%");
// 85% -> yellow
testPercent(85, "#eab308", "85%");
// 86% -> green
testPercent(86, "#22c55e", "86%");
// 100% -> green
testPercent(100, "#22c55e", "100%");

// 7. Unknown / Invalid percentage
function testInvalidPercent(val) {
  const payload = JSON.stringify({
    devices: [{ id: "d1", name: "Test Device", kind: "mouse", percent: val, connected: true, asleep: false }]
  });
  const res = runTemplate(payload);
  const btn = res.content[0];
  assert.strictEqual(btn.content[1], "—", `Invalid percent ${val} should render as em-dash`);
  assert.strictEqual(btn.style.color, "currentColor", `Invalid percent ${val} must NOT be colored red; should be currentColor`);
}

testInvalidPercent(-1);
testInvalidPercent(-10);
testInvalidPercent(105);
testInvalidPercent(NaN);
testInvalidPercent(Infinity);
testInvalidPercent("50");
testInvalidPercent(null);
testInvalidPercent(undefined);

// 8. Device states: Charging, Asleep, Offline
// Charging (blue)
const chargingPayload = JSON.stringify({
  devices: [{ id: "d1", name: "Mouse", kind: "mouse", percent: 45, connected: true, charging: true, asleep: false }]
});
const chargingRes = runTemplate(chargingPayload);
assert.strictEqual(chargingRes.content[0].style.color, "#3b82f6", "Charging device should be blue #3b82f6");

// Asleep (neutral currentColor)
const asleepPayload = JSON.stringify({
  devices: [{ id: "d1", name: "Mouse", kind: "mouse", percent: 10, connected: true, asleep: true }]
});
const asleepRes = runTemplate(asleepPayload);
assert.strictEqual(asleepRes.content[0].style.color, "currentColor", "Asleep device should be neutral currentColor");

// Offline / Disconnected (neutral currentColor and dimmed opacity)
const offlinePayload = JSON.stringify({
  devices: [{ id: "d1", name: "Headset", kind: "headset", percent: 90, connected: false }]
});
const offlineRes = runTemplate(offlinePayload);
assert.strictEqual(offlineRes.content[0].style.color, "currentColor", "Disconnected device should be currentColor");
assert.strictEqual(offlineRes.content[0].style.opacity, 0.55, "Disconnected device should be dimmed to 0.55 opacity");

// 9. Multiple devices of same kind with labels
const multiPayload = JSON.stringify({
  devices: [
    { id: "m1", name: "Mouse A", kind: "mouse", label: "Left", percent: 60, connected: true },
    { id: "m2", name: "Mouse B", kind: "mouse", label: "Right", percent: 70, connected: true }
  ]
});
const multiRes = runTemplate(multiPayload);
assert.strictEqual(multiRes.content.length, 2);
assert.strictEqual(multiRes.content[0].content[2], " Left");
assert.strictEqual(multiRes.content[1].content[2], " Right");

// 10. Long & Unicode device names
const unicodePayload = JSON.stringify({
  devices: [{
    id: "uni-1",
    name: "🎮 Türkçe Karakterli Özel Cihaz Adı — Çok Uzun Başlık 🌟",
    tooltip: "Detaylı Durum: %95 Pil | Şarj Olmuyor",
    kind: "gamepad",
    percent: 95,
    connected: true
  }]
});
const uniRes = runTemplate(unicodePayload);
assert.strictEqual(uniRes.content[0].tooltip, "Detaylı Durum: %95 Pil | Şarj Olmuyor");
assert.strictEqual(uniRes.content[0].content[0].name, "LuGamepad2");

// 11. Large number of devices (100+ devices stress test)
const largeList = [];
for (let i = 0; i < 150; i++) {
  largeList.push({
    id: `dev-${i}`,
    name: `Device ${i}`,
    kind: i % 2 === 0 ? "keyboard" : "mouse",
    percent: (i * 7) % 101,
    connected: true
  });
}
const largePayload = JSON.stringify({ devices: largeList });
const largeRes = runTemplate(largePayload);
assert.strictEqual(largeRes.content.length, 150, "All 150 devices should be rendered");

// 12. OnClick handler verification
assert.strictEqual(largeRes.content[0].onClick, 'open("batteryhub://show-menu")', "onClick must trigger batteryhub://show-menu");

console.log("All Seelen Plugin template tests passed successfully! (12/12 test suites passed)\n");
