#pragma once
#include <Arduino.h>

/**
 * Pid – discrete PID controller with anti-windup.
 *
 * output = Kp*e + Ki*integral(e) + Kd*de/dt
 *
 * Integral is clamped to [-iMax, +iMax].
 * Output is clamped to [-outMax, +outMax].
 */
class Pid {
public:
    float kp, ki, kd;
    float outMax, iMax;

    Pid(float kp, float ki, float kd, float outMax, float iMax)
        : kp(kp), ki(ki), kd(kd), outMax(outMax), iMax(iMax) {}

    /** Reset integrator and derivative state. */
    void reset() { _integral = 0; _prevError = 0; _firstRun = true; }

    /**
     * @brief Compute one PID step.
     * @param setpoint  Desired value.
     * @param measured  Current measurement.
     * @param dt        Time delta in seconds.
     * @return Control output.
     */
    float compute(float setpoint, float measured, float dt) {
        if (dt <= 0 || dt > 0.5f) return 0;

        const float error = setpoint - measured;

        // Proportional
        const float p = kp * error;

        // Integral with anti-windup
        _integral += ki * error * dt;
        _integral = constrain(_integral, -iMax, iMax);

        // Derivative (skip on first run to avoid spike)
        float d = 0;
        if (!_firstRun) {
            d = kd * (error - _prevError) / dt;
        }
        _firstRun = false;
        _prevError = error;

        float out = p + _integral + d;
        return constrain(out, -outMax, outMax);
    }

    float integral()  const { return _integral; }
    float prevError() const { return _prevError; }

private:
    float _integral  = 0;
    float _prevError = 0;
    bool  _firstRun  = true;
};
