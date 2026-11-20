const openMenu = 'open("batteryhub://show-menu")';
const messages = {
  en: { missing: "BatteryHub not found", waiting: "Waiting for data", empty: "No devices found", disconnected: "Disconnected", error: "Data error", syncing: "Waiting for synchronization", tray: "In tray mode", disabled: "Seelen integration is disabled" },
  de: { missing: "BatteryHub nicht gefunden", waiting: "Daten werden erwartet", empty: "Keine Geräte gefunden", disconnected: "Verbindung getrennt", error: "Datenfehler", syncing: "Synchronisierung wird erwartet", tray: "Im Infobereich-Modus", disabled: "Seelen-Integration ist deaktiviert" },
  es: { missing: "No se encontró BatteryHub", waiting: "Esperando datos", empty: "No se encontraron dispositivos", disconnected: "Desconectado", error: "Error de datos", syncing: "Esperando sincronización", tray: "En modo bandeja", disabled: "La integración con Seelen está desactivada" },
  fr: { missing: "BatteryHub introuvable", waiting: "En attente des données", empty: "Aucun appareil trouvé", disconnected: "Déconnecté", error: "Erreur de données", syncing: "En attente de synchronisation", tray: "En mode zone de notification", disabled: "L’intégration Seelen est désactivée" },
  pl: { missing: "Nie znaleziono BatteryHub", waiting: "Oczekiwanie na dane", empty: "Nie znaleziono urządzeń", disconnected: "Rozłączono", error: "Błąd danych", syncing: "Oczekiwanie na synchronizację", tray: "W trybie zasobnika", disabled: "Integracja z Seelen jest wyłączona" },
  "pt-BR": { missing: "BatteryHub não encontrado", waiting: "Aguardando dados", empty: "Nenhum dispositivo encontrado", disconnected: "Desconectado", error: "Erro de dados", syncing: "Aguardando sincronização", tray: "No modo de bandeja", disabled: "A integração com o Seelen está desativada" },
  tr: { missing: "BatteryHub bulunamadı", waiting: "Veri bekleniyor", empty: "Cihaz bulunamadı", disconnected: "Bağlantı kesildi", error: "Veri hatası", syncing: "Senkronizasyon bekleniyor", tray: "Tray modunda", disabled: "Seelen entegrasyonu kapalı" },
  ru: { missing: "BatteryHub не найден", waiting: "Ожидание данных", empty: "Устройства не найдены", disconnected: "Соединение разорвано", error: "Ошибка данных", syncing: "Ожидание синхронизации", tray: "Режим области уведомлений", disabled: "Интеграция с Seelen отключена" },
  uk: { missing: "BatteryHub не знайдено", waiting: "Очікування даних", empty: "Пристроїв не знайдено", disconnected: "З’єднання розірвано", error: "Помилка даних", syncing: "Очікування синхронізації", tray: "Режим області сповіщень", disabled: "Інтеграцію з Seelen вимкнено" },
  he: { missing: "BatteryHub לא נמצא", waiting: "ממתין לנתונים", empty: "לא נמצאו התקנים", disconnected: "מנותק", error: "שגיאת נתונים", syncing: "ממתין לסנכרון", tray: "במצב מגש", disabled: "השילוב עם Seelen מושבת" },
  ar: { missing: "لم يتم العثور على BatteryHub", waiting: "في انتظار البيانات", empty: "لم يتم العثور على أجهزة", disconnected: "غير متصل", error: "خطأ في البيانات", syncing: "في انتظار المزامنة", tray: "في وضع علبة النظام", disabled: "تكامل Seelen معطّل" },
  ja: { missing: "BatteryHub が見つかりません", waiting: "データを待っています", empty: "デバイスが見つかりません", disconnected: "切断されました", error: "データエラー", syncing: "同期を待っています", tray: "トレイモードです", disabled: "Seelen 連携は無効です" },
  "zh-CN": { missing: "未找到 BatteryHub", waiting: "正在等待数据", empty: "未找到设备", disconnected: "连接已断开", error: "数据错误", syncing: "正在等待同步", tray: "当前为托盘模式", disabled: "Seelen 集成已关闭" },
  "zh-TW": { missing: "找不到 BatteryHub", waiting: "正在等待資料", empty: "找不到裝置", disconnected: "連線已中斷", error: "資料錯誤", syncing: "正在等待同步", tray: "目前為系統匣模式", disabled: "Seelen 整合已關閉" },
  ko: { missing: "BatteryHub를 찾을 수 없음", waiting: "데이터 기다리는 중", empty: "장치를 찾을 수 없음", disconnected: "연결 끊김", error: "데이터 오류", syncing: "동기화 기다리는 중", tray: "트레이 모드", disabled: "Seelen 통합이 꺼져 있음" },
};
const languageCodes = Object.keys(messages);
let activeLanguage = "en";
const normalizeLanguage = (value) => {
  if (typeof value !== "string") return "en";
  const lower = value.trim().toLowerCase();
  return languageCodes.find((code) => code.toLowerCase() === lower) || "en";
};
const message = (key) => (messages[activeLanguage] || messages.en)[key] || messages.en[key];
const isRtl = () => activeLanguage === "ar" || activeLanguage === "he";
const statusButton = (text) => Button({
  tooltip: "BatteryHub",
  content: text,
  onClick: openMenu,
  style: { display: "flex", alignItems: "center", whiteSpace: "nowrap", opacity: 0.85, direction: isRtl() ? "rtl" : "ltr", unicodeBidi: "plaintext" },
});

if (typeof batteryHub !== "string" || !batteryHub.trim()) {
  return statusButton(message("missing"));
}

let data;
try {
  data = JSON.parse(batteryHub);
} catch {
  return statusButton(message("error"));
}

if (!data || typeof data !== "object" || Array.isArray(data)) {
  return statusButton(message("error"));
}

activeLanguage = normalizeLanguage(data.language);

// Date is a reactive Seelen scope. It forces another evaluation after a failed
// remoteData fetch leaves the last successful response cached by the toolbar.
const renderTick = date;
const now = Date.now();
const heartbeat = typeof data.heartbeat === "number" && Number.isFinite(data.heartbeat)
  ? data.heartbeat
  : null;
if (heartbeat !== null && now - heartbeat > 90000) {
  return statusButton(message("disconnected"));
}

if (data.status === "error") {
  return statusButton(message("error"));
}

const settings = data.settings && typeof data.settings === "object" && !Array.isArray(data.settings)
  ? data.settings
  : {};
if (settings.seelenIntegration === false) {
  return statusButton(message("disabled"));
}
if (settings.displayMode === "tray") {
  return statusButton(message("tray"));
}
if (data.status === "pending") {
  return statusButton(message("waiting"));
}
if (data.status !== "ready" || !Array.isArray(data.devices)) {
  return statusButton(message("error"));
}

const timestamp = typeof data.timestamp === "number" && Number.isFinite(data.timestamp)
  ? data.timestamp
  : null;
const pollIntervalMs = typeof settings.pollIntervalMs === "number" && Number.isFinite(settings.pollIntervalMs)
  ? Math.max(30000, settings.pollIntervalMs)
  : 60000;
if (timestamp !== null && now - timestamp > pollIntervalMs + 60000) {
  return statusButton(message("syncing"));
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

  return {
    name,
    label,
    tooltip: rawTooltip || name,
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
  return statusButton(message("empty"));
}

const monochrome = settings.monochrome === true;
const stacked = settings.stacked === true;
const colorFor = (device) => {
  if (monochrome) return "#ffffff";
  if (!device.connected || device.asleep || !device.hasValidPercent) return "currentColor";
  if (device.charging) return "#3b82f6";
  if (device.percent < 20) return "#ef4444";
  if (device.percent <= 85) return "#eab308";
  return "#22c55e";
};

const detailFor = (device) => [
  device.hasValidPercent ? (device.percent + "%") : "—",
  device.label ? (" " + device.label) : "",
  monochrome && device.charging ? icon("LuZap", 12) : null,
];

return Group({
  content: validDevices.map((device) => Button({
    tooltip: device.tooltip,
    content: stacked
      ? [
          icon(iconForKind[device.kind] || "LuBatteryMedium", 14),
          Group({
            content: detailFor(device),
            style: { display: "flex", alignItems: "center", gap: "0.15rem", lineHeight: 1, direction: "ltr", unicodeBidi: "isolate" },
          }),
        ]
      : [icon(iconForKind[device.kind] || "LuBatteryMedium"), ...detailFor(device)],
    onClick: openMenu,
    style: {
      display: "flex",
      flexDirection: stacked ? "column" : "row",
      justifyContent: "center",
      alignItems: "center",
      gap: stacked ? "0.05rem" : "0.25rem",
      color: colorFor(device),
      opacity: device.dim ? 0.55 : 1,
      whiteSpace: "nowrap",
      maxWidth: "140px",
      lineHeight: 1,
      direction: "ltr",
      unicodeBidi: "isolate",
    },
  })),
  style: {
    display: "flex",
    alignItems: "center",
    gap: "0.5rem",
    padding: "0 var(--item-padding)",
    direction: "ltr",
  },
});
