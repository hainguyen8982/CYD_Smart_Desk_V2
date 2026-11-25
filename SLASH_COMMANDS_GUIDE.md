# ⚡ Hướng Dẫn Chi Tiết Các Lệnh `/` (Slash Commands) Trong Antigravity AI Agent

> Tài liệu tra cứu nhanh cách dùng các lệnh `/` để **làm việc hiệu quả hơn, code ít lỗi hơn và tiết kiệm tối đa token**.  
> Chỉ cần gõ dấu `/` vào đầu ô chat, giao diện sẽ hiện danh sách gợi ý để bạn chọn nhanh.

---

## 📋 1. Bảng Tra Cứu Nhanh (Cheat Sheet)

| Lệnh | Nhóm | Tóm tắt công dụng |
| :--- | :--- | :--- |
| **`/compact`** | Tiết kiệm Token | Nén & tóm tắt lịch sử chat để giải phóng bộ nhớ ngữ cảnh |
| **`/learn`** | Ghi nhớ dài hạn | Lưu lại quy tắc, cách fix lỗi hoặc cấu hình để dùng cho các phiên sau |
| **`/plan`** | Lập kế hoạch | Lập bản thiết kế & kế hoạch từng bước trước khi đụng vào code |
| **`/grill-me`** | Chốt yêu cầu | AI phỏng vấn ngược lại bạn để làm rõ toàn bộ góc khuất thiết kế |
| **`/boost`** | Suy nghĩ sâu | Phân tích đa góc độ, kiểm chứng kỹ cho bài toán / bug khó |
| **`/goal`** | Chạy tới đích | Chạy bền bỉ, tự kiểm tra kỹ cho đến khi hoàn thành trọn vẹn mục tiêu |
| **`/teamwork`** | Đa Agent | Huy động nhiều agent con chạy song song trên nhiều mảng khác nhau |
| **`/browser`** | Trình duyệt | Điều khiển trực tiếp Chrome để kiểm tra web, bấm nút, test UI live |
| **`/schedule`** | Hẹn giờ | Đặt hẹn giờ 1 lần hoặc lịch chạy định kỳ (cron) trong phiên |
| **`/automation`** | Tự động hóa | Thiết kế tác vụ nền tự động chạy định kỳ dài hạn |
| **`/plugin`** | Mở rộng | Cài đặt, bật/tắt hoặc đóng gói plugin (MCP, skills, rules, hooks) |

---

## 🔍 2. Hướng Dẫn Chi Tiết Từng Lệnh

### 2.1. `/compact` — Nén ngữ cảnh & Tiết kiệm Token
- **Tác dụng:** Tóm tắt toàn bộ lịch sử hội thoại dài thành một bản tóm lược ngắn gọn, giữ lại quyết định quan trọng và trạng thái code hiện tại, bỏ đi các log dài đã xong.
- **Khi nào nên dùng:**
  - Khi cuộc trò chuyện đã kéo dài nhiều chục lượt (đặc biệt sau khi debug nhiều lỗi hoặc gửi nhiều ảnh).
  - Trước khi chuyển sang một đầu việc mới nhưng vẫn muốn AI nhớ bối cảnh cũ.
- **Khi nào KHÔNG cần:** Hội thoại mới bắt đầu được vài lượt.
- **Cách dùng mẫu:**
  ```text
  /compact
  ```
  *(Hoặc kèm trọng tâm cần giữ lại)*:
  ```text
  /compact giữ lại toàn bộ thông số vỏ 3D và cấu hình Cloud OTA v2.5.0
  ```

---

### 2.2. `/learn` — Dạy AI nhớ quy tắc cho các lần sau
- **Tác dụng:** Đóng gói một bài học, lỗi đặc thù của môi trường máy bạn, hoặc quy chuẩn code/thiết kế vào bộ nhớ dài hạn (rule/skill) để lần sau AI **không lặp lại sai lầm cũ**.
- **Khi nào nên dùng:**
  - Ngay sau khi vừa sửa xong một lỗi môi trường khó chịu (vd: PlatformIO trên Windows, PowerShell 5.1 không dùng `&&`, thông số support PETG cho Anycubic Kobra X...).
  - Khi bạn muốn AI luôn tuân thủ một phong cách riêng (ngôn ngữ, cấu trúc thư mục, chuẩn đặt tên).
- **Cách dùng mẫu:**
  ```text
  /learn Luôn dùng Top Z distance 0.24mm và Normal (manual) Snug support khi xuất cấu hình in PETG cho Anycubic Kobra X
  ```

---

### 2.3. `/plan` — Lập kế hoạch trước khi sửa code
- **Tác dụng:** Ép AI khảo sát codebase, viết ra bản kế hoạch chi tiết (file nào sẽ sửa, kiến trúc ra sao, rủi ro gì, kiểm thử thế nào) và **chờ bạn duyệt** trước khi viết dòng code nào.
- **Khi nào nên dùng:**
  - Tính năng mới đụng từ 3 file trở lên (vd: thêm Cloud OTA, thêm trang mới trên màn hình CYD, tách giao thức ESP-NOW).
  - Refactor kiến trúc lớn mà bạn muốn kiểm soát hướng đi từ đầu.
- **Khi nào KHÔNG cần:** Sửa nhanh 1 dòng CSS, đổi 1 hằng số, sửa 1 câu chữ.
- **Cách dùng mẫu:**
  ```text
  /plan Thiết kế thêm trang hiển thị chứng khoán VN-Index trên CYD SmartDesk V2 mà không làm tăng RAM quá 5KB
  ```

---

### 2.4. `/grill-me` — AI phỏng vấn ngược để chốt yêu cầu chặt chẽ
- **Tác dụng:** Thay vì AI tự đoán ý bạn rồi làm sai ý phải sửa lại nhiều lần, AI sẽ đặt lần lượt từng câu hỏi ngắn gọn, sắc bén về UX, phần cứng, edge cases, bảo mật… cho tới khi bản thiết kế kín kẽ 100%.
- **Khi nào nên dùng:**
  - Khi bạn mới có ý tưởng sơ bộ trong đầu và chưa chắc nên làm theo phương án nào.
  - Trước khi bắt tay làm sản phẩm thương mại mới hoặc phiên bản lớn (vd: SmartDesk V3, App Mobile, Web Portal).
- **Cách dùng mẫu:**
  ```text
  /grill-me Mình muốn làm tính năng đồng bộ lịch Google Calendar lên CYD SmartDesk, hãy hỏi để chốt thiết kế
  ```

---

### 2.5. `/boost` — Suy nghĩ sâu & Kiểm chứng đa góc độ
- **Tác dụng:** Bật mức độ phân tích sâu nhất, cân nhắc nhiều phương án đối chiếu, soi kỹ các lỗi ẩn (race condition, rò rỉ RAM, bảo mật, phần cứng) và kiểm chứng chặt chẽ hơn.
- **Khi nào nên dùng:**
  - Gặp bug khó tái hiện (vd: ESP32 bị crash ngẫu nhiên khi chạy Wi-Fi + SPI + DAC cùng lúc, lỗi đứt kết nối Web OTA).
  - Cần review bảo mật hoặc đánh giá kiến trúc toàn bộ dự án trước khi xuất xưởng hàng loạt.
- **Cách dùng mẫu:**
  ```text
  /boost Kiểm tra toàn bộ xung đột bộ nhớ và đa luồng giữa Core 0 (NetworkTask) và Core 1 (Display/WebDashboard)
  ```

---

### 2.6. `/goal` — Chế độ chạy bền bỉ đến khi đạt đích
- **Tác dụng:** Đặt một mục tiêu rõ ràng có tiêu chí hoàn thành cụ thể; AI sẽ tự làm, tự build, tự test, tự sửa lỗi liên tục cho đến khi mọi tiêu chí đều xanh 100% mới dừng lại báo cáo.
- **Khi nào nên dùng:**
  - Tác vụ nhiều bước mất thời gian (vd: viết full unit test, nâng cấp toàn bộ hệ thống và kiểm tra build sạch cả 2 board CYD & NiceMCU).
  - Khi bạn muốn giao việc rồi đi làm việc khác / để chạy qua đêm.
- **Cách dùng mẫu:**
  ```text
  /goal Tối ưu toàn bộ ảnh bitmap và HTML sao cho Flash < 80% trên cả 2 env platformio và build pass không lỗi
  ```

---

### 2.7. `/teamwork` — Huy động đội ngũ nhiều Agent làm song song
- **Tác dụng:** Tách dự án lớn thành nhiều mảng độc lập và giao cho nhiều agent con chạy đồng thời (mỗi agent giữ ngữ cảnh riêng, không làm loãng nhau), sau đó tổng hợp lại.
- **Khi nào nên dùng:**
  - Làm cùng lúc nhiều phần độc lập: VD Agent 1 viết Firmware ESP32, Agent 2 viết Server Node.js, Agent 3 viết giao diện Web Admin & tài liệu hướng dẫn.
  - Review dự án lớn theo nhiều góc nhìn (Bảo mật / Hiệu năng / UX).
- **Cách dùng mẫu:**
  ```text
  /teamwork Xây dựng trọn bộ DeskSetup WebPortal: 1 agent làm backend API, 1 agent làm giao diện React, 1 agent viết Docker & tài liệu deploy
  ```

---

### 2.8. `/browser` — Điều khiển trực tiếp trình duyệt Chrome
- **Tác dụng:** Mở Chrome thật để AI nhìn giao diện, click nút, điền form, cuộn trang, kiểm tra console lỗi và chụp ảnh xác nhận.
- **Khi nào nên dùng:**
  - Test trực tiếp giao diện Web Dashboard (`http://192.168.x.x`) hoặc trang Cloud Admin (`https://smartdesk.fabx.vn/admin`).
  - Cần tương tác với web động (JavaScript render) hoặc web đã đăng nhập sẵn của bạn.
- **Cách dùng mẫu:**
  ```text
  /browser Mở https://smartdesk.fabx.vn/admin kiểm tra giao diện trên màn hình mobile và báo nếu có lỗi hiển thị
  ```

---

### 2.9. `/schedule` & `/automation` — Hẹn giờ & Tự động hóa định kỳ
- **`/schedule`:** Đặt hẹn giờ chạy 1 lần hoặc cron ngắn hạn ngay trong phiên làm việc hiện tại.
- **`/automation`:** Thiết kế tác vụ nền tự động chạy lâu dài theo lịch (kể cả khi bạn không mở phiên chat này).
- **Cách dùng mẫu:**
  ```text
  /schedule Kiểm tra https://smartdesk.fabx.vn/health mỗi 30 phút và báo mình nếu server bị sập
  ```
  ```text
  /automation Mỗi sáng thứ 2 lúc 8h tổng hợp danh sách thiết bị mới kích hoạt tuần qua trên smartdesk.fabx.vn
  ```

---

### 2.10. `/plugin` — Quản lý & Đóng gói Plugin mở rộng
- **Tác dụng:** Tìm, bật/tắt hoặc tự đóng gói bộ công cụ tùy chỉnh (MCP servers, skills, rules, hooks) thành 1 plugin gọn gàng dùng chung cho nhiều dự án.
- **Cách dùng mẫu:**
  ```text
  /plugin Đóng gói quy trình build PlatformIO và xuất STL vỏ 3D thành một plugin dùng chung
  ```

---

## 🎯 3. 3 Combo Quy Trình Thực Chiến Hiệu Quả Nhất

### Combo A: Làm tính năng mới chuẩn chỉnh, không phải đập đi làm lại
1. **`/grill-me`** → AI hỏi 3–5 câu để chốt sạch yêu cầu.
2. **`/plan`** → AI lên danh sách file & bước làm, bạn bấm duyệt.
3. Thực thi & build test thực tế.
4. **`/learn`** (nếu có kinh nghiệm mới cần nhớ) → **`/compact`** để thu gọn chat.

### Combo B: Fix bug khó (như lỗi phần cứng, đa luồng, âm thanh, mạng)
1. Mô tả rõ hiện tượng thực tế + gửi ảnh/log.
2. Dùng **`/boost`** để AI phân tích sâu tận gốc phần cứng/thời gian thực thay vì vá bề mặt.

### Combo C: Tiết kiệm Token tối đa khi làm dự án dài ngày
1. **Tách riêng từng chat theo chủ đề** (Firmware riêng, Vỏ 3D riêng, Server riêng).
2. Khi 1 chat dài quá ~15–20 lượt → gõ **`/compact`**.
3. Cập nhật tóm tắt kiến trúc vào file `GEMINI.md` của dự án để mỗi chat mới AI đọc 1 lần là nắm ngay toàn bộ quy ước.
