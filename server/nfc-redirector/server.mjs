import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";

const PORT = Number(process.env.PORT || 3000);
const DATA_DIR = process.env.DATA_DIR || "/app/data";
const FIRMWARE_DIR = path.join(DATA_DIR, "firmware");
const PUBLIC_DIR =
  process.env.PUBLIC_DIR ||
  path.join(
    path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")),
    "public"
  );
const GUIDE_FILE = path.join(PUBLIC_DIR, "guide.html");
const ADMIN_FILE = path.join(PUBLIC_DIR, "admin.html");
const DATA_FILE = path.join(DATA_DIR, "devices.json");
const RELEASES_FILE = path.join(DATA_DIR, "releases.json");
const CHECKIN_SECRET = process.env.CHECKIN_SECRET || "smartdesk-2026";
const ADMIN_KEY = process.env.ADMIN_KEY || CHECKIN_SECRET;
const PUBLIC_BASE_URL = (process.env.PUBLIC_BASE_URL || "https://smartdesk.fabx.vn").replace(/\/+$/, "");
const MAX_DEVICES = Number(process.env.MAX_DEVICES || 10000);
const MAX_FIRMWARE_BYTES = 2600000; // 2.6 MB max

fs.mkdirSync(DATA_DIR, { recursive: true });
fs.mkdirSync(FIRMWARE_DIR, { recursive: true });

/** @type {Record<string, any>} */
let devices = {};
try {
  if (fs.existsSync(DATA_FILE)) {
    devices = JSON.parse(fs.readFileSync(DATA_FILE, "utf8"));
  }
} catch (err) {
  console.error("[Store] Failed to load devices.json, starting fresh:", err.message);
  devices = {};
}

/** @type {Array<{id:string, version:string, hw:string, filename:string, size:number, sha256:string, changelog:string, beta:boolean, releasedAt:number}>} */
let releases = [];
try {
  if (fs.existsSync(RELEASES_FILE)) {
    const parsed = JSON.parse(fs.readFileSync(RELEASES_FILE, "utf8"));
    if (Array.isArray(parsed)) releases = parsed;
  }
} catch (err) {
  console.error("[Store] Failed to load releases.json, starting fresh:", err.message);
  releases = [];
}

let saveTimer = null;
function writeJsonAtomic(filePath, data) {
  const tmp = filePath + ".tmp";
  fs.writeFileSync(tmp, JSON.stringify(data, null, 2), "utf8");
  fs.renameSync(tmp, filePath);
}

function flushSaveSync() {
  if (saveTimer) {
    clearTimeout(saveTimer);
    saveTimer = null;
  }
  try {
    writeJsonAtomic(DATA_FILE, devices);
  } catch (err) {
    console.error("[Store] Failed to save devices.json:", err.message);
  }
}

function saveReleasesSync() {
  try {
    writeJsonAtomic(RELEASES_FILE, releases);
  } catch (err) {
    console.error("[Store] Failed to save releases.json:", err.message);
  }
}

function scheduleSave() {
  if (saveTimer) return;
  saveTimer = setTimeout(flushSaveSync, 500);
}

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

function isValidHw(hw) {
  return typeof hw === "string" && /^[a-z0-9._-]{2,32}$/i.test(hw);
}

function isValidVersion(ver) {
  return typeof ver === "string" && /^[0-9]+\.[0-9]+\.[0-9]+(-[a-z0-9.]+)?$/i.test(ver);
}

function compareSemver(a, b) {
  const pa = String(a || "0.0.0").split("-")[0].split(".").map((n) => parseInt(n, 10) || 0);
  const pb = String(b || "0.0.0").split("-")[0].split(".").map((n) => parseInt(n, 10) || 0);
  for (let i = 0; i < 3; i++) {
    if ((pa[i] || 0) !== (pb[i] || 0)) return (pa[i] || 0) - (pb[i] || 0);
  }
  return 0;
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

function sendJson(res, status, obj, extraHeaders = {}) {
  const body = JSON.stringify(obj);
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
    ...extraHeaders,
  });
  res.end(body);
}

function corsHeaders() {
  return {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, PATCH, DELETE, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, X-Admin-Key, X-Api-Key",
  };
}

function isAdminAuthorized(req, url) {
  const key = req.headers["x-admin-key"] || url.searchParams.get("key") || "";
  return Boolean(ADMIN_KEY && key === ADMIN_KEY);
}

function readJsonBody(req, maxBytes = 16384) {
  return new Promise((resolve, reject) => {
    let raw = "";
    let tooLarge = false;
    req.on("data", (chunk) => {
      raw += chunk;
      if (raw.length > maxBytes) {
        tooLarge = true;
        req.destroy();
      }
    });
    req.on("end", () => {
      if (tooLarge) return reject(new Error("payload_too_large"));
      try {
        resolve(JSON.parse(raw || "{}"));
      } catch {
        reject(new Error("invalid_json"));
      }
    });
    req.on("error", reject);
  });
}

function renderRedirectPage(id, entry) {
  const safeId = escHtml(id);
  const safeIp = escHtml(entry.ip);
  const targetUrl = `http://${safeIp}/`;
  const mdnsHost = entry.mdns && /^[a-z0-9-]+\.local$/i.test(entry.mdns) ? escHtml(entry.mdns) : "";
  const mdnsUrl = mdnsHost ? `http://${mdnsHost}/` : "";
  const minsAgo = Math.max(0, Math.round((Date.now() - (entry.updatedAt || Date.now())) / 60000));
  const seenText =
    minsAgo < 1 ? "vừa xong" : minsAgo < 60 ? `${minsAgo} phút trước` : `${Math.round(minsAgo / 60)} giờ trước`;

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
      <a class="btn btn-outline" href="/guide">📖 Xem Hướng dẫn sử dụng</a>
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
    <a class="btn" style="background:transparent;color:#64d2ff;border:1px solid rgba(100,210,255,.45);margin-top:10px" href="/guide">📖 Xem Hướng dẫn sử dụng</a>
  </div>
</body>
</html>`;
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  const pathname = url.pathname;

  if (req.method === "OPTIONS") {
    res.writeHead(204, corsHeaders());
    return res.end();
  }

  // ── Health check ──────────────────────────────────────────────
  if (req.method === "GET" && pathname === "/health") {
    return sendJson(res, 200, {
      status: "ok",
      devices: Object.keys(devices).length,
      releases: releases.length,
    });
  }

  // ── Device Check-in (ESP32 -> Server) ─────────────────────────
  if (req.method === "POST" && pathname === "/api/checkin") {
    const key = req.headers["x-api-key"] || url.searchParams.get("key") || "";
    if (CHECKIN_SECRET && key !== CHECKIN_SECRET) {
      return sendJson(res, 401, { ok: false, error: "unauthorized" });
    }
    try {
      const body = await readJsonBody(req, 4096);
      const id = String(body.id || "").trim().toUpperCase();
      const ip = String(body.ip || "").trim();
      const mdns = body.mdns ? String(body.mdns).trim() : undefined;
      const fw = body.fw ? String(body.fw).trim().slice(0, 24) : undefined;
      const hw = body.hw ? String(body.hw).trim().slice(0, 32) : undefined;
      const rssi = typeof body.rssi === "number" ? Math.round(body.rssi) : undefined;

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

      const prev = devices[id] || {};
      const now = Date.now();
      devices[id] = {
        ...prev,
        ip,
        mdns: mdns || prev.mdns,
        fw: fw || prev.fw,
        hw: hw || prev.hw || "nicemcu_2.8ips",
        rssi: rssi !== undefined ? rssi : prev.rssi,
        firstSeenAt: prev.firstSeenAt || now,
        updatedAt: now,
      };
      scheduleSave();
      console.log(`[Checkin] ${id} -> ${ip} (fw=${fw || "?"} hw=${hw || "?"} rssi=${rssi ?? "?"})`);
      return sendJson(res, 200, { ok: true });
    } catch (err) {
      return sendJson(res, 400, { ok: false, error: err.message || "invalid_json" });
    }
  }

  // ── Cloud OTA: Latest Firmware Manifest (Public + CORS) ───────
  if (req.method === "GET" && (pathname === "/api/firmware/latest" || pathname === "/api/firmware/latest.json")) {
    const hw = (url.searchParams.get("hw") || "nicemcu_2.8ips").trim();
    const id = (url.searchParams.get("id") || "").trim().toUpperCase();
    const allowBeta = Boolean(id && devices[id] && devices[id].beta);

    const candidates = releases.filter((r) => r.hw === hw && (allowBeta || !r.beta));
    candidates.sort((a, b) => compareSemver(b.version, a.version) || (b.releasedAt - a.releasedAt));
    const latest = candidates[0] || null;

    if (!latest) {
      return sendJson(res, 200, { ok: true, hw, release: null }, corsHeaders());
    }
    const proto = req.headers["x-forwarded-proto"] || "https";
    const host = req.headers.host || "";
    const base = PUBLIC_BASE_URL || `${proto}://${host}`;
    return sendJson(
      res,
      200,
      {
        ok: true,
        hw,
        version: latest.version,
        url: `${base}/firmware/${encodeURIComponent(latest.filename)}`,
        size: latest.size,
        sha256: latest.sha256,
        changelog: latest.changelog || "",
        beta: Boolean(latest.beta),
        releasedAt: latest.releasedAt,
      },
      corsHeaders()
    );
  }

  // ── Cloud OTA: Download Firmware Binary ───────────────────────
  if (req.method === "GET" && pathname.startsWith("/firmware/")) {
    const file = path.basename(decodeURIComponent(pathname.slice(10)));
    if (!/^[A-Za-z0-9._-]+\.bin$/.test(file)) {
      res.writeHead(400, { "Content-Type": "text/plain; charset=utf-8" });
      return res.end("Invalid firmware filename");
    }
    const filePath = path.join(FIRMWARE_DIR, file);
    if (!fs.existsSync(filePath)) {
      res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
      return res.end("Firmware not found");
    }
    const stat = fs.statSync(filePath);
    res.writeHead(200, {
      "Content-Type": "application/octet-stream",
      "Content-Length": stat.size,
      "Content-Disposition": `attachment; filename="${file}"`,
      "Cache-Control": "public, max-age=3600",
      ...corsHeaders(),
    });
    return fs.createReadStream(filePath).pipe(res);
  }

  // ── NFC Redirect Page ─────────────────────────────────────────
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
    const html = entry && entry.ip ? renderRedirectPage(id, entry) : renderNotFoundPage(id);
    res.writeHead(200, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store, no-cache, must-revalidate",
    });
    return res.end(html);
  }

  // ── Admin UI Page ─────────────────────────────────────────────
  if (req.method === "GET" && (pathname === "/admin" || pathname === "/admin/")) {
    try {
      const html = fs.readFileSync(ADMIN_FILE, "utf8");
      res.writeHead(200, {
        "Content-Type": "text/html; charset=utf-8",
        "Cache-Control": "no-store",
      });
      return res.end(html);
    } catch {
      res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
      return res.end("Admin UI not found");
    }
  }

  // ── Admin API (Protected by ADMIN_KEY) ────────────────────────
  if (pathname.startsWith("/api/admin/") || pathname === "/admin/devices") {
    if (!isAdminAuthorized(req, url)) {
      return sendJson(res, 403, { ok: false, error: "forbidden" }, corsHeaders());
    }

    if (req.method === "GET" && (pathname === "/api/admin/overview" || pathname === "/admin/devices")) {
      const now = Date.now();
      const list = Object.entries(devices).map(([id, d]) => ({ id, ...d }));
      const online24h = list.filter((d) => d.updatedAt && now - d.updatedAt < 86400000).length;
      const betaCount = list.filter((d) => d.beta).length;
      const fwCounts = {};
      for (const d of list) {
        const v = d.fw || "unknown";
        fwCounts[v] = (fwCounts[v] || 0) + 1;
      }
      return sendJson(
        res,
        200,
        {
          ok: true,
          stats: {
            total: list.length,
            online24h,
            betaCount,
            releases: releases.length,
            fwCounts,
          },
          devices,
          releases: [...releases].sort((a, b) => b.releasedAt - a.releasedAt),
        },
        corsHeaders()
      );
    }

    // PATCH /api/admin/devices/:id — create/update metadata (customer, orderId, soldAt, warrantyMonths, note, beta, hw)
    if (req.method === "PATCH" && pathname.startsWith("/api/admin/devices/")) {
      const id = decodeURIComponent(pathname.slice(19)).trim().toUpperCase();
      if (!isValidDeviceId(id)) {
        return sendJson(res, 400, { ok: false, error: "invalid_id" }, corsHeaders());
      }
      try {
        const body = await readJsonBody(req, 8192);
        const prev = devices[id] || { createdAt: Date.now() };
        const next = { ...prev };
        if (body.customer !== undefined) next.customer = String(body.customer).trim().slice(0, 80);
        if (body.phone !== undefined) next.phone = String(body.phone).trim().slice(0, 30);
        if (body.orderId !== undefined) next.orderId = String(body.orderId).trim().slice(0, 40);
        if (body.soldAt !== undefined) next.soldAt = String(body.soldAt).trim().slice(0, 20);
        if (body.warrantyMonths !== undefined) next.warrantyMonths = Number(body.warrantyMonths) || 6;
        if (body.note !== undefined) next.note = String(body.note).trim().slice(0, 300);
        if (body.nfcUid !== undefined) next.nfcUid = String(body.nfcUid).trim().slice(0, 40);
        if (body.hw !== undefined && isValidHw(String(body.hw).trim())) next.hw = String(body.hw).trim();
        if (body.beta !== undefined) next.beta = Boolean(body.beta);
        devices[id] = next;
        scheduleSave();
        return sendJson(res, 200, { ok: true, id, device: next }, corsHeaders());
      } catch (err) {
        return sendJson(res, 400, { ok: false, error: err.message }, corsHeaders());
      }
    }

    // DELETE /api/admin/devices/:id
    if (req.method === "DELETE" && pathname.startsWith("/api/admin/devices/")) {
      const id = decodeURIComponent(pathname.slice(19)).trim().toUpperCase();
      if (devices[id]) {
        delete devices[id];
        scheduleSave();
      }
      return sendJson(res, 200, { ok: true }, corsHeaders());
    }

    // POST /api/admin/firmware?version=2.5.0&hw=nicemcu_2.8ips&beta=0&changelog=...
    if (req.method === "POST" && pathname === "/api/admin/firmware") {
      const version = (url.searchParams.get("version") || "").trim();
      const hw = (url.searchParams.get("hw") || "nicemcu_2.8ips").trim();
      const beta = url.searchParams.get("beta") === "1" || url.searchParams.get("beta") === "true";
      const changelog = (url.searchParams.get("changelog") || "").trim().slice(0, 500);

      if (!isValidVersion(version)) {
        return sendJson(res, 400, { ok: false, error: "invalid_version (use x.y.z)" }, corsHeaders());
      }
      if (!isValidHw(hw)) {
        return sendJson(res, 400, { ok: false, error: "invalid_hw" }, corsHeaders());
      }

      const chunks = [];
      let total = 0;
      let aborted = false;
      req.on("data", (chunk) => {
        total += chunk.length;
        if (total > MAX_FIRMWARE_BYTES) {
          aborted = true;
          req.destroy();
          return;
        }
        chunks.push(chunk);
      });
      req.on("end", () => {
        if (aborted) {
          return sendJson(res, 413, { ok: false, error: "file_too_large" }, corsHeaders());
        }
        if (total < 10000) {
          return sendJson(res, 400, { ok: false, error: "firmware_too_small" }, corsHeaders());
        }
        const buf = Buffer.concat(chunks);
        // Basic ESP32 app image magic check (0xE9)
        if (buf[0] !== 0xe9) {
          return sendJson(res, 400, { ok: false, error: "invalid_esp32_bin_magic" }, corsHeaders());
        }
        const sha256 = crypto.createHash("sha256").update(buf).digest("hex");
        const filename = `${hw}_v${version}.bin`;
        const outPath = path.join(FIRMWARE_DIR, filename);
        fs.writeFileSync(outPath, buf);

        const relId = `${hw}@${version}`;
        releases = releases.filter((r) => r.id !== relId);
        const entry = {
          id: relId,
          version,
          hw,
          filename,
          size: buf.length,
          sha256,
          changelog,
          beta,
          releasedAt: Date.now(),
        };
        releases.push(entry);
        saveReleasesSync();
        console.log(`[Firmware] Uploaded ${filename} (${buf.length} B, beta=${beta})`);
        return sendJson(res, 200, { ok: true, release: entry }, corsHeaders());
      });
      return;
    }

    // PATCH /api/admin/firmware/:id — toggle beta or update changelog
    if (req.method === "PATCH" && pathname.startsWith("/api/admin/firmware/")) {
      const relId = decodeURIComponent(pathname.slice(20)).trim();
      const rel = releases.find((r) => r.id === relId);
      if (!rel) return sendJson(res, 404, { ok: false, error: "not_found" }, corsHeaders());
      try {
        const body = await readJsonBody(req, 4096);
        if (body.beta !== undefined) rel.beta = Boolean(body.beta);
        if (body.changelog !== undefined) rel.changelog = String(body.changelog).trim().slice(0, 500);
        saveReleasesSync();
        return sendJson(res, 200, { ok: true, release: rel }, corsHeaders());
      } catch (err) {
        return sendJson(res, 400, { ok: false, error: err.message }, corsHeaders());
      }
    }

    // DELETE /api/admin/firmware/:id
    if (req.method === "DELETE" && pathname.startsWith("/api/admin/firmware/")) {
      const relId = decodeURIComponent(pathname.slice(20)).trim();
      const rel = releases.find((r) => r.id === relId);
      if (rel) {
        const fpath = path.join(FIRMWARE_DIR, path.basename(rel.filename));
        if (fs.existsSync(fpath)) fs.unlinkSync(fpath);
        releases = releases.filter((r) => r.id !== relId);
        saveReleasesSync();
      }
      return sendJson(res, 200, { ok: true }, corsHeaders());
    }

    return sendJson(res, 404, { ok: false, error: "unknown_admin_endpoint" }, corsHeaders());
  }

  // ── Online User Guide ─────────────────────────────────────────
  if (req.method === "GET" && (pathname === "/" || pathname === "/guide" || pathname === "/guide/")) {
    try {
      const html = fs.readFileSync(GUIDE_FILE, "utf8");
      res.writeHead(200, {
        "Content-Type": "text/html; charset=utf-8",
        "Cache-Control": "public, max-age=300",
      });
      return res.end(html);
    } catch {
      res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
      return res.end("Guide not found");
    }
  }

  if (req.method === "GET" && pathname.startsWith("/images/")) {
    const file = path.basename(pathname.slice(8));
    const ext = path.extname(file).toLowerCase();
    const mime = { ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".png": "image/png", ".webp": "image/webp" }[ext];
    if (mime) {
      const imgPath = path.join(PUBLIC_DIR, "images", file);
      if (fs.existsSync(imgPath)) {
        res.writeHead(200, { "Content-Type": mime, "Cache-Control": "public, max-age=86400" });
        return fs.createReadStream(imgPath).pipe(res);
      }
    }
  }

  res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
  res.end("CYD SmartDesk NFC Redirector");
});

server.listen(PORT, "0.0.0.0", () => {
  console.log(`[SmartDesk Cloud] Listening on :${PORT} (data: ${DATA_DIR})`);
});

for (const sig of ["SIGTERM", "SIGINT"]) {
  process.on(sig, () => {
    flushSaveSync();
    server.close(() => process.exit(0));
  });
}
