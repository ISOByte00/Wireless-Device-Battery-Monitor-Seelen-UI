if (typeof batteryHub !== "string" || !batteryHub.trim()) {
  return "Veri bekleniyor";
}

let data;
try {
  data = JSON.parse(batteryHub);
} catch {
  return "Veri okunamadı";
}

if (!data || typeof data !== "object" || Array.isArray(data)) {
  return "Veri okunamadı";
}

if (data.status === "pending") {
  return "Veri bekleniyor";
}

if (!Array.isArray(data.devices)) {
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

const sanitizeDevice = (d) => {
  if (!d || typeof d !== "object" || Array.isArray(d)) return null;

  const rawPercent = d.percent;
  const hasValidPercent = typeof rawPercent === "number" && Number.isFinite(rawPercent) && rawPercent >= 0 && rawPercent <= 100;
  const percent = hasValidPercent ? Math.round(rawPercent) : -1;

  const connected = d.connected !== false;
  const asleep = Boolean(d.asleep);
  const charging = Boolean(d.charging) && connected && !asleep && percent < 100;
  const dim = Boolean(d.dim) || !connected || !hasValidPercent;

  const rawKind = typeof d.kind === "string" ? d.kind.toLowerCase().trim() : "other";
  const kind = iconForKind[rawKind] ? rawKind : "other";

  const rawName = typeof d.name === "string" ? d.name.trim() : "";
  const name = rawName || "BatteryHub";

  const rawLabel = typeof d.label === "string" ? d.label.trim() : "";
  const label = rawLabel.length > 8 ? rawLabel.slice(0, 8) : rawLabel;

  const rawTooltip = typeof d.tooltip === "string" ? d.tooltip.trim() : "";
  const tooltip = rawTooltip || name;

  return {
    name,
    label,
    tooltip,
    kind,
    percent,
    hasValidPercent,
    charging,
    connected,
    asleep,
    dim,
  };
};

const validDevices = data.devices.map(sanitizeDevice).filter(Boolean);

if (!validDevices.length) {
  return "Cihaz yok";
}

const colorFor = (device) => {
  if (!device.connected || device.asleep || !device.hasValidPercent) return "currentColor";
  if (device.charging) return "#3b82f6";
  if (device.percent < 20) return "#ef4444";
  if (device.percent <= 85) return "#eab308";
  return "#22c55e";
};

return Group({
  content: validDevices.map((device) => Button({
    tooltip: device.tooltip,
    content: [
      icon(iconForKind[device.kind] || "LuBatteryMedium"),
      device.hasValidPercent ? (device.percent + "%") : "—",
      device.label ? (" " + device.label) : "",
    ],
    onClick: 'open("batteryhub://show-menu")',
    style: {
      display: "flex",
      alignItems: "center",
      gap: "0.25rem",
      color: colorFor(device),
      opacity: device.dim ? 0.55 : 1,
      whiteSpace: "nowrap",
      maxWidth: "140px",
    },
  })),
  style: {
    display: "flex",
    alignItems: "center",
    gap: "0.5rem",
    padding: "0 var(--item-padding)",
  },
});
