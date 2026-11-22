import http from "node:http";
import fs from "node:fs";
import path from "node:path";

const PORT = Number(process.env.PORT || 3000);
const DATA_DIR = process.env.DATA_DIR || "/app/data";
const DATA_FILE = path.join(DATA_DIR, "devices.json");
const CHECKIN_SECRET = process.env.CHECKIN_SECRET || "smartdesk-2026";
const ADMIN_KEY = process.env.ADMIN_KEY || "";
const MAX_DEVICES = Number(process.env.MAX_DEVICES || 10000);

fs.mkdirSync(DATA_DIR, { recursive: true });

/** @type {Record<string, {ip: string, mdns?: string, fw?: string, updatedAt: number}>} */
let devices = {};
try {
  if (fs.existsSync(DATA_FILE)) {
    devices = JSON.parse(fs.readFileSync(DATA_FILE, "utf8"));
  }
} catch (err) {
  console.error("[Store] Failed to load devices.json, starting fresh:", err.message);
  devices = {};
}

let saveTimer = null;
function flushSaveSync() {
  if (saveTimer) {
    clearTimeout(saveTimer);
    saveTimer = null;
  }
  try {
    const tmp = DATA_FILE + ".tmp";
    fs.writeFileSync(tmp, JSON.stringify(devices, null, 2), "utf8");
    fs.renameSync(tmp, DATA_FILE);
  } catch (err) {
    console.error("[Store] Failed to save devices.json:", err.message);
  }
}

function scheduleSave() {
  if (saveTimer) return;
  saveTimer = setTimeout(flushSaveSync, 500);
}

// Only allow private LAN IPv4 addresses (prevents open-redirect abuse)
function isPrivateIPv4(ip) {
  if (typeof ip !== "string") return false;
  const m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/.exec(ip.trim());
  if (!m) return false;
  const [a, b, c, d] = m.slice(1).map(Number);
  if ([a, b, c, d].some((n) => n < 0 || n > 255)) return false;
  if (a === 10) return true;
  if (a === 192 && b === 168) return true;
  if (a === 172 && b >= 16 && b <= 31) return true;
  return false;
}

function isValidDeviceId(id) {
  return typeof id === "string" && /^[A-Za-z0-9_-]{4,16}$/.test(id);
}

function escHtml(s) {
  return String(s || "").replace(/[&<>"']/g, (c) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;",
  }[c]));
}

function sendJson(res, status, obj) {
  const body = JSON.stringify(obj);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(body);
}

function renderRedirectPage(id, entry) {
  const safeId = escHtml(id);
  const safeIp = escHtml(entry.ip);
  const targetUrl = `http://${safeIp}/`;
  const mdnsHost = entry.mdns && /^[a-z0-9-]+\.local$/i.test(entry.mdns) ? escHtml(entry.mdns) : "";
  const mdnsUrl = mdnsHost ? `http://${mdnsHost}/` : "";
  const minsAgo = Math.max(0, Math.round((Date.now() - entry.updatedAt) / 60000));
  const seenText = minsAgo < 1 ? "vừa xong" : minsAgo < 60 ? `${minsAgo} phút trước` : `${Math.round(minsAgo / 60)} giờ trước`;

  return `<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD SmartDesk — Đang kết nối...</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#000;color:#fff;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
  .card{width:100%;max-width:380px;background:#0c1218;border:1px solid #264a5e;border-radius:20px;padding:28px 22px;text-align:center}
  .badge{display:inline-block;font-size:11px;font-weight:700;letter-spacing:.08em;color:#64d2ff;background:rgba(100,210,255,.12);border:1px solid rgba(100,210,255,.35);padding:4px 10px;border-radius:999px;margin-bottom:14px}
  h1{font-size:20px;margin-bottom:8px}
  p{font-size:13px;color:#aab2be;line-height:1.55}
  .spinner{width:34px;height:34px;border:3px solid rgba(100,210,255,.2);border-top-color:#64d2ff;border-radius:50%;margin:18px auto;animation:spin .8s linear infinite}
  @keyframes spin{to{transform:rotate(360deg)}}
  .help{display:none;margin-top:18px;padding-top:16px;border-top:1px solid rgba(255,255,255,.08);text-align:left}
  .help.show{display:block}
  .btn{display:block;width:100%;text-decoration:none;text-align:center;padding:12px 14px;border-radius:12px;font-weight:700;font-size:14px;margin-top:10px}
  .btn-primary{background:#64d2ff;color:#000}
  .btn-outline{background:transparent;color:#64d2ff;border:1px solid rgba(100,210,255,.45)}
  .meta{margin-top:14px;font-size:11px;color:#606672;text-align:center}
</style>
</head>
<body>
  <div class="card">
    <div class="badge">SMARTDESK • ${safeId}</div>
    <h1>Đang mở bảng điều khiển...</h1>
    <p>Đang chuyển hướng tới SmartDesk tại <b>${safeIp}</b></p>
    <div class="spinner" id="sp"></div>
    <div class="help" id="help">
      <p style="color:#fff;font-weight:600;margin-bottom:6px">⚠️ Chưa vào được thiết bị?</p>
      <p>• Hãy đảm bảo điện thoại đang kết nối <b>cùng mạng Wi-Fi</b> với SmartDesk (tắt 4G/5G hoặc VPN nếu đang bật).<br>• Kiểm tra SmartDesk đang có nguồn điện.</p>
      <a class="btn btn-primary" href="${targetUrl}">🔄 Mở trực tiếp (${safeIp})</a>
      ${mdnsUrl ? `<a class="btn btn-outline" href="${mdnsUrl}">📡 Thử qua mDNS (${mdnsHost})</a>` : ""}
    </div>
    <div class="meta">Cập nhật IP lần cuối: ${escHtml(seenText)}</div>
  </div>
  <script>
    setTimeout(function(){ window.location.href = ${JSON.stringify(targetUrl)}; }, 80);
    setTimeout(function(){
      document.getElementById('sp').style.display = 'none';
      document.getElementById('help').classList.add('show');
    }, 2600);
  </script>
</body>
</html>`;
}

function renderNotFoundPage(id) {
  const safeId = escHtml(id);
  return `<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD SmartDesk — Chưa kích hoạt</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#000;color:#fff;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
  .card{width:100%;max-width:380px;background:#0c1218;border:1px solid #264a5e;border-radius:20px;padding:28px 22px;text-align:center}
  .badge{display:inline-block;font-size:11px;font-weight:700;color:#ebbe5f;background:rgba(235,190,95,.12);border:1px solid rgba(235,190,95,.35);padding:4px 10px;border-radius:999px;margin-bottom:14px}
  h1{font-size:19px;margin-bottom:10px}
  p{font-size:13px;color:#aab2be;line-height:1.6;text-align:left}
  .btn{display:block;width:100%;text-decoration:none;text-align:center;padding:12px;border-radius:12px;font-weight:700;font-size:14px;margin-top:16px;background:#64d2ff;color:#000}
</style>
</head>
<body>
  <div class="card">
    <div class="badge">SMARTDESK • ${safeId}</div>
    <h1>Thiết bị chưa kết nối Wi-Fi</h1>
    <p>1. Cấp nguồn cho CYD SmartDesk.<br>2. Nếu lần đầu sử dụng, hãy quét mã <b>QR Wi-Fi</b> hiển thị trên màn hình máy để kết nối Wi-Fi.<br>3. Sau khi máy lên mạng (~10 giây), chạm lại thẻ NFC hoặc bấm nút bên dưới.</p>
    <a class="btn" href="javascript:location.reload()">🔄 Kiểm tra lại</a>
  </div>
</body>
</html>`;
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  const pathname = url.pathname;

  if (req.method === "GET" && pathname === "/health") {
    return sendJson(res, 200, { status: "ok", devices: Object.keys(devices).length });
  }

  if (req.method === "POST" && pathname === "/api/checkin") {
    const key = req.headers["x-api-key"] || url.searchParams.get("key") || "";
    if (CHECKIN_SECRET && key !== CHECKIN_SECRET) {
      return sendJson(res, 401, { ok: false, error: "unauthorized" });
    }

    let raw = "";
    req.on("data", (chunk) => {
      raw += chunk;
      if (raw.length > 2048) req.destroy();
    });
    req.on("end", () => {
      try {
        const body = JSON.parse(raw || "{}");
        const id = String(body.id || "").trim().toUpperCase();
        const ip = String(body.ip || "").trim();
        const mdns = body.mdns ? String(body.mdns).trim() : undefined;
        const fw = body.fw ? String(body.fw).trim() : undefined;

        if (!isValidDeviceId(id)) {
          return sendJson(res, 400, { ok: false, error: "invalid_id" });
        }
        if (!isPrivateIPv4(ip)) {
          return sendJson(res, 400, { ok: false, error: "invalid_lan_ip" });
        }

        if (!devices[id]) {
          const keys = Object.keys(devices);
          if (keys.length >= MAX_DEVICES) {
            let oldestKey = keys[0];
            for (const k of keys) {
              if ((devices[k].updatedAt || 0) < (devices[oldestKey].updatedAt || 0)) {
                oldestKey = k;
              }
            }
            delete devices[oldestKey];
          }
        }
        devices[id] = { ip, mdns, fw, updatedAt: Date.now() };
        scheduleSave();
        console.log(`[Checkin] ${id} -> ${ip}${mdns ? ` (${mdns})` : ""}`);
        return sendJson(res, 200, { ok: true });
      } catch {
        return sendJson(res, 400, { ok: false, error: "invalid_json" });
      }
    });
    return;
  }

  if (req.method === "GET" && pathname.startsWith("/d/")) {
    let id = "";
    try {
      id = decodeURIComponent(pathname.slice(3)).replace(/\/+$/, "").toUpperCase();
    } catch {
      res.writeHead(400, { "Content-Type": "text/plain; charset=utf-8" });
      return res.end("Invalid Device ID");
    }
    if (!isValidDeviceId(id)) {
      res.writeHead(400, { "Content-Type": "text/plain; charset=utf-8" });
      return res.end("Invalid Device ID");
    }
    const entry = devices[id];
    const html = entry ? renderRedirectPage(id, entry) : renderNotFoundPage(id);
    res.writeHead(200, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store, no-cache, must-revalidate",
    });
    return res.end(html);
  }

  if (req.method === "GET" && pathname === "/admin/devices") {
    const key = url.searchParams.get("key") || req.headers["x-admin-key"] || "";
    if (!ADMIN_KEY || key !== ADMIN_KEY) {
      return sendJson(res, 403, { ok: false, error: "forbidden" });
    }
    return sendJson(res, 200, { count: Object.keys(devices).length, devices });
  }

  res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
  res.end("CYD SmartDesk NFC Redirector");
});

server.listen(PORT, "0.0.0.0", () => {
  console.log(`[SmartDesk NFC Redirector] Listening on :${PORT} (data: ${DATA_FILE})`);
});

for (const sig of ["SIGTERM", "SIGINT"]) {
  process.on(sig, () => {
    flushSaveSync();
    server.close(() => process.exit(0));
  });
}
