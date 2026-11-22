# CYD SmartDesk V2 — NFC Cloud Redirector (Docker Compose)

Dịch vụ chuyển hướng siêu nhẹ (**Node.js 22 Alpine, 0 dependency, ~15 MB RAM**) giúp thẻ NFC mở trực tiếp Web Dashboard của CYD SmartDesk trên **100% điện thoại Android & iOS** (khắc phục lỗi Android không phân giải được `.local` mDNS).

## 1. Triển khai nhanh bằng Docker Compose

```bash
cp .env.example .env
# Sửa CHECKIN_SECRET và ADMIN_KEY trong .env nếu cần
docker compose up -d --build
```

Kiểm tra dịch vụ đang chạy:
```bash
curl http://127.0.0.1:3080/health
# -> {"status":"ok","devices":0}
```

## 2. Cấu hình Nginx Reverse Proxy (HTTPS)

```nginx
server {
    listen 443 ssl http2;
    server_name go.tenmien.vn; # Thay bằng subdomain thực tế

    location / {
        proxy_pass http://127.0.0.1:3080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## 3. Các Endpoint

* `POST /api/checkin` (Header `X-Api-Key: <CHECKIN_SECRET>`): ESP32 tự gửi `{"id":"A3F91C","ip":"192.168.1.45","mdns":"smartdesk-f91c.local","fw":"2.4"}` mỗi khi vào Wi-Fi hoặc đổi IP LAN.
* `GET /d/<DEVICE_ID>`: Link ghi lên tem NFC (ví dụ `https://go.tenmien.vn/d/A3F91C`) → Tự động nhảy thẳng vào `http://192.168.1.45/` và hiện hướng dẫn nếu điện thoại đang dùng 4G/khác Wi-Fi.
* `GET /health`: Healthcheck cho Docker/Monitoring.
* `GET /admin/devices?key=<ADMIN_KEY>`: Xem JSON danh sách các máy đã check-in.

## 4. Kết nối với Firmware ESP32

Mở `include/Config.h` và điền domain sau khi deploy xong:
```cpp
#define NFC_REDIRECTOR_URL      "https://go.tenmien.vn/api/checkin"
#define NFC_CHECKIN_SECRET      "smartdesk-2026"
```
