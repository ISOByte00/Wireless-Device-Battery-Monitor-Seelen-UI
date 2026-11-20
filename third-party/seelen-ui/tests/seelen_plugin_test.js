// State-oriented mock tests for the Seelen UI BatteryHub toolbar template.
// These do not emulate Seelen's remoteData failure handling or actual window behavior.
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const template = fs.readFileSync(path.join(__dirname, "../plugin/plugin/template.js"), "utf8");
function runTemplate(batteryHub) {
  const icon = (name) => ({ type: "icon", name });
  const Button = (props) => ({ type: "button", ...props });
  const Group = (props) => ({ type: "group", ...props });
  return new Function("batteryHub", "icon", "Button", "Group", template)(batteryHub, icon, Button, Group);
}
const run = (data) => runTemplate(JSON.stringify(data));
const status = (result) => result.content[2];

assert.equal(status(runTemplate(undefined)), "BatteryHub erişilemiyor");
assert.equal(status(runTemplate(null)), "BatteryHub erişilemiyor");
assert.equal(status(runTemplate("")), "BatteryHub erişilemiyor");
assert.equal(status(runTemplate("   ")), "BatteryHub erişilemiyor");
assert.equal(status(runTemplate("{bad")), "Geçersiz yanıt");
assert.equal(status(runTemplate("null")), "Geçersiz yanıt");
assert.equal(status(runTemplate("[]")), "Geçersiz yanıt");
assert.equal(status(run({ status: "pending", devices: [] })), "Cihazlar taranıyor");
assert.equal(status(run({ status: "disabled", devices: [] })), "Entegrasyon kapalı");
assert.equal(status(run({ status: "error", devices: [] })), "Veri alınamıyor");
assert.equal(status(run({ status: "unavailable", devices: [] })), "Veri alınamıyor");
assert.equal(status(run({ status: "other", devices: [] })), "Bilinmeyen durum");
assert.equal(status(run({ status: "ready", devices: [] })), "Cihaz bulunamadı");
assert.equal(status(run({ status: "ready", devices: null })), "Cihaz verisi hatalı");
assert.equal(status(run({ status: "ready", devices: [null, 12] })), "Cihaz verisi hatalı");

const mixed = run({ status: "ready", devices: [null, 12, [], { name: "Mouse", kind: "mouse", percent: 80 }] });
assert.equal(mixed.type, "group");
assert.equal(mixed.content.length, 1);
assert.equal(mixed.content[0].content[1], "80%");

for (const [percent, expectedColor] of [
  [0, "#ef4444"], [19, "#ef4444"], [20, "#eab308"],
  [85, "#eab308"], [86, "#22c55e"], [100, "#22c55e"],
]) {
  const button = run({ status: "ready", devices: [{ kind: "mouse", percent }] }).content[0];
  assert.equal(button.style.color, expectedColor);
  assert.equal(button.content[1], percent + "%");
}
for (const percent of [-1, 105, null, "50"]) {
  const button = run({ status: "ready", devices: [{ percent }] }).content[0];
  assert.equal(button.content[1], "—");
  assert.equal(button.style.opacity, 0.55);
}
const disconnected = run({ status: "ready", devices: [{ kind: "headset", percent: 55, connected: false }] }).content[0];
assert.equal(disconnected.content[1], "—");
assert.match(disconnected.tooltip, /Bağlı değil/);

const asleep = run({ status: "ready", devices: [{ percent: 78, asleep: true }] }).content[0];
assert.equal(asleep.content[1], "—");

const charging = run({ status: "ready", devices: [{ percent: 78, charging: true }] }).content[0];
assert.equal(charging.style.color, "#3b82f6");
assert.equal(charging.onClick, 'open("batteryhub://show-menu")');

const multiple = run({ status: "ready", devices: [
  { kind: "mouse", percent: 60, label: "Left" },
  { kind: "keyboard", percent: 70, label: "Right" },
] });
assert.equal(multiple.content.length, 2);
assert.equal(multiple.content[0].content[2], " Left");
assert.equal(multiple.content[1].content[2], " Right");

const long = run({ status: "ready", devices: [{ name: "Türkçe isim", tooltip: "Özel tooltip", percent: 50, label: "abcdefgh123" }] }).content[0];
assert.equal(long.tooltip, "Özel tooltip");
assert.equal(long.content[2], " abcdefgh");

const devices = Array.from({ length: 150 }, (_, i) => ({ name: "Device " + i, percent: i % 101 }));
assert.equal(run({ status: "ready", devices }).content.length, 150);
assert.equal(runTemplate({ status: "ready", devices: [{ percent: 42 }] }).content[0].content[1], "42%");

console.log("All BatteryHub toolbar status tests passed");
