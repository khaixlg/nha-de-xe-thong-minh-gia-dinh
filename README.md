# 🚗 Nhà để xe thông minh – ESP32 + Blynk

Hệ thống nhà để xe thông minh dùng **ESP32**, điều khiển và giám sát từ xa qua **Blynk IoT**. Hệ thống có cửa tự động bằng servo, quạt hút tự bật khi phát hiện khí gas/khói, và chống trộm bằng cảm biến siêu âm.

## ✨ Tính năng

- **Đóng/mở cửa**: bằng nút bấm vật lý hoặc công tắc trên app Blynk (hai bên luôn đồng bộ trạng thái).
- **Quạt hút tự động**: đọc cảm biến MQ-2; bật quạt khi nồng độ khí vượt ngưỡng, tắt khi xuống dưới ngưỡng thấp hơn (có độ trễ để quạt không bật/tắt liên tục).
- **Nhận biết có xe**: cảm biến HC-SR04 gắn trên trần, đo khoảng cách để biết trong gara có xe hay không.
- **Chống trộm**:
  - Chỉ bật được khi **đang có xe** (không có xe thì app tự từ chối và nhả công tắc về OFF).
  - Khi đang bật mà xe biến mất liên tiếp → **còi kêu** + **thông báo đẩy** về điện thoại qua Blynk.
- **Màn hình OLED**: hiển thị nồng độ khí, trạng thái xe, chống trộm, quạt, cửa, kết nối WiFi/Blynk.
- **Hoạt động offline**: mất WiFi thì nút bấm, quạt, còi, OLED vẫn chạy bình thường.

## 🧰 Phần cứng

| Linh kiện | Chức năng |
|---|---|
| ESP32 DevKit | Vi điều khiển chính |
| Servo (SG90/MG90S) | Đóng/mở cửa |
| Cảm biến khí MQ-2 | Phát hiện khí gas/khói |
| Cảm biến siêu âm HC-SR04 | Nhận biết có xe (gắn trên trần) |
| OLED SSD1306 128x64 (I2C) | Hiển thị trạng thái |
| Module relay + quạt hút | Thông gió |
| Còi (buzzer) | Báo động chống trộm |
| Nút bấm | Mở/đóng cửa tại chỗ |

### Sơ đồ nối chân

| Thiết bị | Chân ESP32 |
|---|---|
| HC-SR04 – TRIG | GPIO 5 |
| HC-SR04 – ECHO | GPIO 18 |
| MQ-2 (AO) | GPIO 34 |
| Servo (tín hiệu) | GPIO 19 |
| Còi | GPIO 17 |
| Nút bấm (nối GND, dùng `INPUT_PULLUP`) | GPIO 16 |
| Relay quạt (IN) | GPIO 26 |
| OLED – SDA | GPIO 21 |
| OLED – SCL | GPIO 22 |

> ⚠️ HC-SR04 chạy mức 5V, chân ECHO nên qua cầu chia áp (hoặc module đã hỗ trợ 3.3V) để bảo vệ ESP32. Servo và quạt nên cấp nguồn riêng, nối chung GND với ESP32.

## 📱 Cấu hình Blynk

Template: **`gara`**

| Virtual Pin | Kiểu | Hướng | Mô tả |
|---|---|---|---|
| `V0` | Switch (0/1) | App → ESP32 | Mở/đóng cửa |
| `V1` | Switch (0/1) | App → ESP32 | Bật/tắt chống trộm |
| `V2` | LED | ESP32 → App | Trạng thái quạt (255 = đang chạy) |
| `V3` | Value / Gauge | ESP32 → App | Giá trị MQ-2 (gửi mỗi 1 giây) |
| `V4` | LED | ESP32 → App | Cảnh báo trộm (255 = có trộm) |

**Event** cần tạo trong template: `theft_alert` (bật Notifications để nhận thông báo đẩy).

## 📚 Thư viện cần cài (Arduino IDE)

- `Blynk` (BlynkSimpleEsp32)
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `ESP32Servo`

Board: **ESP32 Dev Module** (cài *esp32 by Espressif Systems* trong Boards Manager).

## 🚀 Cài đặt & nạp code

1. Tạo template **`gara`** trên [Blynk Console](https://blynk.cloud), thêm các datastream `V0`–`V4` và event `theft_alert` như bảng trên.
2. Mở `blynk.ino`, điền thông tin của bạn:
   ```cpp
   #define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
   #define BLYNK_TEMPLATE_NAME "gara"
   #define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"

   char ssid[] = "YOUR_WIFI_NAME";
   char pass[] = "YOUR_WIFI_PASSWORD";
   ```
3. Chọn board **ESP32 Dev Module**, chọn cổng COM, bấm **Upload**.
4. Mở Serial Monitor (115200 baud) để xem log.

## ⚙️ Thông số có thể chỉnh

Nằm ở đầu file `blynk.ino`:

| Hằng số | Mặc định | Ý nghĩa |
|---|---|---|
| `DIST_CAR_PRESENT` | `180` | Khoảng cách (cm) ≤ giá trị này thì coi là **có xe** |
| `GAS_ON_TH` | `2000` | MQ-2 lớn hơn mức này thì **bật quạt** |
| `GAS_OFF_TH` | `1499` | MQ-2 nhỏ hơn mức này thì **tắt quạt** |
| `GAS_WARMUP_MS` | `20000` | Thời gian chờ MQ-2 làm nóng (ms), trong lúc này quạt không bật |
| `THEFT_CONFIRM` | `2` | Số lần đo liên tiếp (mỗi 1 giây) mất xe mới báo trộm |
| `GAS_SEND_MS` | `1000` | Chu kỳ gửi dữ liệu khí lên Blynk (ms) |
| `RELAY_ON` / `BUZZER_ON` | `HIGH` | Đổi thành `LOW` nếu relay/còi kích mức thấp |

## 🔍 Nguyên lý hoạt động

- Mỗi **1 giây** hệ thống đọc cảm biến, cập nhật quạt/chống trộm, đẩy dữ liệu lên Blynk và làm mới OLED.
- Khoảng cách được đo **3 lần và lấy giá trị giữa** để lọc nhiễu siêu âm.
- Dữ liệu `V2`, `V4` chỉ gửi khi trạng thái **thay đổi** để tiết kiệm message Blynk.
- Sau khi báo trộm, tắt chống trộm trên app để tắt còi và reset trạng thái.

## 🛠️ Xử lý sự cố

| Hiện tượng | Cách xử lý |
|---|---|
| OLED chỉ nháy sáng 1 lần rồi tối | Kiểm tra dây SDA/SCL (GPIO 21/22), nguồn 3.3V, và địa chỉ I2C (`0x3C`, có thể là `0x3D`) |
| Không bật được chống trộm | Cảm biến đang không thấy xe (khoảng cách > `DIST_CAR_PRESENT`); kiểm tra vị trí gắn HC-SR04 |
| Quạt không chạy / chạy ngược | Đổi `RELAY_ON`/`RELAY_OFF` giữa `HIGH` và `LOW` |
| Quạt không bật trong ~20 giây đầu | Bình thường, MQ-2 đang làm nóng |
| Không kết nối Blynk | Kiểm tra SSID/mật khẩu WiFi, Auth Token và Template ID |

## 🔒 Lưu ý bảo mật

**Không đẩy `BLYNK_AUTH_TOKEN`, tên và mật khẩu WiFi thật lên GitHub.** Hãy dùng placeholder như trên, hoặc tách ra file `secrets.h` và thêm vào `.gitignore`.

## 📄 Giấy phép

Dự án học tập/cá nhân – tự do sử dụng và chỉnh sửa.
