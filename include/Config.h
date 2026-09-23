#pragma once

#include <Arduino.h>

// Fixed wiring from prompt.md. Do not change these pins.
#define PIN_EN_L 14
#define PIN_STEP_L 26
#define PIN_DIR_L 25
#define PIN_EN_R 27
#define PIN_STEP_R 32
#define PIN_DIR_R 33
#define PIN_SDA 21
#define PIN_SCL 22

namespace Config {
constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr uint16_t I2C_TIMEOUT_MS = 20;
constexpr uint32_t DEBUG_INTERVAL_MS = 100;

// MOTOR CONFIG: DRV8825, 12 V and 1/8 confirmed by user.
constexpr int MOTOR_STEPS_PER_REV = 200;
// Confirmed from user step angle 1.8 degrees: 360 / 1.8 = 200.
constexpr int MICROSTEP = 8;
constexpr float MOTOR_SUPPLY_V = 12.0f;
constexpr int MOTOR_ENABLE_LEVEL = LOW; // DRV8825 nENBL is active LOW.
constexpr int LEFT_MOTOR_SIGN = 1;      // DEFAULT / NEED direction test.
constexpr int RIGHT_MOTOR_SIGN = 1;     // DEFAULT / NEED direction test.
// Motor label: 12.1 V, 1.8 A, 1.8 DEG (user confirmed).
// Driver Vref adjusted by user; actual current limit/Vref and Rsense not
// measured here. MICROSTEP documents hardware MODE pins; software cannot set
// those pins.

// ROBOT CONFIG
constexpr float WHEEL_DIAMETER_MM = 80.0f;

// COMPLEMENTARY FILTER
// alpha at nominal 5 ms dt: 0.98 = 98% gyro, 2% accel.
constexpr float COMP_ALPHA = 0.98f;

// BALANCING LOOP
constexpr uint32_t LOOP_INTERVAL_MS = 5; // 200 Hz control loop
constexpr float FALL_ANGLE_DEG = 40.0f;  // Stop motors if |angle| > this

// PID INITIAL VALUES (tune via Serial: p/i/d + value + Enter)
constexpr float PID_KP = 200.0f;
constexpr float PID_KI = 0.6f;
constexpr float PID_KD = 0.5f;
constexpr float BALANCE_SETPOINT = -6.0; // degrees; trim if robot drifts
constexpr float PID_OUT_MAX = 15000.0f;  // steps/s, positive = forward
constexpr float PID_I_MAX = 400.0f;      // anti-windup clamp (tighter)
constexpr float PID_DEADBAND = 0.0f;     // steps/s; below = stop (was 80)
constexpr float STEPPER_ACCEL_MAX =
    80000.0f; // steps/s^2; max acceleration to prevent stalling

// SPEED LOOP INITIAL VALUES
constexpr float SPEED_KP = 0.005f;
constexpr float SPEED_KI = 0.0002f;

// JOYSTICK LIMITS
constexpr float JOY_MAX_SPEED =
    3000.0f; // Max target speed from joystick (steps/s)
constexpr float JOY_MAX_TURN = 2000.0f; // Max differential turn speed (steps/s)

// STEPPER HARDWARE TIMER (Arduino ESP32 3.x API)
// timerBegin(STEPPER_TIMER_HZ) -> 1 MHz base clock
// timerAlarm(timer, STEPPER_ALARM_TICKS, true, 0) -> ISR every 50µs -> 20 kHz
constexpr uint32_t STEPPER_TIMER_HZ = 1000000; // Timer base clock: 1 MHz
constexpr uint64_t STEPPER_ALARM_TICKS = 50;   // 50 ticks -> 20 kHz ISR rate
constexpr uint32_t STEPPER_ISR_HZ = 20000; // ISR fires per second (DDS base)

// DIRECTION: mirrored wheels — positive speed = forward on both motors.
// LEFT : DIR=HIGH -> forward, DIR=LOW  -> backward
// RIGHT: DIR=LOW  -> forward, DIR=HIGH -> backward (mirrored)

// WEB UI & WIFI AP CONFIG
constexpr char WIFI_AP_SSID[] = "XeCanBang-AP";
constexpr char WIFI_AP_PASS[] = "12345678";
constexpr uint16_t WEBSERVER_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 50; // 20 Hz
} // namespace Config
