# 🖥️ CYD SMARTDESK V2 — WORKSPACE CONTEXT & ECOSYSTEM SPEC

> **Tài liệu tham chiếu chuẩn cho AI Agent & Nhà phát triển**  
> Dự án: **CYD SmartDesk V2 (The Brain / Master Control Station)**  
> Nằm trong: **Bộ sưu tập Desk Setup Ecosystem 2026**

---

## 1. ĐỊNH VỊ SẢN PHẨM & LIÊN KẾT HỆ SINH THÁI (ECOSYSTEM CONTEXT)

Hệ sinh thái bàn làm việc gồm 3 thành phần liên kết chặt chẽ:
1. 🖥️ **Trạm điều khiển trung tâm (The Brain - Dự án này)**: [CYD_SmartDesk_V2](file:///D:/Projects_2026/CYD_SmartDesk_V2)  
   - ESP32 Cheap Yellow Display (ESP32-2432S028), Màn hình 2.8" cảm ứng (320x240).
   - Công năng: Dashboard đa năng (Lịch Âm Dương Can Chi, Thời tiết, Giá vàng SJC thực tế 7 ngày, Tỉ giá ngoại tệ, Trạm Pomodoro & Âm thanh chuông báo Acoustic).
2. 🤖 **Bạn đồng hành cảm xúc (The Soul)**: [ESP32_TDisplay_Emo_Companion](file:///d:/Projects_2026/ESP32_TDisplay_Emo_Companion)  
   - LilyGO TTGO T-Display 1.14" IPS (240x135), ESP32 Dual Core.
   - Công năng: Mắt biểu cảm 60 FPS (Robot & Cartoon Dark Spooky), Đồng hồ StandBy Luxury phong cách Apple, Cảm ứng chạm ẩn trên nóc vỏ, 100% êm ái (Silent Companion).
3. 🌐 **Cổng Web điều khiển tập trung (The Web Portal)**: `DeskSetup_WebPortal`  
   - Web App hiện đại (PWA/SPA) triển khai trên Vercel (Staging) và Private Server.

---

## 2. NGUYÊN TẮC CỐT LÕI (CORE PRINCIPLES)

1. **"Standalone by Default – Better Together" (Độc lập mặc định, Hoàn hảo khi kết hợp)**:
   - Khách mua lẻ CYD SmartDesk: Tự túc Wi-Fi, tự lấy giờ NTP, tự lấy thời tiết, giá vàng SJC, tỉ giá, chạy Pomodoro độc lập kèm âm thanh còi buzzer/chuông acoustic.
   - Khách mua cả bộ: Hai thiết bị tự động nhận diện và đồng bộ qua **ESP-NOW** mà không cần cấu hình phức tạp.
   - **TUYỆT ĐỐI KHÔNG TÁCH NHÁNH FIRMWARE**: Cùng 1 bản firmware cho cả bản bán lẻ lẫn bản combo.
2. **Quy chuẩn Cảm ứng & Âm thanh trên CYD**:
   - Cảm ứng điện trở: Vùng chạm lớn ($\ge 40\times 40\text{ px}$), không dùng cử chỉ vuốt (no swipe), phản hồi âm thanh buzzer click siêu nhẹ ($15\text{ms}$).
   - Không gian RAM: Dùng kiến trúc Component / Card Sprites cục bộ, giữ RAM trống $>120\text{ KB}$ để chạy mượt mà Wi-Fi, JSON và Web Server.

---

## 3. ĐẶC TẢ GIAO THỨC ĐỒNG BỘ ESP-NOW (ECOSYSTEM PROTOCOL)

Khi cả hai thiết bị cùng online trên cùng bàn làm việc, CYD đóng vai trò Master phát các sự kiện qua ESP-NOW (< 5ms):

### Cấu trúc gói tin nhị phân (Ecosystem Packet):
```cpp
typedef struct __attribute__((packed)) {
    uint8_t magic;         // 0xEE (Ecosystem Identifier)
    uint8_t senderId;      // 0x01 = CYD_SMARTDESK, 0x02 = EMO_COMPANION
    uint8_t eventType;     // Mã sự kiện
    uint16_t payload;      // Dữ liệu kèm theo (thời gian, giá trị)
    uint8_t checksum;      // XOR checksum kiểm tra tính toàn vẹn
} EcosystemPacket;
```

### Các sự kiện CYD phát ra:
* Khi bấm Bắt đầu Pomodoro trên CYD ➔ Gửi `0x10` (`EVT_POMODORO_START`) để EMO Companion vào chế độ chăm chú học bài.
* Khi hết 25 phút Pomodoro trên CYD ➔ Gửi `0x12` (`EVT_POMODORO_BREAK`) để EMO Companion reo vui mừng.
* Khi đến giờ Báo thức trên CYD ➔ Gửi `0x20` (`EVT_ALARM_RING`) để EMO Companion mở to mắt chớp liên tục thu hút sự chú ý.
* Khi đến giờ ngủ đêm (23:00) ➔ Gửi `0x30` (`EVT_NIGHT_SLEEP`) để EMO Companion tự dim sáng và nhắm mắt ngủ.

---

## 4. TÀI LIỆU LIÊN QUAN TRONG BỘ SƯU TẬP
* Kế hoạch chi tiết CYD SmartDesk V2: [CYD_SmartDesk_V2/PROJECT_PLAN.md](file:///D:/Projects_2026/CYD_SmartDesk_V2/PROJECT_PLAN.md)
* Hướng dẫn NFC & PWA: [CYD_SmartDesk_V2/NFC_PWA_GUIDE.md](file:///D:/Projects_2026/CYD_SmartDesk_V2/NFC_PWA_GUIDE.md)
* Đặc tả phần cứng NiceMCU-32S 2.8" IPS: [NICEMCU_32S_2.8IPS_HARDWARE_SPEC.md](file:///D:/Projects_2026/CYD_SmartDesk_V2/NICEMCU_32S_2.8IPS_HARDWARE_SPEC.md)
* Bạn đồng hành cảm xúc: [ESP32_TDisplay_Emo_Companion/GEMINI.md](file:///d:/Projects_2026/ESP32_TDisplay_Emo_Companion/GEMINI.md)
* Báo cáo đánh giá thương mại hóa: [commercial_desk_companion_evaluation.md](file:///C:/Users/tien/.gemini/antigravity-ide/brain/269027f9-0abc-43e2-a0eb-3e6e6c51b0ad/commercial_desk_companion_evaluation.md)
