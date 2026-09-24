/**
 * Xe Can Bang 2 Banh — ESP32 + MPU6050 + DRV8825 Stepper
 * =========================================================
 * Modules:
 *   Imu.h/.cpp      — MPU6050 I2C + ImuMath complementary filter
 *   Pid.h           — Discrete PID with anti-windup
 *   Balancer.h/.cpp — Hardware timer stepper (20 kHz ISR, DDS)
 *   Config.h        — All tunable constants
 *
 * Serial commands (115200 baud):
 *   p<val>  Set Kp           e.g. "p20.5"
 *   i<val>  Set Ki           e.g. "i0.3"
 *   d<val>  Set Kd           e.g. "d1.2"
 *   a<val>  Set angle offset e.g. "a-1.5"
 *   s       Emergency stop / resume
 *   v       Toggle verbose debug output
 *   h       Show this help
 */

#include "Balancer.h"
#include "Config.h"
#include "Imu.h"
#include "Pid.h"
#include "Storage.h"
#include "WebUI.h"
#include <Arduino.h>

// ── Instances
// ─────────────────────────────────────────────────────────────────
static Imu imu;
static Pid pid(Config::PID_KP, Config::PID_KI, Config::PID_KD,
               Config::PID_OUT_MAX, Config::PID_I_MAX);

// ── Runtime state
// ─────────────────────────────────────────────────────────────
static float g_setpoint = Config::BALANCE_SETPOINT;
static float g_speed_kp = Config::SPEED_KP;
static float g_speed_ki = Config::SPEED_KI;
static float g_joy_x = 0.0f;
static float g_joy_y = 0.0f;
static float g_filtered_speed = 0.0f;
static float g_distance = 0.0f;
static bool g_stopped = false; // true = user-requested emergency stop
static bool g_verbose = true;
static uint32_t g_debugTimer = 0;

// ── Serial command handler
// ─────────────────────────────────────────────────────
static void printHelp() {
  Serial.println("+======================================+");
  Serial.println("|   XE CAN BANG — Serial Commands      |");
  Serial.println("+--------------------------------------+");
  Serial.println("|  p<val>  Set Kp  e.g.: p20.0         |");
  Serial.println("|  i<val>  Set Ki  e.g.: i0.3          |");
  Serial.println("|  d<val>  Set Kd  e.g.: d1.2          |");
  Serial.println("|  a<val>  Trim angle offset            |");
  Serial.println("|  s       Stop / Resume               |");
  Serial.println("|  v       Toggle verbose output       |");
  Serial.println("|  h       Show this help              |");
  Serial.println("+======================================+");
  Serial.printf("  Kp=%.2f  Ki=%.2f  Kd=%.2f  SP=%.2f\n", pid.kp, pid.ki,
                pid.kd, g_setpoint);
}

static void handleSerial() {
  if (!Serial.available())
    return;
  String s = Serial.readStringUntil('\n');
  s.trim();
  if (s.length() == 0)
    return;

  char cmd = s[0];
  float val = s.substring(1).toFloat();

  switch (cmd) {
  case 'p':
    pid.kp = val;
    pid.reset();
    Serial.printf(">> Kp = %.3f\n", pid.kp);
    break;
  case 'i':
    pid.ki = val;
    pid.reset();
    Serial.printf(">> Ki = %.3f\n", pid.ki);
    break;
  case 'd':
    pid.kd = val;
    pid.reset();
    Serial.printf(">> Kd = %.3f\n", pid.kd);
    break;
  case 'a':
    g_setpoint = val;
    Serial.printf(">> Setpoint = %.2f deg\n", g_setpoint);
    break;
  case 's':
    g_stopped = !g_stopped;
    if (g_stopped) {
      Balancer::stop();
      Serial.println(">> DUNG KHAN CAP (Motors Disabled)");
    } else {
      pid.reset();
      Balancer::enable(true);
      Serial.println(">> TIEP TUC CAN BANG (Motors Enabled)");
    }
    break;
  case 'v':
    g_verbose = !g_verbose;
    Serial.printf(">> Verbose: %s\n", g_verbose ? "ON" : "OFF");
    break;
  case 'h':
  default:
    printHelp();
    break;
  }
}

// ── Calibration progress callback
// ─────────────────────────────────────────────
static void onCalProgress(uint32_t count) {
  Serial.printf("[CAL] %u / 500 samples...\n", count);
}

// ── Setup
// ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(Config::SERIAL_BAUD);
  delay(500);

  Serial.println("\n[BOOT] Xe Can Bang 2 Banh — ESP32");

  // Load saved PID from flash
  Storage::begin();
  Storage::load(pid.kp, pid.ki, pid.kd, g_setpoint, g_speed_kp, g_speed_ki,
                Config::PID_KP, Config::PID_KI, Config::PID_KD,
                Config::BALANCE_SETPOINT, Config::SPEED_KP, Config::SPEED_KI);

  Serial.println("[BOOT] Khoi tao Balancer (stepper timer)...");
  Balancer::begin();

  Serial.println("[BOOT] Khoi tao MPU6050...");
  if (!imu.begin()) {
    Serial.println("[ERROR] Khong tim thay MPU6050 tren I2C!");
    Serial.println("        Kiem tra: SDA=21, SCL=22, dia chi 0x68");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("[BOOT] MPU6050 OK.");

  // Calibrate gyro (robot must be stationary)
  imu.calibrate(onCalProgress);

  Serial.println("[BOOT] San sang can bang!");
  printHelp();

  // Start WebUI
  WebUI::begin(pid.kp, pid.ki, pid.kd, g_setpoint, g_speed_kp, g_speed_ki);

  WebUI::onSave(
      [](float kp, float ki, float kd, float sp, float skp, float ski) {
        Storage::save(kp, ki, kd, sp, skp, ski);
        Serial.println("[WebUI] Da luu vao Flash!");
      });

  WebUI::onUpdate([](float kp, float ki, float kd, float sp, float skp,
                     float ski) {
    pid.kp = kp;
    pid.ki = ki;
    pid.kd = kd;
    g_setpoint = sp;
    g_speed_kp = skp;
    g_speed_ki = ski;
    pid.reset();
    Serial.printf(
        "[WebUI] Update: Kp=%.2f Ki=%.2f Kd=%.2f SP=%.2f Skp=%.3f Ski=%.4f\n",
        kp, ki, kd, sp, skp, ski);
  });

  WebUI::onToggleStop([]() {
    g_stopped = !g_stopped;
    if (g_stopped) {
      Balancer::stop();
      Serial.println("[WebUI] DUNG KHAN CAP");
    } else {
      pid.reset();
      Balancer::enable(true);
      Serial.println("[WebUI] TIEP TUC");
    }
  });

  WebUI::onJoystick([](float x, float y) {
    g_joy_x = x;
    g_joy_y = y;
  });

  Balancer::enable(true);
  pid.reset();
  g_debugTimer = millis();
}

// ── Main loop (200 Hz)
// ────────────────────────────────────────────────────────
void loop() {
  WebUI::loop();

  // ── 1. Timing ──────────────────────────────────────────────────────────
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if (now - lastMs < Config::LOOP_INTERVAL_MS)
    return;
  const float dt = (now - lastMs) * 0.001f;
  lastMs = now;

  handleSerial();

  if (g_stopped)
    return;

  if (!imu.update())
    return; // Skip if I2C glitch
  const float pitch = imu.pitchDeg();
  const float rate = imu.pitchRateDegS(); // deg/s, for debug

  if (fabsf(pitch) > Config::FALL_ANGLE_DEG) {
    Balancer::stop();
    pid.reset();
    Serial.printf(
        "[FALL] Nga! goc=%.1f deg. Dat lai xe roi nhan 's' de tiep tuc.\n",
        pitch);
    g_stopped = true;
    return;
  }

  // ── 5. Cascaded Speed/Position Loop ────────────────────────────────────
  static float last_output = 0;

  // We use the previous motor output to estimate current speed
  float actual_speed = -last_output; // positive = moving forward

  // Low-pass filter to smooth speed estimation (critical to prevent vibration)
  g_filtered_speed = (0.9f * g_filtered_speed) + (0.1f * actual_speed);

  // Filter joystick input for smoother acceleration
  static float smoothed_joy_y = 0.0f;
  smoothed_joy_y = (0.98f * smoothed_joy_y) + (0.02f * g_joy_y);
  float target_speed = smoothed_joy_y * Config::JOY_MAX_SPEED;
  
  // Integrate speed error to get position (distance error)
  if (g_stopped)
    g_distance = 0; // reset if stopped or fallen
  else
    g_distance += (g_filtered_speed - target_speed) * dt;

  // Decay the integral when joystick is released to prevent windup/wandering
  if (g_joy_y == 0.0f) {
    g_distance *= 0.95f; 
  }

  // Limit distance integral to prevent extreme windup
  g_distance = constrain(g_distance, -20000.0f, 20000.0f);

  // Calculate the angle adjustment required to track target speed
  float speed_error = g_filtered_speed - target_speed;
  float angle_adjustment =
      (speed_error * g_speed_kp) + (g_distance * g_speed_ki);
  
  // Restore full authority to allow robot to pitch enough to recover and accelerate
  angle_adjustment = constrain(angle_adjustment, -15.0f, 15.0f);

  // New dynamic setpoint
  float dynamic_setpoint = g_setpoint - angle_adjustment;

  // Cache prevError BEFORE compute() updates it (for correct D display)
  const float prevErr = pid.prevError();

  // Compute main angle PID using the dynamic setpoint
  float output = pid.compute(dynamic_setpoint, pitch, dt);
  const float err = dynamic_setpoint - pitch;

  // Deadband is removed to allow micro-adjustments near 0, preventing limit cycles
  if (fabsf(output) < Config::PID_DEADBAND)
    output = 0;

  // ── 6. Acceleration Limiting (Slew Rate) ───────────────────────────────
  float max_delta = Config::STEPPER_ACCEL_MAX * dt;
  if (output - last_output > max_delta)
    output = last_output + max_delta;
  else if (output - last_output < -max_delta)
    output = last_output - max_delta;

  // Save output for next iteration speed estimation
  last_output = output;

  // INVERT OUTPUT: if falling forward (pitch > 0, err < 0, output < 0),
  // we need to drive FORWARD (positive speed) to catch the fall.
  float base_speed = -output;
  // Filter joystick input for smoother turning
  static float smoothed_joy_x = 0.0f;
  smoothed_joy_x = (0.90f * smoothed_joy_x) + (0.10f * g_joy_x);
  float turn_speed = -smoothed_joy_x * Config::JOY_MAX_TURN; // Inverted to fix Left/Right directions
  
  Balancer::setSpeeds((int32_t)(base_speed + turn_speed), (int32_t)(base_speed - turn_speed));

  // ── 7. Debug output ────────────────────────────────────────────────────
  if (g_verbose && (now - g_debugTimer >= Config::DEBUG_INTERVAL_MS)) {
    g_debugTimer = now;
    const float dTerm = (dt > 0) ? pid.kd * (err - prevErr) / dt : 0;
    Serial.printf("[BAL] pitch=%+6.2f r=%+5.1f | err=%+6.2f | P=%+7.1f "
                  "I=%+6.1f D=%+6.1f | spd=%+5d\n",
                  pitch, rate, err, pid.kp * err, pid.integral(), dTerm,
                  (int)output);
  }

  // ── 8. WebUI Telemetry ─────────────────────────────────────────────────
  static uint32_t lastTelemetryMs = 0;
  if (now - lastTelemetryMs >= Config::TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    WebUI::sendTelemetry(pitch, output);
  }
}
