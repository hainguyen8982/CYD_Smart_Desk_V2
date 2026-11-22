#pragma once
#include <Arduino.h>

// =========================================================================
// GIAO THỨC ĐỒNG BỘ HỆ SINH THÁI KHÔNG DÂY (ESP-NOW ECOSYSTEM PROTOCOL 2026)
// Tham chiếu: GEMINI.md - Mục 3 ("Standalone by Default – Better Together")
// =========================================================================

#define ECO_MAGIC               0xEE  // Định danh gói tin Hệ sinh thái Desk Setup
#define ECO_SENDER_CYD          0x01  // Trạm điều khiển trung tâm (The Brain - CYD SmartDesk V2)
#define ECO_SENDER_EMO          0x02  // Bạn đồng hành cảm xúc (The Soul - EMO Companion)

// Bảng mã sự kiện hệ sinh thái (Event Types)
enum EcosystemEventType : uint8_t {
    EVT_HEARTBEAT       = 0x01,  // Gói tin nhịp tim giữ liên lạc / nhận diện (Ping)
    EVT_HEARTBEAT_ACK   = 0x02,  // Phản hồi nhịp tim (Pong)

    EVT_POMODORO_START  = 0x10,  // Kích hoạt Pomodoro (payload = số phút tập trung)
    EVT_POMODORO_TICK   = 0x11,  // Đếm ngược Pomodoro (payload = số giây còn lại)
    EVT_POMODORO_BREAK  = 0x12,  // Hết giờ Pomodoro -> Nghỉ giải lao / Vươn vai ăn mừng
    EVT_POMODORO_STOP   = 0x13,  // Tạm dừng hoặc Đặt lại Pomodoro (payload = 0: Reset, >0: Pause)

    EVT_ALARM_RING      = 0x20,  // Chuông báo thức CYD đang reo -> EMO mở to mắt chớp cảnh báo
    EVT_ALARM_DISMISS   = 0x21,  // Tắt chuông báo thức (từ CYD hoặc từ nút bấm trên EMO)

    EVT_NIGHT_SLEEP     = 0x30,  // Lệnh ngủ đêm / Tắt màn hình -> EMO dim màn hình & cuộn tròn ngủ
    EVT_MORNING_WAKE    = 0x31,  // Lệnh thức dậy / Bật màn hình -> EMO tăng độ sáng & chào buổi sáng

    EVT_MOOD_SYNC       = 0x40,  // Đồng bộ tâm trạng bàn làm việc (payload = MoodType 0..5)

    EVT_WEATHER_SYNC    = 0x50,  // Đồng bộ nhiệt độ từ CYD sang EMO (payload = int16_t(roundf(temp * 10.0f)))
    EVT_HUMIDITY_SYNC   = 0x51,  // Đồng bộ độ ẩm & mã WMO (payload = (humidity << 8) | wmoCode)

    EVT_PAIR_REQ        = 0x60,  // Yêu cầu ghép đôi khóa MAC (CYD -> EMO trong Pairing Mode 30s)
    EVT_PAIR_ACK        = 0x61   // Xác nhận khóa MAC (EMO -> CYD)
};

// Cấu trúc gói tin nhị phân chuẩn (Ecosystem Packet - 6 bytes đóng gói chặt)
typedef struct __attribute__((packed)) {
    uint8_t magic;         // 0xEE (Ecosystem Identifier)
    uint8_t senderId;      // 0x01 = CYD_SMARTDESK, 0x02 = EMO_COMPANION
    uint8_t eventType;     // Mã sự kiện (EcosystemEventType)
    uint16_t payload;      // Dữ liệu kèm theo (thời gian, giây, phút, mã tâm trạng)
    uint8_t checksum;      // XOR checksum kiểm tra tính toàn vẹn
} EcosystemPacket;

// Hàm tính và kiểm tra Checksum XOR
inline uint8_t ecoCalculateChecksum(const EcosystemPacket &pkt) {
    return pkt.magic ^ pkt.senderId ^ pkt.eventType ^ (uint8_t)(pkt.payload & 0xFF) ^ (uint8_t)(pkt.payload >> 8);
}

