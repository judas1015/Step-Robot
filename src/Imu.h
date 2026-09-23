#pragma once
#include <Arduino.h>
#include "ImuMath.h"

/**
 * Imu – wraps MPU6050 I2C reads and ImuMath complementary filter.
 *
 * Axis convention (default, MPU6050 mounted flat on robot):
 *   pitch = Estimate.y = atan2(-ax, az)   <- forward/backward tilt
 *   roll  = Estimate.x = atan2( ay, az)   <- left/right tilt
 *
 * If the robot tilts on the other axis, swap pitchDeg() to return estimate.x.
 */
class Imu {
public:
    /**
     * @brief Init I2C and configure MPU6050 registers.
     * @return false if device not found on bus.
     */
    bool begin(uint8_t addr = 0x68);

    /**
     * @brief Collect samples for gyro calibration. Robot must be still.
     *        Blocks until 2000 valid samples collected (~10 s at 200 Hz).
     * @param progressCb  Optional callback called every 100 samples.
     */
    void calibrate(void (*progressCb)(uint32_t count) = nullptr);

    /**
     * @brief Read raw sensor and update complementary filter.
     * @return true if filter updated successfully.
     */
    bool update();

    /** Pitch angle (forward/backward tilt) in degrees. */
    float pitchDeg() const { return estimate.y; }

    /** Gyro rate on pitch axis (deg/s). */
    float pitchRateDegS() const { return estimate.rateY; }

    /** Accelerometer-only pitch (for debug). */
    float accelPitchDeg() const { return estimate.accelY; }

    /** True once calibrated. */
    bool calibrated() const { return _calibrated; }

    // Raw calibrated values (public for debug logging)
    ImuMath::Raw raw;
    ImuMath::Estimate estimate;

private:
    uint8_t _addr = 0x68;
    bool    _calibrated = false;
    float   _bias[3] = {0, 0, 0};  // gyro bias [gx, gy, gz] deg/s
    unsigned long _lastUs = 0;

    void writeReg(uint8_t reg, uint8_t val);
    bool readBurst(uint8_t startReg, uint8_t* buf, uint8_t len);
};
