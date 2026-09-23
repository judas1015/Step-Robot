#include "Balancer.h"
#include "Config.h"
#include <Arduino.h>
#include "soc/gpio_reg.h"

// ─── Bitmask constants (compile-time, no runtime branching) ───────────────────
//  Bank 0 (GPIO 0-31): EN_L=14, STEP_L=26, DIR_L=25, EN_R=27
//  Bank 1 (GPIO 32-39): STEP_R=32, DIR_R=33
static constexpr uint32_t M_EN_L   = 1u << PIN_EN_L;            // bank0
static constexpr uint32_t M_STEP_L = 1u << PIN_STEP_L;          // bank0
static constexpr uint32_t M_DIR_L  = 1u << PIN_DIR_L;           // bank0
static constexpr uint32_t M_EN_R   = 1u << PIN_EN_R;            // bank0
static constexpr uint32_t M_STEP_R = 1u << (PIN_STEP_R - 32);   // bank1
static constexpr uint32_t M_DIR_R  = 1u << (PIN_DIR_R - 32);    // bank1

// Volatile register write (expression, not statement — safe in any context)
#define _WR(addr, v) (*(volatile uint32_t*)(addr) = (uint32_t)(v))

// Bank 0
#define B0_H(m) _WR(GPIO_OUT_W1TS_REG,  (m))
#define B0_L(m) _WR(GPIO_OUT_W1TC_REG,  (m))
// Bank 1
#define B1_H(m) _WR(GPIO_OUT1_W1TS_REG, (m))
#define B1_L(m) _WR(GPIO_OUT1_W1TC_REG, (m))

// ─── ISR-shared state ─────────────────────────────────────────────────────────
static volatile int32_t  g_speedL   = 0;
static volatile int32_t  g_speedR   = 0;
static volatile uint32_t g_accumL   = 0;
static volatile uint32_t g_accumR   = 0;
static volatile bool     g_pendLowL = false;
static volatile bool     g_pendLowR = false;
static volatile bool     g_enabled  = false;

static hw_timer_t* g_timer = nullptr;

// ─── Hardware timer ISR (20 kHz) ──────────────────────────────────────────────
// DDS: accumulator += |speed| per tick; step when accumulator >= ISR_HZ.
// DIR is set before STEP rise to satisfy DRV8825 setup time (>= 200 ns).
// STEP pulse width = one ISR period = 50 µs >> DRV8825 minimum 1.9 µs.
static void IRAM_ATTR stepISR() {
    // Lower STEP from previous tick
    if (g_pendLowL) { B0_L(M_STEP_L); g_pendLowL = false; }
    if (g_pendLowR) { B1_L(M_STEP_R); g_pendLowR = false; }

    if (!g_enabled) return;

    const int32_t spdL = g_speedL;
    const int32_t spdR = g_speedR;
    
    if (spdL == 0) g_accumL = 0;
    if (spdR == 0) g_accumR = 0;
    if (spdL == 0 && spdR == 0) return;

    const uint32_t absSpdL = (spdL < 0) ? (uint32_t)(-spdL) : (uint32_t)(spdL);
    const uint32_t absSpdR = (spdR < 0) ? (uint32_t)(-spdR) : (uint32_t)(spdR);

    // DIR: positive speed = forward
    // LEFT : FWD = HIGH, BWD = LOW
    // RIGHT: FWD = LOW (mirrored), BWD = HIGH
    if (spdL > 0) B0_H(M_DIR_L);
    else if (spdL < 0) B0_L(M_DIR_L);
    
    if (spdR > 0) B1_L(M_DIR_R);
    else if (spdR < 0) B1_H(M_DIR_R);

    // Left motor DDS
    g_accumL += absSpdL;
    if (g_accumL >= Config::STEPPER_ISR_HZ) {
        g_accumL -= Config::STEPPER_ISR_HZ;
        B0_H(M_STEP_L);
        g_pendLowL = true;
    }

    // Right motor DDS
    g_accumR += absSpdR;
    if (g_accumR >= Config::STEPPER_ISR_HZ) {
        g_accumR -= Config::STEPPER_ISR_HZ;
        B1_H(M_STEP_R);
        g_pendLowR = true;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void Balancer::begin() {
    const uint8_t pins[] = {
        PIN_EN_L, PIN_STEP_L, PIN_DIR_L,
        PIN_EN_R, PIN_STEP_R, PIN_DIR_R
    };
    for (uint8_t p : pins) {
        pinMode(p, OUTPUT);
        digitalWrite(p, LOW);
    }
    // Disable motors initially (EN = HIGH for DRV8825)
    B0_H(M_EN_L | M_EN_R);

    // New ESP32 Arduino 3.x timer API:
    //   timerBegin(Hz)  — sets base clock in Hz
    //   timerAttachInterrupt(timer, cb)  — no edge parameter
    //   timerAlarm(timer, ticks, autoreload, reload_count)
    g_timer = timerBegin(Config::STEPPER_TIMER_HZ);
    timerAttachInterrupt(g_timer, &stepISR);
    timerAlarm(g_timer, Config::STEPPER_ALARM_TICKS, true, 0);
}

void Balancer::enable(bool on) {
    g_enabled = on;
    if (on) {
        B0_L(M_EN_L | M_EN_R);  // EN=LOW → motors active
    } else {
        g_speedL = 0;
        g_speedR = 0;
        B0_H(M_EN_L | M_EN_R);  // EN=HIGH → motors disabled
    }
}

void Balancer::setSpeeds(int32_t speedLeft, int32_t speedRight) {
    g_speedL = speedLeft;
    g_speedR = speedRight;
}

void Balancer::stop() {
    g_speedL  = 0;
    g_speedR  = 0;
    g_enabled = false;
    B0_H(M_EN_L | M_EN_R);
}

bool    Balancer::isEnabled()     { return g_enabled; }
int32_t Balancer::currentSpeedL() { return g_speedL;  }
int32_t Balancer::currentSpeedR() { return g_speedR;  }
