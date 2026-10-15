# 🏷️ Hướng Dẫn NFC & Cài Đặt PWA (SmartDesk App) Cho CYD SmartDesk V2

Tài liệu này hướng dẫn cách cấu hình **Thẻ NFC 1 chạm** và cài đặt **PWA (Progressive Web App)** để biến bảng điều khiển Web của CYD SmartDesk thành một ứng dụng độc lập, tràn viền toàn màn hình trên điện thoại Android và iOS.

---

## 📱 1. PWA Là Gì & Trải Nghiệm Sử Dụng

**PWA (Progressive Web App)** cho phép bạn cài đặt Web Dashboard trực tiếp lên màn hình chính của điện thoại mà **không cần thông qua App Store hay Google Play**:
- **Tràn viền (Full-screen Standalone):** Ẩn hoàn toàn thanh địa chỉ URL của trình duyệt, thanh điều hướng trình duyệt và nút back/forward.
- **Có Icon & Tên ứng dụng riêng:** Xuất hiện trên màn hình chính (Homescreen) và danh sách ứng dụng cạnh các app thông thường.
- **Tốc độ phản hồi cực nhanh:** Nhờ cơ chế Service Worker lưu cache giao diện sẵn trên máy, ứng dụng khởi động tức thì khi bấm mở.
- **Tiện lợi:** Bạn chỉ cần chạm NFC để mở và cài đặt lần đầu tiên. Từ những lần sau, chỉ cần bấm vào Icon ngoài màn hình chính để mở app!

---

## 📲 2. Cách Cài Đặt App Vào Màn Hình Chính

### Dành cho iPhone / iPad (iOS Safari)
> **Lưu ý:** Trên iOS, bạn cần mở đường link bằng trình duyệt mặc định **Safari**.

1. Mở Safari và truy cập vào: `http://smartdesk.local` (hoặc địa chỉ IP hiển thị trên màn hình Cài đặt của CYD).
2. Bấm vào nút **Chia sẻ (Share)** ở thanh công cụ dưới đáy Safari (biểu tượng hình vuông có mũi tên chỉ lên: ⎋).
3. Cuộn xuống và chọn **"Thêm vào MH chính"** (Add to Home Screen).
4. Đặt tên app là **"SmartDesk"** và bấm **"Thêm"** (Add) ở góc trên bên phải.
5. Icon ứng dụng **CYD SmartDesk V2** sẽ xuất hiện trên màn hình chính điện thoại. Bấm vào để mở trải nghiệm toàn màn hình!

---

### Dành cho Android (Samsung, Xiaomi, Oppo, Pixel...)
> **Lưu ý:** Sử dụng trình duyệt **Google Chrome** hoặc **Samsung Internet**.

1. Mở Chrome và truy cập vào: `http://smartdesk.local` (hoặc IP của CYD).
2. Khi trang web tải xong:
   - Sẽ có một thanh banner hiện lên: **"Cài đặt SmartDesk App"** kèm nút **"Cài đặt"** (Install).
   - Bấm nút **"Cài đặt"** -> chọn **"Cài đặt"** trong hộp thoại xác nhận.
   - *(Nếu không thấy banner: Bấm vào dấu **3 chấm dọc** ở góc trên bên phải trình duyệt -> Chọn **"Cài đặt ứng dụng"** hoặc **"Thêm vào màn hình chính"**).*
3. Ứng dụng sẽ được cài đặt và tạo lối tắt trên màn hình chính điện thoại.

---

## 🏷️ 3. Mua Thẻ NFC & Cách Ghi Link 1 Chạm

### A. Nên mua loại thẻ NFC nào?
- **Loại thẻ khuyên dùng:** **NTAG213** hoặc **NTAG215** dạng **Sticker (Tem dán keo 3M)**.
- **Kích thước:** Tròn đường kính khoảng 20mm - 25mm, siêu mỏng.
- **Giá thành:** Rất rẻ (chỉ khoảng 3.000đ - 8.000đ/tem trên Shopee/Lazada).
- **Lưu ý quan trọng:** Nếu bạn định dán tem lên bề mặt kim loại, hãy mua loại tem có ghi chú **"Chống kim loại" (Anti-metal tag)** để sóng NFC không bị triệt tiêu. Dán lên gỗ, nhựa, mica thì dùng tem thường thoải mái.

---

### B. Ứng dụng cần cài trên điện thoại để ghi thẻ
Tải app miễn phí **"NFC Tools"** từ kho ứng dụng:
- **iOS:** Tìm "NFC Tools" trên Apple App Store.
- **Android:** Tìm "NFC Tools" trên Google Play Store.

---

### C. Các bước ghi đường link vào thẻ NFC

1. Mở app **NFC Tools** trên điện thoại.
2. Bấm chọn tab **Write** (Ghi).
3. Bấm **Add a record** (Thêm bản ghi).
4. Chọn mục **URL / URI**.
5. Nhập đường dẫn:
   - Cách 1 (Khuyên dùng): `http://smartdesk.local`
   - Cách 2: `http://192.168.x.x` (IP tĩnh của CYD trong mạng WiFi gia đình bạn).
6. Bấm **OK**.
7. Bấm nút **Write / [X] Bytes** (Ghi).
8. Áp mặt lưng điện thoại (vùng anten NFC: gần cụm camera) vào con tem NFC cho đến khi màn hình báo **Success! (Ghi thành công tích xanh)**.

---

## 📍 4. Gợi Ý Vị Trí Dán Thẻ NFC Trên Bàn Làm Việc

- **Góc chân đế màn hình CYD:** Khách tới bàn hoặc chính bạn chỉ cần đưa điện thoại quẹt nhẹ vào chân đế là mở ngay dashboard điều khiển.
- **Đế sạc không dây / Giá đỡ điện thoại:** Mỗi khi ngồi vào bàn làm việc và đặt máy lên đế sạc, điện thoại sẽ tự động bật popup mở SmartDesk.
- **Dán ẩn dưới mặt bàn gỗ (độ dày < 1.5cm):** Khoét nhẹ hoặc dán tem NFC mặt dưới bàn, mặt trên bàn có thể dán 1 sticker logo nhỏ đánh dấu vị trí "Touch to Control".

---

## 🛠️ 5. Xử Lý Sự Cố Thường Gặp (Troubleshooting)

1. **Điện thoại quét NFC không phản hồi:**
   - Kiểm tra xem bạn đã bật tính năng NFC trong mục Cài đặt kết nối của điện thoại chưa (iPhone từ đời XS trở lên luôn tự động bật NFC).
   - Tháo thử ốp lưng nếu ốp quá dày hoặc có gắn vòng kim loại MagSafe chắn anten NFC.
2. **Quét NFC mở ra nhưng báo "Không thể kết nối" hoặc "Trang web không khả dụng":**
   - Đảm bảo điện thoại đang kết nối **cùng một mạng WiFi** với chiếc CYD SmartDesk.
   - Một số dòng máy Android đời cũ không hỗ trợ mDNS phân giải tên miền `.local`. Nếu gặp trường hợp này, hãy ghi thẻ NFC bằng **địa chỉ IP trực tiếp** của CYD (ví dụ: `http://192.168.1.150`). Bạn có thể cố định IP của ESP32 trong router WiFi gia đình (DHCP Static Lease).
3. **Mất icon App trên màn hình chính:**
   - Bạn chỉ cần chạm lại thẻ NFC hoặc vào lại Safari/Chrome để bấm "Thêm vào MH chính" một lần nữa.
