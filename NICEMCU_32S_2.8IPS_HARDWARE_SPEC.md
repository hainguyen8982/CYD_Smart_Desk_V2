# 📟 NICEMCU-32S-DEV 2.8" IPS — HARDWARE SPEC & PORTING GUIDE

> **Dự án liên kết:** [CYD_SmartDesk_V2](file:///D:/Projects_2026/CYD_SmartDesk_V2) (Trạm điều khiển trung tâm)  
> **Mã sản phẩm:** `NiceMCU-32S-DEV_2.8IPS` (ký hiệu in trên PCB: `micemcu_32s_2.8isp` hoặc `NiceMCU_32S_2.8IPS`)  
> **Nhà phát triển:** Dongguan NiceMCU Technology Co., Ltd. (东莞耐思物联科技有限公司) / SparkleIoT  
> **Kho GitHub chính thức:** [NiceMCU-01/NiceMCU-32S-DEV_2.8IPS](https://github.com/NiceMCU-01/NiceMCU-32S-DEV_2.8IPS)

---

## 1. ĐỊNH VỊ PHẦN CỨNG & LỢI THẾ NÂNG CẤP

Board **NiceMCU-32S-DEV 2.8" IPS** là phiên bản phần cứng nâng cấp vượt trội so với dòng **CYD (ESP32-2432S028R "Cheap Yellow Display")** truyền thống đang sử dụng trên dự án SmartDesk V2:

| Tiêu chí | CYD Vàng Truyền Thống (ESP32-2432S028R) | **NiceMCU-32S-DEV 2.8" IPS** (Board Mới) |
| :--- | :--- | :--- |
| **Tấm nền LCD** | TFT TN (Góc nhìn hẹp, dễ bạc màu khi nhìn nghiêng) | **IPS TFT 2.8"** (Màu sắc trong trẻo, góc nhìn rộng 178°) |
| **Driver Màn hình** | ILI9341 (SPI) | **ST7789 / ST7789P3** (SPI tốc độ cao) |
| **Độ phân giải** | 240 × 320 pixels | 240 × 320 pixels |
| **Công nghệ cảm ứng** | **Điện trở (Resistive)** chip XPT2046 (phải dùng móng tay/bút stylus) | **Điện dung (Capacitive)** chip CST816D (chạm lướt nhẹ nhàng bằng ngón tay) |
| **Giao thức cảm ứng** | SPI (chung hoặc riêng bus SPI) | **I2C** độc lập (không tốn tài nguyên bus SPI) |
| **Cổng nạp / Nguồn** | Micro-USB hoặc Type-C đời cũ | **Type-C** chuẩn có cặp trở 5.1k CC (tương thích cả cáp C-to-C) |
| **Âm thanh** | Còi Buzzer bị động đơn âm (hoặc LM4871 tùy đợt) | **Audio Amp chuyên dụng** mini kết nối trực tiếp DAC2 (`IO26`), giắc loa 2-pin 1.25mm |
| **Cảm biến quang** | Quang trở LDR hàn góc board | Quang trở LDR SMD (`GT36516`) đo ánh sáng môi trường |
| **Chất lượng gia công** | PCB vàng, linh kiện phổ thông | PCB đen hoàn thiện cao cấp, tích hợp tản nhiệt và jack cắm JST 1.25mm gọn gàng |

---

## 2. THÔNG SỐ KỸ THUẬT PHẦN CỨNG CHI TIẾT

* **Vi điều khiển (MCU):** Module **SparkleIoT XH-32S**, trang bị chip **ESP32-D0WD-V3** (Xtensa® Dual-Core 32-bit LX6 @ 240MHz).
* **Bộ nhớ:** 4MB Flash (SPI), 520KB SRAM, 448KB ROM (có footprint dự phòng lắp thêm chip PSRAM SPI).
* **Kết nối mạng:** Wi-Fi 802.11 b/g/n + Bluetooth 4.2 / BLE.
* **Màn hình hiển thị:**
  * Kích thước: 2.8 inch IPS TFT
  * Độ phân giải: 240(W) × RGB × 320(H)
  * Driver IC: **ST7789P3**
  * Đèn nền (Backlight): 4 bóng LED trắng mắc song song, điều khiển qua MOSFET AO3401 (PWM).
* **Màn cảm ứng (Touch Panel):**
  * Loại: Kính cảm ứng điện dung bề mặt (Capacitive Touch Screen)
  * Driver IC: **CST816D** (hoặc FT6336G)
  * Giao tiếp: I2C (Địa chỉ mặc định: `0x15`)
* **Lưu trữ ngoài:** Khe cắm thẻ nhớ MicroSD (TF Card) chuẩn SPI.
* **Cảm biến & Ngoại vi:**
  * Quang trở LDR (`GT36516`) đo cường độ sáng.
  * LED RGB 3 màu SMD trên bo mạch.
  * 2 nút bấm cứng: `RST` (Reset) và `BOOT` (GPIO 0).
  * Chip chuyển đổi USB-UART: **CH340C** tự động nạp không cần giữ nút.

---

## 3. BẢNG PHÂN BỔ CHÂN (PINOUT REFERENCE TABLE)

> [!WARNING]
> Mặc dù cùng kích thước 2.8" với CYD vàng, các chân tín hiệu **DC, RST, Backlight và Touch** của NiceMCU-32S **hoàn toàn khác biệt**. Tuyệt đối không dùng trực tiếp file cấu hình chân của CYD cũ mà không chỉnh sửa.

### 3.1. Màn hình LCD (ST7789 - SPI)
| Tên tín hiệu | Chân ESP32 | Ghi chú |
| :--- | :---: | :--- |
| `LCD_MOSI` | **GPIO 13** | SPI Master Out Slave In |
| `LCD_CLK` | **GPIO 14** | SPI Serial Clock |
| `LCD_CS` | **GPIO 15** | SPI Chip Select |
| `LCD_DC` | **GPIO 12** | Data / Command (*Lưu ý: CYD cũ là GPIO 2*) |
| `LCD_RST` | **GPIO 2** | Reset phần cứng (*Dùng chung với Touch RST*) |
| `LCD_BL` | **GPIO 25** | Điều khiển độ sáng đèn nền PWM (*Lưu ý: CYD cũ là GPIO 21*) |

### 3.2. Cảm ứng điện dung (CST816D - I2C)
| Tên tín hiệu | Chân ESP32 | Ghi chú |
| :--- | :---: | :--- |
| `TOUCH_SDA` | **GPIO 33** | I2C Data Line |
| `TOUCH_SCL` | **GPIO 32** | I2C Clock Line |
| `TOUCH_INT` | **GPIO 36** | Chân ngắt chạm (SENSOR_VP - chỉ nhận INPUT) |
| `TOUCH_RST` | **GPIO 2** | Dùng chung chân Reset với LCD |
| `I2C_ADDR` | `0x15` | Địa chỉ I2C của chip CST816D |

### 3.3. Khe thẻ nhớ MicroSD (TF Card - SPI)
| Tên tín hiệu | Chân ESP32 | Ghi chú |
| :--- | :---: | :--- |
| `SD_CS` | **GPIO 5** | TF Card Chip Select |
| `SD_MOSI` | **GPIO 23** | VSPI Data Out (CMD) |
| `SD_CLK` | **GPIO 18** | VSPI Clock (SCK) |
| `SD_MISO` | **GPIO 19** | VSPI Data In (DAT0) |

### 3.4. Ngoại vi & Cảm biến tích hợp
| Thiết bị | Chân ESP32 | Ghi chú |
| :--- | :---: | :--- |
| **LED RGB (Đỏ / Red)** | **GPIO 4** | Kích hoạt mức LOW hoặc HIGH tùy đợt bóng |
| **LED RGB (Xanh lục / Green)** | **GPIO 16** | |
| **LED RGB (Xanh lam / Blue)** | **GPIO 17** | |
| **Audio / Loa (Speaker)** | **GPIO 26** | Ngõ ra âm thanh DAC2 qua IC khuếch đại công suất |
| **Quang trở LDR** | **GPIO 34** | Kênh đo analog ADC1_CH6 (chỉ INPUT) |
| **Nút BOOT** | **GPIO 0** | Nhấn kéo xuống GND |

### 3.5. Các giắc cắm mở rộng JST 1.25mm
* **Cổng I2C mở rộng (4-pin):** `3V3`, `SDA (GPIO 21)`, `SCL (GPIO 22)`, `GND`
* **Cổng UART mở rộng (4-pin):** `VIN (5V)`, `TX (GPIO 1)`, `RX (GPIO 3)`, `GND`
* **Cổng Loa ngoài (2-pin):** Ngõ ra từ mạch khuếch đại (đấu loa 8Ω 1W ~ 2W)
* **Các chân GPIO rời:** `GPIO 27`, `GPIO 35`, `GPIO 39`


---

## 4. KHẢ NĂNG NÂNG CẤP PSRAM (FOOTPRINT SMD 8 CHÂN CHƯA HÀN)

Trên mặt bo mạch (gần module ESP32 XH-32S) có một **footprint IC dán SMD 8 chân (SOP-8 / SOIC-8) để trống chưa hàn**. Đây chính là vị trí thiết kế sẵn để người dùng có thể hàn thêm chip **PSRAM (SPI/QSPI RAM)** dung lượng lên tới **8MB**:

* **Mã hiệu trong sơ đồ nguyên lý:** Khối **SPRAM** (Serial PSRAM).
* **Mô tả trong tài liệu sản phẩm hãng:** PSRAM: PSRAM芯片（可选） (Tùy chọn).
* **Sơ đồ chân footprint SOP-8:**
  * Chân 1 (CS#): Nối đường CMD (Chip Select)
  * Chân 2 (DO): Nối đường SD0 (Data In/Out)
  * Chân 3 (WP): Nối đường SD3 (Write Protect / Quad SPI Data)
  * Chân 4 (GND): Nối GND
  * Chân 5 (DI): Nối đường SD1 (Data In/Out)
  * Chân 6 (CLK): Nối đường CLK (SPI Clock)
  * Chân 7 (HOLD): Nối đường SD2 (Hold / Quad SPI Data)
  * Chân 8 (VCC): Nối nguồn **3.3V** (có tụ lọc gốm 100nF đặt ngay cạnh)
* **Loại chip tương thích để mua hàn thêm:**
  * **Điện áp bắt buộc:** **3.3V** (Không dùng loại 1.8V).
  * **Đóng gói:** SOP-8 / SOIC-8 (150mil hoặc 208mil, bước chân 1.27mm).
  * **Mã chip khuyến nghị:** **APS6404L-3SQR-SN** (8MB / 64Mbit QSPI PSRAM 3.3V của APMemory, rất phổ biến, giá ~20k VNĐ) hoặc **ESP-PSRAM64H** (8MB 3.3V của Espressif).
* **Khi nào cần hàn thêm PSRAM?**
  * Với UI SmartDesk V2 hiện tại: **Chưa bắt buộc** vì kiến trúc component sprites nội bộ đã tối ưu trong 520KB SRAM nội.
  * Cần thiết khi: Nâng cấp lên **LVGL 9.x Full Double Buffering 320x240** (tốn >300KB RAM đồ họa), chạy animation ảnh động dung lượng lớn, hoặc tích hợp voice/AI assistant mini.
* **Cấu hình kích hoạt trong PlatformIO sau khi hàn:**
  `ini
  build_flags =
      -D BOARD_HAS_PSRAM
      -mfix-esp32-psram-cache-issue
  `

## 5. HƯỚNG DẪN CẤU HÌNH CHO DỰ ÁN CYD_SMARTDESK_V2

Để chạy firmware SmartDesk V2 trên bo mạch mới này, thêm một môi trường build mới vào file [platformio.ini](file:///D:/Projects_2026/CYD_SmartDesk_V2/platformio.ini):

### 5.1. Cấu hình PlatformIO (`platformio.ini`)
```ini
[env:nicemcu_32s_2_8ips]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = huge_app.csv

build_flags =
    -D CORE_DEBUG_LEVEL=3
    ; ── Cấu hình Driver TFT_eSPI cho NiceMCU 2.8" IPS ──
    -D USER_SETUP_LOADED=1
    -D USE_HSPI_PORT=1
    -D ST7789_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=320
    -D TFT_MOSI=13
    -D TFT_SCLK=14
    -D TFT_CS=15
    -D TFT_DC=12
    -D TFT_RST=2
    -D TFT_BL=25
    -D TFT_BACKLIGHT_ON=HIGH
    -D TFT_INVERSION_ON=1
    ; ── Tốc độ SPI ──
    -D SPI_FREQUENCY=40000000
    -D SPI_READ_FREQUENCY=20000000
    ; ── Định nghĩa chân phần cứng mở rộng ──
    -D BOARD_HAS_CAPACITIVE_TOUCH=1
    -D PIN_TOUCH_SDA=33
    -D PIN_TOUCH_SCL=32
    -D PIN_TOUCH_INT=36
    -D PIN_AUDIO_DAC=26
    -D PIN_LDR_SENSOR=34
    -D PIN_RGB_RED=4
    -D PIN_RGB_GREEN=16
    -D PIN_RGB_BLUE=17
    ; ── Thư viện Font ──
    -D LOAD_GLCD=1
    -D LOAD_FONT2=1
    -D LOAD_FONT4=1
    -D LOAD_FONT6=1
    -D LOAD_FONT7=1
    -D LOAD_FONT8=1
    -D LOAD_GFXFF=1
    -D SMOOTH_FONT=1

lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    bblanchon/ArduinoJson @ ^7.1.0
    tzapu/WiFiManager @ ^2.0.17
    esphome/ESPAsyncWebServer-esphome @ ^3.1.0
    esphome/AsyncTCP-esphome @ ^2.0.0
    ricmoo/QRCode @ ^0.0.1
    ; Driver cảm ứng điện dung CST816D
    https://github.com/fbiego/CST816D.git
```

### 5.2. Khởi tạo Cảm ứng Điện dung CST816D trong mã nguồn C++
Thay thế việc đọc `tft.getTouch(&x, &y)` của XPT2046 cũ bằng thư viện I2C:

```cpp
#include <Wire.h>
#include <CST816D.h>

CST816D touch(33, 32, 2, 36); // SDA, SCL, RST, INT

void setupTouch() {
    touch.begin();
}

bool checkTouch(uint16_t &x, uint16_t &y) {
    if (touch.getPosition(x, y)) {
        // Màn hình 320x240 ngang: ánh xạ tọa độ nếu cần xoay hướng
        return true;
    }
    return false;
}
```

### 5.3. Điều khiển độ sáng đèn nền PWM (GPIO 25)
```cpp
const int LEDC_CHANNEL_BL = 0;
const int LEDC_FREQ_BL = 1000;
const int LEDC_RES_BL = 8;

void setupBacklight() {
    ledcSetup(LEDC_CHANNEL_BL, LEDC_FREQ_BL, LEDC_RES_BL);
    ledcAttachPin(25, LEDC_CHANNEL_BL);
    ledcWrite(LEDC_CHANNEL_BL, 200); // Độ sáng 0 - 255
}
```

---

## 6. TÀI NGUYÊN THAM KHẢO CHÍNH THỨC

* **Repository mẫu từ nhà sản xuất:** [github.com/NiceMCU-01/NiceMCU-32S-DEV_2.8IPS](https://github.com/NiceMCU-01/NiceMCU-32S-DEV_2.8IPS)
* **Sơ đồ nguyên lý PDF:** `docs/NiceMCU-32S-2.8IPS_原理图V2.0.pdf` (trong repo trên)
* **Sổ tay đặc tả màn hình:** `docs/DIY0159_HS-CT28QV-A082-TPM_ SPEC.pdf`
* **Sổ tay module SparkleIoT XH-32S:** `docs/SPARKLEIOT_XH-32S模组手册V2.0本.docx`
