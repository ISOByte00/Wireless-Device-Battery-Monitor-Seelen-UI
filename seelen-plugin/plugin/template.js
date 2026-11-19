
if (typeof batteryHub !== "string") {
  return "Veri bekleniyor";
}

let data;
try {
  data = JSON.parse(batteryHub);
} catch {
  return "Veri okunamadı";
}

if (!Array.isArray(data.devices) || !data.devices.length) {
  return "Cihaz yok";
}

const iconForKind = {
  mouse: "LuMouse",
  keyboard: "LuKeyboard",
  headset: "LuHeadphones",
  gamepad: "LuGamepad2",
  phone: "LuSmartphone",
  microphone: "LuMic",
};

const colorFor = (device) => {
  if (!device.connected || device.asleep || device.percent < 0) return "currentColor";
  if (device.charging) return "#3b82f6";
  if (device.percent < 20) return "#ef4444";
  if (device.percent <= 85) return "#eab308";
  return "#22c55e";
};

return Group({
  content: data.devices.filter((device) => device && typeof device === "object").map((device) => Button({
    tooltip: device.tooltip || device.name || "BatteryHub",
    content: [
      icon(iconForKind[device.kind] || "LuBatteryMedium"),
      String(device.percent >= 0 ? device.percent : "—") + (device.percent >= 0 ? "%" : ""),
      device.label ? " " + device.label : "",
    ],
    onClick: 'open("batteryhub://show-menu")',
    style: {
      display: "flex",
      alignItems: "center",
      gap: "0.2rem",
      color: colorFor(device),
      opacity: device.dim ? 0.55 : 1,
      whiteSpace: "nowrap",
    },
  })),
  style: {
    display: "flex",
    alignItems: "center",
    gap: "0.65rem",
    padding: "0 var(--item-padding)",
  },
});
