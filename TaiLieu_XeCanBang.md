# Tài Liệu Kỹ Thuật: Xe Cân Bằng 2 Bánh (ESP32)

Tài liệu này được soạn thảo nhằm mục đích bàn giao dự án Xe Cân Bằng 2 Bánh (Version 3). Bất kỳ ai có kiến thức cơ bản về lập trình C++ và hệ thống nhúng đều có thể đọc, hiểu cấu trúc và tiếp tục phát triển dựa trên bộ mã nguồn này.

---

## 1. Tổng quan hệ thống (System Overview)

- **Vi điều khiển (MCU):** ESP32
- **Cảm biến góc nghiêng (IMU):** MPU6050 (giao tiếp I2C)
- **Động cơ:** Động cơ bước (Stepper Motor) kết hợp với Driver DRV8825. Động cơ bước được chọn vì độ chính xác cao và khả năng giữ vị trí tốt ở tốc độ bằng 0.
- **Phương pháp điều khiển:** Cascade PID (PID vòng xoắn - Vòng ngoài kiểm soát Tốc độ/Vị trí, Vòng trong kiểm soát Góc nghiêng).
- **Chu kỳ điều khiển:** Vòng lặp chính chạy ở tần số **200Hz** (mỗi 5ms cập nhật một lần).
- **Điều khiển động cơ:** Sử dụng Hardware Timer ngắt ở tần số **20kHz** (50µs) với thuật toán **DDS** (Direct Digital Synthesis) để băm xung (STEP) cực kỳ mượt mà.

---

## 2. Cấu trúc mã nguồn & Chức năng các Module

Toàn bộ logic được chia nhỏ thành các module chuẩn mực trong thư mục `src` và `include`:

| File / Module | Chức năng chính |
| :--- | :--- |
| `include/Config.h` | Nơi chứa **tất cả** các thông số phần cứng, chân cắm (Pins), hằng số PID mặc định, tần số ngắt, và cài đặt WiFi. Đọc file này đầu tiên để hiểu các giới hạn của xe. |
| `src/Imu.h`, `Imu.cpp` | Khởi tạo giao tiếp I2C với MPU6050. Đọc dữ liệu thô và tự động hiệu chỉnh (calibrate) Gyro khi khởi động. |
| `include/ImuMath.h` | Chứa thuật toán **Lọc bù (Complementary Filter)** để tính ra góc nghiêng thực tế của xe từ Gia tốc kế và Con quay hồi chuyển. |
| `src/Pid.h` | Lớp (Class) tính toán PID rời rạc, có bao gồm tính năng **Anti-windup** (Chống bão hòa khâu Tích phân I). |
| `src/Balancer.h`, `Balancer.cpp` | Module cực kỳ quan trọng điều khiển trực tiếp phần cứng Động cơ bước. Sử dụng ngắt phần cứng ESP32. |
| `src/Storage.h`, `Storage.cpp` | Lưu trữ và đọc lại các thông số PID, Setpoint vào bộ nhớ Flash của ESP32 để không bị mất khi rút nguồn. |
| `src/WebUI.h`, `WebUI.cpp` | Tạo một Access Point WiFi (`XeCanBang-AP`) và máy chủ Web, phát WebSocket để vẽ biểu đồ và cho phép điều khiển xe qua điện thoại (Joystick). |
| `src/main.cpp` | Nơi ráp nối mọi thứ lại với nhau. Chứa vòng lặp 5ms, đọc IMU, chạy thuật toán Cascade PID, và gửi lệnh cho `Balancer`. |

---

## 3. Phân tích thuật toán & Các tính toán cốt lõi

Đây là phần giải thích "tại sao lại tính toán như vậy" dành cho người tiếp quản dự án.

### 3.1. Tính toán Góc nghiêng (Lọc bù - Complementary Filter)
**Vấn đề:** 
- Con quay hồi chuyển (Gyroscope) đo tốc độ góc (độ/giây) rất mượt và phản ứng ngay lập tức, nhưng nếu tính góc bằng cách cộng dồn theo thời gian (Tích phân) nó sẽ bị "trôi" (Drift) ngày càng sai lệch.
- Gia tốc kế (Accelerometer) đo góc dựa trên trọng lực Trái Đất (luôn đúng về lâu dài) nhưng lại cực kỳ nhiễu khi xe di chuyển hoặc rung lắc.

**Cách giải quyết (`ImuMath.h`):**
```cpp
// Trọng số (weight) dựa trên hằng số COMP_ALPHA = 0.98
góc_tính_toán = (1 - weight) * góc_gia_tốc_kế + weight * góc_con_quay_hồi_chuyển_cộng_dồn;
```
- Lấy 98% sự thay đổi của Gyro (phản ứng nhanh) và dùng 2% của Accelerometer để kéo góc về chuẩn từ từ (chống trôi). Điều này cho ra một góc nghiêng mượt và chính xác để cân bằng.

### 3.2. Điều khiển Cascade PID (Hai vòng lặp lồng nhau)
Nằm tại hàm `loop()` trong `main.cpp`.
Xe cân bằng không chỉ cần đứng thẳng (Vòng trong), mà còn cần đứng yên tại chỗ không trôi, và có thể đi tiến/lùi (Vòng ngoài).

**A. Vòng ngoài (Kiểm soát Tốc độ & Vị trí - PI)**
- Lấy tốc độ hiện tại (ước lượng từ lệnh xuất ra động cơ) trừ đi tốc độ mong muốn (từ Joystick điện thoại). Ta được `sai_số_tốc_độ`.
- Tích phân sai số tốc độ theo thời gian ta sẽ được `khoảng_cách` bị trôi so với vị trí cần đứng.
- Tính ra một **Góc bù (Angle Adjustment)**:
  ```cpp
  angle_adjustment = (speed_error * speed_kp) + (distance * speed_ki);
  ```
- *Tại sao?* Nếu xe đang bị đẩy lùi về phía sau (khoảng cách âm), thuật toán sẽ tạo ra một góc bù. Xe sẽ lừa vòng trong rằng "chưa đạt góc cân bằng", khiến xe tự nghiêng về phía trước để chạy tới lấy lại vị trí ban đầu.

**B. Vòng trong (Kiểm soát Góc nghiêng - PID)**
- `dynamic_setpoint = góc_cân_bằng_chuẩn - angle_adjustment`
- Sai số góc = `dynamic_setpoint - góc_đọc_từ_IMU`.
- Cho sai số này qua bộ PID (`Pid.h`):
  ```cpp
  Output = Kp*error + Ki*integral(error) + Kd*derivative(error);
  ```
- `Output` (đầu ra) chính là **vận tốc quay** cần thiết của bánh xe. Nếu xe ngã về trước -> Output ra dương -> Bánh xe chạy nhanh về trước để đỡ xe lại.

### 3.3. Giới hạn gia tốc (Slew Rate / Acceleration Limiting)
**Vấn đề:** Động cơ bước rất dễ bị "mất bước" (stall) phát ra tiếng bíp bíp chói tai rồi khựng lại nếu bắt nó tăng tốc từ 0 lên 1000 vòng/phút ngay lập tức.
**Giải pháp (trong `main.cpp`):**
```cpp
float max_delta = Config::STEPPER_ACCEL_MAX * dt;
if (output - last_output > max_delta) output = last_output + max_delta;
```
Tốc độ xuất ra động cơ bị ép không được tăng/giảm quá một lượng `max_delta` trong mỗi chu kỳ. Điều này giúp xe tăng tốc êm ái, bám đường tốt và không rớt bước.

### 3.4. Băm xung động cơ cực mượt (DDS)
**Vấn đề:** Lệnh `delayMicroseconds()` trong vòng lặp chính làm code bị khựng, không thể chạy giao diện Web, và động cơ sẽ giật cụt nếu xung không đều.
**Giải pháp (trong `Balancer.cpp`):**
- ESP32 được cấu hình một bộ đếm thời gian (Hardware Timer) ngắt liên tục 20.000 lần mỗi giây (20kHz).
- Bất chấp vòng lặp chính đang làm gì (đang gửi Web, in Serial), cứ đúng 50µs là ESP32 bỏ đó nhảy vào hàm `stepISR()`.
- **DDS (Direct Digital Synthesis):**
  ```cpp
  g_accumL += absSpdL; // Cộng dồn tốc độ yêu cầu
  if (g_accumL >= 20000) {
      g_accumL -= 20000;
      Kích_1_xung_STEP_ra_động_cơ();
  }
  ```
  Nếu yêu cầu tốc độ càng cao, biến `accumL` tăng càng lẹ, số lần chạm mốc 20000 càng nhiều -> Xung phát ra càng dày -> Chạy nhanh. Cực kỳ mượt và không tốn tải CPU để tính toán số phẩy động (float) trong ngắt.

---

## 4. Giao diện WebUI & Telemetry

- Khi bật xe lên, dùng điện thoại hoặc máy tính bắt WiFi: `XeCanBang-AP` (Pass: `12345678`).
- Truy cập vào địa chỉ: `http://192.168.4.1`.
- Giao diện cung cấp:
  - **Telemetry:** Nhìn thấy đồ thị góc nghiêng và tốc độ động cơ theo thời gian thực (để dễ dò PID).
  - **Tuning:** Đổi trực tiếp thông số `Kp`, `Ki`, `Kd` của góc nghiêng và Tốc độ. Bấm "Save" thì nó sẽ gọi xuống `Storage::save()` lưu vào Flash.
  - **Joystick:** Joystick cảm ứng điều khiển cho xe chạy tới lùi/xoay trái phải (Gửi dữ liệu qua WebSocket cực kỳ độ trễ thấp).

---

## 5. Hướng dẫn Dò thông số (Tuning) cho người mới

Nếu thay đổi khối lượng xe, cần dò lại thông số theo quy trình sau:
1. Cho `Ki = 0`, `Kd = 0`, `Speed_Kp = 0`, `Speed_Ki = 0`. Chỉ giữ lại `Kp`.
2. Tăng `Kp` dần (từ 50 -> 100 -> 200) cho đến khi xe có thể tự giữ thẳng (nhưng sẽ lắc qua lắc lại rất nhanh quanh điểm cân bằng).
3. Tăng `Kd` (Derivative) dần (từ 0.1 -> 0.5) để giảm sự dao động. `Kd` đóng vai trò như "giảm xóc". Khi xe bắt đầu hết lắc và ngưng rung mạnh là được.
4. Tăng nhẹ `Ki` để loại bỏ sai số nếu phần cứng xe hơi lệch.
5. Cuối cùng mới bật `Speed_Kp` và `Speed_Ki` để xe chống trôi đi nơi khác.

---
*Tài liệu này đã bao quát toàn bộ logic cốt lõi. Chúc bạn thành công trong việc tiếp quản và nâng cấp dự án!*
