# TÀI LIỆU BÀN GIAO DỰ ÁN — Xe Cân Bằng 2 Bánh (ESP32)

> **Phiên bản:** 3.0  
> **Ngày lập:** 2026-09-24  
> **Trạng thái:** Production — Hoạt động ổn định  
> **Đối tượng:** Đội kỹ thuật tiếp quản bảo trì & phát triển

---

## Mục lục

1. [Phase 1: Môi trường & Hướng dẫn Cài đặt](#phase-1-môi-trường--hướng-dẫn-cài-đặt)
2. [Phase 2: Kiến trúc & Cấu trúc Module](#phase-2-kiến-trúc--cấu-trúc-module)
3. [Phase 3: Phân tích Module chi tiết & Luồng dữ liệu](#phase-3-phân-tích-module-chi-tiết--luồng-dữ-liệu)
4. [Phase 4: Bảo trì & Các Ràng buộc đã biết](#phase-4-bảo-trì--các-ràng-buộc-đã-biết)

---

## Phase 1: Môi trường & Hướng dẫn Cài đặt

### 1.1. Tech Stack

| Thành phần | Chi tiết |
|:---|:---|
| **MCU** | ESP32-WROOM-32 (chip ESP32-D0WD-V3, 4 MB Flash) |
| **Framework** | Arduino (thông qua PlatformIO) |
| **Platform** | `espressif32` |
| **Board** | `esp32dev` |
| **Build System** | PlatformIO (CLI hoặc VS Code Extension) |
| **Ngôn ngữ** | C++11 (Arduino ESP32 3.x toolchain) |
| **Phần cứng cảm biến** | MPU6050 (IMU 6-trục, I2C address `0x68`) |
| **Driver động cơ** | DRV8825 × 2 (Stepper Driver, microstepping 1/8) |
| **Động cơ** | Stepper Motor NEMA17 (1.8°/step, 200 steps/rev, 12.1V / 1.8A) |
| **Nguồn cấp** | 12V DC |
| **Serial Baud Rate** | 115200 |

### 1.2. Thư viện phụ thuộc (Dependencies)

Được khai báo trong `platformio.ini`, PlatformIO tự động tải khi build:

| Thư viện | Phiên bản | Mục đích |
|:---|:---|:---|
| `links2004/WebSockets` | `^2.4.1` | WebSocket server trên ESP32 để truyền telemetry real-time |
| `bblanchon/ArduinoJson` | `^6.21.3` | Parse/Serialize JSON cho giao tiếp WebSocket |

Ngoài ra, project sử dụng các thư viện built-in của Arduino ESP32 Core (không cần khai báo thêm):

- `Wire.h` — Giao tiếp I2C với MPU6050
- `WiFi.h` — Tạo Access Point
- `WebServer.h` — HTTP server phục vụ trang WebUI
- `Preferences.h` — Đọc/ghi NVS Flash (lưu thông số PID)

### 1.3. Sơ đồ chân kết nối phần cứng (Pin Mapping)

Được định nghĩa cố định trong `include/Config.h`. **Không được thay đổi** nếu không đấu lại mạch phần cứng.

| Chức năng | GPIO | Ghi chú |
|:---|:---|:---|
| `PIN_EN_L` (Enable Motor Trái) | 14 | Active LOW (DRV8825 nENBL) |
| `PIN_STEP_L` (Step Motor Trái) | 26 | Bank 0 (GPIO 0-31) |
| `PIN_DIR_L` (Direction Motor Trái) | 25 | HIGH = Forward |
| `PIN_EN_R` (Enable Motor Phải) | 27 | Active LOW |
| `PIN_STEP_R` (Step Motor Phải) | 32 | Bank 1 (GPIO 32-39) |
| `PIN_DIR_R` (Direction Motor Phải) | 33 | LOW = Forward (mirrored) |
| `PIN_SDA` (I2C Data) | 21 | Mặc định ESP32 |
| `PIN_SCL` (I2C Clock) | 22 | Mặc định ESP32 |

> **Lưu ý quan trọng:** Motor trái và motor phải có chiều quay **đảo ngược nhau** (mirrored). `DIR=HIGH` trên motor trái = tiến, nhưng trên motor phải = lùi.

### 1.4. Hướng dẫn Cài đặt từng bước

#### Bước 1: Cài đặt PlatformIO

```bash
# Cách 1: Cài PlatformIO CLI qua pip
pip install platformio

# Cách 2 (khuyến nghị): Cài VS Code Extension
# Tìm "PlatformIO IDE" trong VS Code Marketplace → Install
```

#### Bước 2: Clone repository

```bash
git clone <repository-url> xecanbangv3
cd xecanbangv3
```

#### Bước 3: Build firmware

```bash
# PlatformIO tự động tải framework, toolchain, và dependencies khi build lần đầu
pio run
```

> Build lần đầu sẽ mất 3–5 phút do cần tải `espressif32` platform (~300 MB).

#### Bước 4: Upload firmware lên ESP32

```bash
# Kết nối ESP32 qua USB, xác định COM port
pio run --target upload

# Nếu cần chỉ định port cụ thể:
pio run --target upload --upload-port /dev/cu.usbserial-XXXX   # macOS
pio run --target upload --upload-port COM9                      # Windows
```

#### Bước 5: Mở Serial Monitor

```bash
pio device monitor
```

Cấu hình monitor đã được thiết lập sẵn trong `platformio.ini`:

| Tham số | Giá trị | Ý nghĩa |
|:---|:---|:---|
| `monitor_speed` | `115200` | Baud rate |
| `monitor_echo` | `yes` | Hiển thị ký tự đã nhập |
| `monitor_filters` | `send_on_enter, time, log2file` | Gửi khi Enter, thêm timestamp, tự ghi log ra file |

#### Bước 6: Kết nối WebUI

1. Bật nguồn ESP32 → Chờ Serial in `[WebUI] AP Started`
2. Trên điện thoại/máy tính, kết nối WiFi: **`XeCanBang-AP`** (Mật khẩu: **`12345678`**)
3. Truy cập: **`http://192.168.4.1`**

### 1.5. Lệnh Serial (Debug / Tuning)

Giao tiếp qua Serial Monitor ở 115200 baud:

| Lệnh | Ví dụ | Chức năng |
|:---|:---|:---|
| `p<val>` | `p20.5` | Thay đổi Kp |
| `i<val>` | `i0.3` | Thay đổi Ki |
| `d<val>` | `d1.2` | Thay đổi Kd |
| `a<val>` | `a-1.5` | Thay đổi angle offset (setpoint) |
| `s` | `s` | Emergency stop / Resume |
| `v` | `v` | Bật/tắt verbose debug output |
| `h` | `h` | Hiển thị bảng trợ giúp |

### 1.6. Test Suite

Project hiện **không có automated test suite** (thư mục `test/` trống). Quy trình kiểm thử hoàn toàn dựa trên hardware-in-the-loop:

- Upload firmware → Đặt xe trên mặt phẳng → Quan sát calibration qua Serial → Kiểm tra cân bằng
- File `include/MotorTest.h` và `include/MotorPulsePlan.h` cung cấp utility test motor riêng lẻ (chạy từng motor, đếm xung) nhưng không được tích hợp vào `main.cpp` hiện tại

---

## Phase 2: Kiến trúc & Cấu trúc Module

### 2.1. Cây thư mục dự án

```
xecanbangv3/
├── platformio.ini            # Cấu hình build, board, dependencies
├── TaiLieu_XeCanBang.md      # Tài liệu gốc (tiếng Việt)
├── HANDOVER_DOC.md           # Tài liệu bàn giao (file hiện tại)
├── .gitignore
│
├── include/                  # Header-only modules (shared constants, math)
│   ├── Config.h              # ★ Tất cả hằng số cấu hình hệ thống
│   ├── ImuMath.h             # Complementary filter + calibration math
│   ├── MotorPulsePlan.h      # Cấu trúc DDS cho test motor đơn lẻ
│   ├── MotorTest.h           # API test motor (không dùng trong production)
│   └── README
│
├── src/                      # Source chính
│   ├── main.cpp              # ★ Entry point — setup(), loop(), cascade PID
│   ├── Imu.h / Imu.cpp       # MPU6050 I2C driver + calibration
│   ├── Pid.h                 # Bộ điều khiển PID rời rạc (header-only)
│   ├── Balancer.h / .cpp     # ★ Hardware timer ISR — DDS stepper driver
│   ├── Storage.h / .cpp      # NVS Flash persistence (Preferences)
│   └── WebUI.h / .cpp        # WiFi AP + HTTP + WebSocket server + UI
│
├── lib/                      # Thư viện bên thứ 3 cục bộ (trống, dùng lib_deps)
├── test/                     # Unit tests (trống)
├── tools/                    # Build scripts (trống)
└── logs/                     # Device monitor logs (84 file, .gitignore'd)
```

### 2.2. Architectural Pattern

Dự án sử dụng **kiến trúc Modular Monolith** trên nền tảng embedded single-core (thực tế ESP32 dual-core nhưng code chạy trên 1 core mặc định của Arduino framework). Cụ thể:

- **Không có RTOS tasks** — Toàn bộ logic chạy trong vòng lặp `loop()` polling-based ở 200 Hz
- **ISR tách biệt** — Duy nhất hàm `stepISR()` chạy ngoài context vòng lặp chính, trên hardware timer interrupt 20 kHz
- **Event-driven callbacks** — WebUI sử dụng mô hình callback registration (`onSave`, `onUpdate`, `onToggleStop`, `onJoystick`) để giao tiếp ngược về `main.cpp`
- **Shared mutable state** — Các biến `g_stopped`, `g_setpoint`, `g_joy_x/y` được chia sẻ trực tiếp giữa `main.cpp` và callbacks (không có mutex, chấp nhận được vì chạy cùng thread)

### 2.3. Sơ đồ tương tác giữa các Module

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            main.cpp (loop @ 200 Hz)                    │
│                                                                         │
│  ┌──────────┐    pitch, rate     ┌──────────┐     output      ┌──────────────┐
│  │   Imu    │ ──────────────────>│ Cascade  │ ───────────────>│  Balancer    │
│  │ (MPU6050)│                    │   PID    │   (steps/s)     │ (HW Timer   │
│  │          │                    │  Logic   │                 │  ISR 20kHz) │
│  └──────────┘                    └──────────┘                 └──────────────┘
│       │                               ^                            │
│       │                               │                            │ DDS pulse
│       v                               │                            v
│  ┌──────────┐                    ┌──────────┐               ┌──────────────┐
│  │ ImuMath  │                    │   Pid    │               │  DRV8825 x2  │
│  │ (Comp.   │                    │ (P+I+D) │               │  Stepper     │
│  │ Filter)  │                    └──────────┘               │  Motors      │
│  └──────────┘                         ^                     └──────────────┘
│                                       │
│                     ┌─────────────────┴──────────────────┐
│                     │           WebUI                     │
│                     │  (WiFi AP + HTTP + WebSocket)       │
│                     │                                     │
│                     │  <── Joystick (x,y)                │
│                     │  <── PID update / save / stop       │
│                     │  ──> Telemetry (pitch, speed)       │
│                     └─────────────────┬──────────────────┘
│                                       │
│                                 ┌──────────┐
│                                 │ Storage  │
│                                 │ (NVS     │
│                                 │  Flash)  │
│                                 └──────────┘
└─────────────────────────────────────────────────────────────────────────┘
```

### 2.4. Luồng thực thi chính (Main Execution Flow)

```
BOOT
  │
  ├── Serial.begin(115200)
  ├── Storage::begin()  →  Đọc PID params từ NVS Flash
  ├── Balancer::begin() →  Init GPIO + Start HW Timer ISR (20 kHz)
  ├── Imu::begin()      →  Init I2C + Config MPU6050 registers
  ├── Imu::calibrate()  →  Thu thập 500 mẫu gyro bias (~2.5s)
  ├── WebUI::begin()    →  Start WiFi AP + HTTP + WebSocket server
  └── Balancer::enable(true) → Bật motor drivers

LOOP (mỗi 5ms = 200 Hz)
  │
  ├── WebUI::loop()              →  Handle HTTP + WebSocket events
  ├── handleSerial()             →  Parse lệnh Serial (nếu có)
  ├── Imu::update()              →  Đọc raw data + Complementary Filter
  ├── Kiểm tra FALL_ANGLE (40°)  →  Nếu ngã → stop + reset
  ├── Speed/Position Loop (PI)   →  Tính angle_adjustment từ speed error
  ├── Pid::compute()             →  PID chính trên góc nghiêng
  ├── Acceleration Limiting      →  Slew rate 80000 steps/s²
  ├── Balancer::setSpeeds()      →  Cập nhật tốc độ motor (L, R)
  ├── Debug output (mỗi 100ms)  →  In telemetry ra Serial
  └── WebUI::sendTelemetry()     →  Gửi pitch + speed qua WebSocket (20 Hz)
```

---

## Phase 3: Phân tích Module chi tiết & Luồng dữ liệu

### 3.1. Module `Config.h` — Trung tâm Cấu hình

**Mục đích:** Tập trung toàn bộ hằng số cấu hình hệ thống vào một file duy nhất. Bất kỳ thay đổi nào về phần cứng hoặc tuning đều bắt đầu từ file này.

**Phân nhóm thông số:**

#### Nhóm 1: Phần cứng I/O (Không thay đổi)

| Hằng số | Giá trị | Mô tả |
|:---|:---|:---|
| `PIN_EN_L/R` | 14 / 27 | Enable pin cho DRV8825 (Active LOW) |
| `PIN_STEP_L/R` | 26 / 32 | Step pulse output |
| `PIN_DIR_L/R` | 25 / 33 | Direction control |
| `PIN_SDA / PIN_SCL` | 21 / 22 | I2C bus cho MPU6050 |

#### Nhóm 2: Thông số Động cơ

| Hằng số | Giá trị | Mô tả |
|:---|:---|:---|
| `MOTOR_STEPS_PER_REV` | 200 | 360° / 1.8° per step |
| `MICROSTEP` | 8 | 1/8 microstepping (set bằng jumper phần cứng) |
| `MOTOR_SUPPLY_V` | 12.0 V | Nguồn cấp driver |
| `WHEEL_DIAMETER_MM` | 80 mm | Đường kính bánh xe |

#### Nhóm 3: Bộ lọc & Vòng điều khiển

| Hằng số | Giá trị | Mô tả |
|:---|:---|:---|
| `COMP_ALPHA` | 0.98 | Complementary filter: 98% gyro, 2% accel |
| `LOOP_INTERVAL_MS` | 5 | Chu kỳ vòng lặp chính (200 Hz) |
| `FALL_ANGLE_DEG` | 40.0° | Ngưỡng ngã — tự động dừng motor |
| `PID_KP / KI / KD` | 200.0 / 0.6 / 0.5 | Giá trị PID mặc định (góc) |
| `BALANCE_SETPOINT` | -6.0° | Góc cân bằng cơ học (phụ thuộc trọng tâm xe) |
| `PID_OUT_MAX` | 15000 steps/s | Giới hạn output PID |
| `PID_I_MAX` | 400 | Anti-windup clamp cho tích phân |
| `STEPPER_ACCEL_MAX` | 80000 steps/s² | Slew rate tối đa |
| `SPEED_KP / SPEED_KI` | 0.005 / 0.0002 | Thông số vòng ngoài (tốc độ/vị trí) |

#### Nhóm 4: WiFi & WebUI

| Hằng số | Giá trị | Mô tả |
|:---|:---|:---|
| `WIFI_AP_SSID` | `"XeCanBang-AP"` | Tên Access Point |
| `WIFI_AP_PASS` | `"12345678"` | Mật khẩu WiFi (8 ký tự tối thiểu cho WPA2) |
| `WEBSERVER_PORT` | 80 | HTTP server port |
| `WEBSOCKET_PORT` | 81 | WebSocket server port |
| `TELEMETRY_INTERVAL_MS` | 50 | Chu kỳ gửi telemetry (20 Hz) |

#### Nhóm 5: Hardware Timer

| Hằng số | Giá trị | Mô tả |
|:---|:---|:---|
| `STEPPER_TIMER_HZ` | 1,000,000 | Base clock 1 MHz |
| `STEPPER_ALARM_TICKS` | 50 | Alarm mỗi 50 ticks → ISR 20 kHz |
| `STEPPER_ISR_HZ` | 20,000 | Tần số ISR thực tế (dùng làm DDS threshold) |

---

### 3.2. Module `Imu` — Đọc cảm biến & Lọc góc nghiêng

**Mục đích:** Giao tiếp I2C với MPU6050, đọc dữ liệu thô (accelerometer + gyroscope), hiệu chuẩn gyro bias khi khởi động, và tính góc nghiêng chính xác qua complementary filter.

**File:** `src/Imu.h` / `src/Imu.cpp`

**Key Entry Points:**

| Hàm / Phương thức | Chữ ký | Mô tả |
|:---|:---|:---|
| `begin()` | `bool begin(uint8_t addr = 0x68)` | Init I2C, reset MPU6050, cấu hình DLPF 44 Hz, ±250°/s gyro, ±2g accel |
| `calibrate()` | `void calibrate(void(*cb)(uint32_t) = nullptr)` | Thu thập 500 mẫu gyro bias. Robot **phải đứng yên**. Timeout 60s |
| `update()` | `bool update()` | Đọc 14 bytes burst (accel+temp+gyro), chạy complementary filter. Gọi **mỗi chu kỳ loop** |
| `pitchDeg()` | `float pitchDeg() const` | Trả về góc pitch hiện tại (độ). **Đây là giá trị chính dùng cho PID** |
| `pitchRateDegS()` | `float pitchRateDegS() const` | Tốc độ góc pitch (°/s), dùng cho debug |

**Luồng dữ liệu:**

```
MPU6050 (I2C 0x68)
    │
    │ readBurst(0x3B, 14 bytes)
    v
Raw {ax, ay, az, gx, gy, gz}  ← int16_t, big-endian
    │
    │ Trừ gyro bias (calibrated)
    │ Scale: accel / 16384, gyro / 131
    v
ImuMath::Estimate::update()
    │
    │ accelPitch = atan2(-ax, az) × RAD_TO_DEG
    │ gyroPitch  = previous_pitch + rateY × dt
    │ weight     = COMP_ALPHA ^ (dt / 0.005)
    │ pitch      = weight × gyroPitch + (1 - weight) × accelPitch
    v
estimate.y  ← Góc pitch (độ) → dùng bởi main.cpp
```

**Điểm cần chú ý cho đội bảo trì:**

- MPU6050 được cấu hình **DLPF = 44 Hz** (`REG_CONFIG = 0x03`) để lọc nhiễu rung cơ khí. Nếu tăng lên sẽ phản ứng nhanh hơn nhưng nhiễu hơn.
- Complementary filter trong `ImuMath.h` có **adaptive weight** — hệ số alpha được điều chỉnh theo `dt` thực tế (không cố định), đảm bảo hành vi nhất quán ngay cả khi loop bị jitter: `weight = alpha^(dt/0.005)`.
- Calibration **chỉ hiệu chuẩn gyro bias**, không hiệu chuẩn accelerometer. Nếu cảm biến bị lệch cơ học, cần điều chỉnh `BALANCE_SETPOINT` trong `Config.h`.
- Hàm `wrap()` trong `ImuMath.h` đảm bảo góc luôn nằm trong khoảng [-180°, +180°], tránh hiện tượng nhảy 360° khi qua ranh giới.

---

### 3.3. Module `ImuMath` — Thuật toán Lọc & Hiệu chuẩn

**Mục đích:** Header-only library chứa toàn bộ logic toán học cho IMU: hằng số scale, complementary filter, thống kê online (Welford's algorithm), và quy trình calibration.

**File:** `include/ImuMath.h`

**Các struct quan trọng:**

| Struct | Mô tả |
|:---|:---|
| `Raw` | Dữ liệu thô MPU6050: `ax, ay, az, gx, gy, gz` (int16_t) |
| `Estimate` | State của complementary filter: `accelX/Y`, `x/y` (góc lọc), `rateX/Y/Z`, `normG` |
| `Stats` | Online mean/variance tracker (Welford's algorithm) cho calibration |
| `Calibration` | State machine thu thập 500+ mẫu, phát hiện chuyển động bất thường, tính gyro bias |

**Thuật toán Complementary Filter (trong `Estimate::update()`):**

```cpp
// Bước 1: Tính góc từ accelerometer (ổn định lâu dài, nhiễu ngắn hạn)
accelY = atan2(-ax, az) * RAD_TO_DEG;

// Bước 2: Tính góc dự đoán từ gyroscope (chính xác ngắn hạn, trôi dài hạn)
predictedY = wrap(y + rateY * dt);

// Bước 3: Adaptive blending (trọng số thích ứng theo dt thực)
weight = pow(alpha, dt / 0.005f);    // alpha = 0.98 ở dt chuẩn = 5ms
y = wrap(predictedY + (1 - weight) * wrap(accelY - predictedY));
```

**Calibration Safety Guards:**

- Gia tốc kế phải đo được ~1g (norm thuộc [0.8, 1.2]) — nếu không, mẫu bị reject
- Nếu accelerometer thay đổi > 0.035g so với mẫu đầu tiên → phát hiện chuyển động → reject
- Nếu gyro rate > 10°/s hoặc lệch > 3 sigma so với mean → reject
- Timeout 60 giây — nếu không thu đủ 500 mẫu hợp lệ, bias đặt = 0

---

### 3.4. Module `Pid` — Bộ điều khiển PID rời rạc

**Mục đích:** Tính toán PID output từ error giữa setpoint và measured value, với anti-windup và output clamping.

**File:** `src/Pid.h` (Header-only, 62 dòng)

**Key Entry Points:**

| Phương thức | Mô tả |
|:---|:---|
| `Pid(kp, ki, kd, outMax, iMax)` | Constructor — truyền thông số ban đầu |
| `compute(setpoint, measured, dt)` | Tính 1 bước PID, trả về output bị clamp trong [-outMax, +outMax] |
| `reset()` | Reset integrator và derivative state. **Bắt buộc gọi** khi thay đổi PID params |

**Công thức:**

```
error = setpoint - measured

P = Kp * error
I += Ki * error * dt          (clamp: [-iMax, +iMax])
D = Kd * (error - prevError) / dt   (skip ở lần chạy đầu)

output = clamp(P + I + D, -outMax, +outMax)
```

**Cơ chế Anti-windup:**

- Thành phần tích phân bị giới hạn cứng ở `[-PID_I_MAX, +PID_I_MAX]` (mặc định ±400)
- Nếu `dt <= 0` hoặc `dt > 0.5s` → trả về 0 (bảo vệ tràn khi reboot hoặc lag nghiêm trọng)

---

### 3.5. Module `Balancer` — Điều khiển Stepper qua Hardware Timer ISR

**Mục đích:** Module quan trọng nhất về mặt phần cứng. Điều khiển chính xác tần số xung STEP cho 2 motor stepper thông qua DDS (Direct Digital Synthesis) trên hardware timer interrupt 20 kHz.

**File:** `src/Balancer.h` / `src/Balancer.cpp`

**Key Entry Points:**

| Hàm | Mô tả |
|:---|:---|
| `begin()` | Init GPIO, set EN pins HIGH (disabled), start HW timer |
| `enable(bool)` | Bật/tắt motor driver. EN=LOW = active (DRV8825) |
| `setSpeeds(int32_t L, int32_t R)` | Đặt tốc độ (steps/s) cho 2 bánh. Giá trị dương = tiến |
| `stop()` | Dừng khẩn cấp: speed=0, disable motors |
| `isEnabled()`, `currentSpeedL/R()` | Đọc trạng thái hiện tại |

**Thuật toán DDS (Direct Digital Synthesis) — trong `stepISR()`:**

```
Mỗi 50µs (20 kHz):
  1. Hạ STEP pin nếu có pending từ tick trước
  2. Nếu disabled → return
  3. Đặt DIR pin theo dấu tốc độ (trước STEP >= 200ns — DRV8825 setup time)
  4. accumulator += |speed|
  5. Nếu accumulator >= 20000 (ISR_HZ):
       accumulator -= 20000
       Nâng STEP pin HIGH → 1 xung
       Đánh dấu pending LOW cho tick sau
```

**Tại sao DDS:** Cho phép tạo tần số xung bất kỳ từ 1 đến 20000 steps/s mà không cần phép chia hoặc float trong ISR. Chỉ dùng phép cộng và so sánh số nguyên → tốc độ thực thi cực nhanh (< 1µs).

**Tối ưu hiệu năng quan trọng:**

- GPIO được điều khiển bằng **direct register write** (`GPIO_OUT_W1TS_REG`, `GPIO_OUT_W1TC_REG`), không dùng `digitalWrite()` (chậm ~100x)
- Motor trái dùng Bank 0 (GPIO 0-31), motor phải dùng Bank 1 (GPIO 32-39) — cần macro riêng (`B0_H/L` vs `B1_H/L`)
- ISR được đánh dấu `IRAM_ATTR` để chạy từ RAM, không bị delay bởi SPI Flash cache miss
- Bitmask được tính `constexpr` tại compile-time

**Chuỗi timing 1 STEP pulse:**

```
Tick N:   DIR set ──> STEP HIGH ──> pendLow = true
Tick N+1: STEP LOW   (width = 50µs >> 1.9µs minimum của DRV8825)
```

---

### 3.6. Module `Storage` — Lưu trữ thông số NVS Flash

**Mục đích:** Persist các thông số PID và setpoint vào non-volatile storage (NVS Flash) của ESP32 để giữ lại cấu hình khi mất nguồn.

**File:** `src/Storage.h` / `src/Storage.cpp`

**Key Entry Points:**

| Hàm | Mô tả |
|:---|:---|
| `begin()` | Mở NVS namespace `"balancer"` ở chế độ read/write |
| `load(...)` | Đọc 6 thông số (`kp, ki, kd, sp, skp, ski`). Nếu chưa có → dùng giá trị default |
| `save(...)` | Ghi 6 thông số vào Flash |

**NVS Keys:**

| Key | Kiểu | Thông số |
|:---|:---|:---|
| `"kp"` | `float` | PID Kp (góc) |
| `"ki"` | `float` | PID Ki (góc) |
| `"kd"` | `float` | PID Kd (góc) |
| `"sp"` | `float` | Balance setpoint (độ) |
| `"skp"` | `float` | Speed loop Kp |
| `"ski"` | `float` | Speed loop Ki |

> **Lưu ý:** NVS Flash có giới hạn chu kỳ ghi (~100,000 lần). Không nên gọi `save()` liên tục trong vòng lặp. Hiện tại `save()` chỉ được gọi khi người dùng nhấn nút "LƯU VÀO FLASH" trên WebUI.

---

### 3.7. Module `WebUI` — Giao diện điều khiển không dây

**Mục đích:** Tạo WiFi Access Point, phục vụ trang HTML dashboard qua HTTP, và truyền dữ liệu real-time qua WebSocket để tuning PID và điều khiển xe từ xa.

**File:** `src/WebUI.h` / `src/WebUI.cpp`

**Key Entry Points:**

| Hàm | Mô tả |
|:---|:---|
| `begin(kp, ki, kd, sp, skp, ski)` | Start WiFi AP + HTTP server (port 80) + WebSocket server (port 81) |
| `loop()` | Xử lý HTTP requests và WebSocket events. **Phải gọi mỗi vòng lặp** |
| `sendTelemetry(pitch, output)` | Broadcast JSON `{type:"telemetry", pitch, speed}` cho tất cả clients |
| `onSave(cb)` | Đăng ký callback khi client nhấn "LƯU VÀO FLASH" |
| `onUpdate(cb)` | Đăng ký callback khi client thay đổi slider/input PID |
| `onToggleStop(cb)` | Đăng ký callback khi client nhấn "DỪNG/CHẠY" |
| `onJoystick(cb)` | Đăng ký callback khi client thao tác D-Pad |

**WebSocket Message Protocol:**

| Direction | `type` | Payload | Mô tả |
|:---|:---|:---|:---|
| Server → Client | `init` | `{kp, ki, kd, sp, skp, ski}` | Gửi thông số hiện tại khi client connect |
| Server → Client | `telemetry` | `{pitch, speed}` | Dữ liệu real-time (20 Hz) |
| Client → Server | `update` | `{kp, ki, kd, sp, skp, ski}` | Client thay đổi thông số PID |
| Client → Server | `save` | `{}` | Lưu thông số vào Flash |
| Client → Server | `stop` | `{}` | Toggle emergency stop |
| Client → Server | `joy` | `{x, y}` | Joystick input. x thuộc [-0.2, 0.2], y thuộc [-0.2, 0.2] |

**Kiến trúc WebUI:**

- Trang HTML được embed trực tiếp trong firmware dưới dạng C++ raw string literal (`R"rawliteral(...)rawliteral"`)
- Giao diện dark theme, responsive, hỗ trợ touch events cho mobile
- D-Pad sử dụng `touchstart`/`touchend` events để phát hiện nhấn giữ

---

### 3.8. Module `main.cpp` — Điều phối trung tâm & Cascade PID

**Mục đích:** Entry point của firmware. Khởi tạo tất cả module, chạy vòng điều khiển chính 200 Hz, thực thi thuật toán Cascade PID (2 vòng lồng nhau), và điều phối giao tiếp giữa tất cả thành phần.

**File:** `src/main.cpp`

**Luồng điều khiển Cascade PID (mỗi 5ms):**

```
VÒNG NGOÀI (Speed/Position PI):
  ┌──────────────────────────────────────────────────────────┐
  │ actual_speed = -last_output                              │
  │ filtered_speed = LPF(actual_speed, alpha=0.1)            │
  │ smoothed_joy_y = LPF(joy_y, alpha=0.02)                  │
  │ target_speed = smoothed_joy_y * JOY_MAX_SPEED            │
  │                                                          │
  │ speed_error = filtered_speed - target_speed              │
  │ distance += (filtered_speed - target_speed) * dt         │
  │ distance *= 0.95  (nếu joystick thả)                    │
  │ distance = clamp(distance, +/-20000)                     │
  │                                                          │
  │ angle_adjustment = speed_error * Skp + distance * Ski    │
  │ angle_adjustment = clamp(+/-15 deg)                      │
  │                                                          │
  │ dynamic_setpoint = BALANCE_SETPOINT - angle_adjustment   │
  └──────────────────────────┬───────────────────────────────┘
                             │
                             v
VÒNG TRONG (Angle PID):
  ┌──────────────────────────────────────────────────────────┐
  │ error = dynamic_setpoint - pitch                         │
  │ output = Kp*err + Ki*integral(err) + Kd*derr/dt          │
  │ output = clamp(+/-PID_OUT_MAX)                           │
  │                                                          │
  │ Deadband: if |output| < PID_DEADBAND → output = 0       │
  │                                                          │
  │ Slew Rate Limit:                                         │
  │   max_delta = STEPPER_ACCEL_MAX * dt                     │
  │   output = clamp(output, last +/- max_delta)             │
  │                                                          │
  │ base_speed = -output  (đảo dấu: ngã tới → chạy tới)     │
  │ turn_speed = -smoothed_joy_x * JOY_MAX_TURN              │
  │                                                          │
  │ Balancer::setSpeeds(                                     │
  │     base_speed + turn_speed,    // motor trái            │
  │     base_speed - turn_speed     // motor phải            │
  │ )                                                        │
  └──────────────────────────────────────────────────────────┘
```

**Giải thích tại sao đảo dấu output:**

Khi xe ngã về **phía trước** (pitch > 0), PID tính ra error < 0 → output < 0. Để cứu xe, cần cho bánh chạy **về phía trước** (positive speed) để đỡ xe lại. Do đó: `base_speed = -output`.

**Bộ lọc thông thấp (Low-Pass Filter) được sử dụng:**

| Biến | Hệ số alpha | Mục đích |
|:---|:---|:---|
| `g_filtered_speed` | 0.1 | Làm mượt ước lượng tốc độ, giảm rung |
| `smoothed_joy_y` | 0.02 | Tăng tốc/giảm tốc mềm khi điều khiển |
| `smoothed_joy_x` | 0.10 | Xoay mềm khi rẽ |

**Cơ chế an toàn:**

1. **Fall detection:** Nếu `|pitch| > 40°` → tự động `Balancer::stop()`, `pid.reset()`, đặt `g_stopped = true`
2. **Emergency stop:** Lệnh `s` qua Serial hoặc nút "DỪNG/CHẠY" trên WebUI toggle `g_stopped`
3. **dt guard:** PID trả về 0 nếu `dt > 0.5s` hoặc `dt <= 0`

---

### 3.9. Module hỗ trợ: `MotorPulsePlan` & `MotorTest`

**Mục đích:** Utility chẩn đoán phần cứng — cho phép test từng motor riêng lẻ trước khi chạy thuật toán cân bằng.

**File:** `include/MotorPulsePlan.h` / `include/MotorTest.h`

**Trạng thái:** Không được sử dụng trong `main.cpp` production hiện tại. Dùng trong giai đoạn development để xác nhận hướng quay motor và kiểm tra kết nối phần cứng.

**Đặc điểm kỹ thuật:**

- `MotorPulsePlan` là DDS đơn giản hơn, giới hạn 100 steps/s, tối đa 100 xung, timeout 1 giây
- Có safety guard: `SENSOR_TIMEOUT_US = 25ms` — dừng nếu không nhận được heartbeat từ IMU
- `MotorTest::arm()` phải gọi trước `start()`, expires sau 30 giây — chống kích hoạt nhầm

---

## Phase 4: Bảo trì & Các Ràng buộc đã biết

### 4.1. Technical Debt & Điểm cần cải thiện

| # | Vấn đề | Mức độ | Chi tiết |
|:---|:---|:---|:---|
| 1 | **Không có unit test** | Trung bình | Thư mục `test/` trống. Toàn bộ kiểm thử là manual hardware-in-the-loop. Đề xuất: viết unit test cho `Pid::compute()` và `ImuMath::Estimate::update()` bằng PlatformIO native test runner |
| 2 | **Shared mutable state không có đồng bộ** | Thấp (chấp nhận) | Các biến `g_setpoint`, `g_joy_x/y` được ghi từ WebSocket callback và đọc từ `loop()`. Trên Arduino framework, cả hai chạy cùng thread (task) nên tạm chấp nhận. Tuy nhiên, `g_speedL/R` trong `Balancer` được đọc/ghi giữa ISR và main context — hiện dùng `volatile` nhưng không có `portENTER_CRITICAL()`. Với `int32_t` trên ESP32 (32-bit write là atomic), đây không phải vấn đề thực tế nhưng không đúng chuẩn |
| 3 | **WebUI HTML embed trong firmware** | Thấp | Trang HTML ~5KB được nhúng trực tiếp trong C++ source. Nếu cần mở rộng UI phức tạp hơn, nên chuyển sang SPIFFS/LittleFS |
| 4 | **Ước lượng tốc độ thô** | Trung bình | `actual_speed = -last_output` là ước lượng open-loop, giả định motor không mất bước. Không có encoder feedback. Nếu motor bị stall hoặc mất bước, vòng ngoài sẽ tích lũy sai số |
| 5 | **Calibration chỉ hỗ trợ gyro** | Thấp | Accelerometer không được hiệu chuẩn — offset cơ học được bù bằng `BALANCE_SETPOINT`. Nếu thay cảm biến hoặc thay đổi hướng lắp, cần dò lại setpoint |
| 6 | **NVS Flash wear** | Rất thấp | ESP32 NVS hỗ trợ ~100,000 chu kỳ ghi. `save()` chỉ được gọi khi người dùng chủ động nhấn nút, nhưng không có bảo vệ spam |

### 4.2. Khắc phục sự cố (Troubleshooting)

#### Sự cố 1: Xe bật lên nhưng motor không quay, Serial hiển thị `[FALL] Nga!` liên tục

**Triệu chứng:** Ngay sau boot và calibration, xe liên tục phát hiện "ngã" mặc dù đang được giữ thẳng.

**Nguyên nhân có thể:**

1. **`BALANCE_SETPOINT` sai** — Nếu trọng tâm xe thay đổi (thêm pin, thay khung), giá trị `-6.0°` không còn đúng
2. **MPU6050 lắp sai hướng** — Trục pitch có thể bị đảo, dẫn đến giá trị lớn hơn `FALL_ANGLE_DEG`
3. **Calibration thất bại** — Xe bị rung trong lúc calibrate → bias = 0 → góc đo bị trôi nhanh

**Cách khắc phục:**

```bash
# Bước 1: Bật verbose mode
v

# Bước 2: Quan sát giá trị pitch khi giữ xe thẳng
# Nếu pitch hiện ≈ -30° thay vì ≈ -6°, cần điều chỉnh setpoint

# Bước 3: Thay đổi setpoint qua Serial
a-2.0     # thử giá trị khác

# Bước 4: Nếu tìm được giá trị đúng, lưu vào Flash qua WebUI
```

Nếu calibration thất bại, Serial sẽ in `[IMU] WARN: Calibration timeout` hoặc `[IMU] WARN: Phat hien dao dong`. Cần đảm bảo xe **hoàn toàn yên tĩnh** trên mặt phẳng trong ~3 giây đầu sau boot.

#### Sự cố 2: Motor phát ra tiếng rít/bíp nhưng không quay (Stall)

**Triệu chứng:** Motor kêu chói tai, xe rung nhưng bánh không quay. Có thể xảy ra khi xe bị nghiêng nhiều hoặc vừa mới khởi động lại.

**Nguyên nhân có thể:**

1. **Slew rate quá cao** — `STEPPER_ACCEL_MAX = 80000 steps/s²` có thể vượt quá khả năng moment xoắn tại tốc độ cao
2. **Nguồn 12V yếu** — Khi motor quay nhanh, dòng kéo lớn, điện áp sụt → mất moment
3. **Microstepping jumper lỏng** — Nếu 1 jumper MODE trên DRV8825 bị lỏng, motor nhận sai cấu hình microstepping, dẫn đến bước nhảy thất thường
4. **Vref DRV8825 quá thấp** — Dòng cấp cho motor không đủ

**Cách khắc phục:**

```bash
# Bước 1: Dừng xe
s

# Bước 2: Giảm gia tốc tối đa (trong Config.h)
# STEPPER_ACCEL_MAX = 50000.0f;   // thử giảm còn 50000

# Bước 3: Kiểm tra nguồn 12V bằng đồng hồ vạn năng
# → Phải đo được >= 11.5V khi motor đang quay

# Bước 4: Kiểm tra 3 jumper MODE (M0, M1, M2) trên DRV8825
# Cấu hình 1/8 microstepping: M0=LOW, M1=LOW, M2=HIGH
```

Nếu vấn đề vẫn tồn tại, giảm `PID_OUT_MAX` từ 15000 xuống 10000 để giới hạn tốc độ tối đa motor.

---

### 4.3. Checklist bảo trì định kỳ

| Hạng mục | Tần suất | Thao tác |
|:---|:---|:---|
| Kiểm tra kết nối I2C (SDA/SCL) | Mỗi lần thay/sửa phần cứng | Đo continuity, kiểm tra pull-up 4.7kΩ |
| Kiểm tra nguồn 12V | Hàng tuần (nếu chạy thường xuyên) | Đo điện áp khi motor quay tải nặng |
| Đo Vref DRV8825 | Sau khi thay driver | Vref = Imax * 5 * Rsense (thường 0.1Ω) |
| Backup thông số PID | Sau mỗi lần tuning thành công | Ghi lại Kp, Ki, Kd, SP, Skp, Ski |
| Kiểm tra độ rơ bánh xe | Hàng tháng | Siết lại coupling giữa trục motor và bánh |

---

> **Kết thúc tài liệu bàn giao.** Mọi thắc mắc kỹ thuật vui lòng liên hệ đội phát triển ban đầu kèm log Serial và phiên bản firmware cụ thể.
