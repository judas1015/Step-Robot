#pragma once
#include <cmath>
#include <cstdint>

namespace ImuMath {
constexpr float GYRO_SCALE = 131.0f;  // LSB/(deg/s), +/-250 deg/s
constexpr float ACCEL_SCALE = 16384.0f;  // LSB/g, +/-2 g
constexpr float RAD_TO_DEG_F = 57.2957795131f;

struct Raw {
  int16_t ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
};

inline float wrap(float angle) {
  return angle - 360.0f * std::floor((angle + 180.0f) / 360.0f);
}

struct Estimate {
  float accelX = 0, accelY = 0, x = 0, y = 0;
  float rateX = 0, rateY = 0, rateZ = 0, normG = 0;
  bool initialized = false;
  void reset() { initialized = false; }
  bool update(const Raw& raw, const float bias[3], float dt, float alpha) {
    if (!(dt > 0 && dt <= 0.025f) || !(alpha > 0 && alpha < 1)) return false;
    const float ax = raw.ax, ay = raw.ay, az = raw.az;
    normG = std::sqrt(ax*ax + ay*ay + az*az) / ACCEL_SCALE;
    // Plane angles for single-axis tilt tests; not a full 3D attitude solution.
    if (normG < 0.2f || (ay*ay + az*az) < 1 || (ax*ax + az*az) < 1) return false;
    accelX = std::atan2(ay, az) * RAD_TO_DEG_F;
    accelY = std::atan2(-ax, az) * RAD_TO_DEG_F;
    rateX = raw.gx / GYRO_SCALE - bias[0];
    rateY = raw.gy / GYRO_SCALE - bias[1];
    rateZ = raw.gz / GYRO_SCALE - bias[2];
    if (!initialized) {
      x = accelX; y = accelY; initialized = true;
    } else {
      // ALPHA is specified at the nominal 5 ms interval. Adapt to measured dt.
      const float weight = std::pow(alpha, dt / 0.005f);
      const float predictedX = wrap(x + rateX * dt);
      const float predictedY = wrap(y + rateY * dt);
      x = wrap(predictedX + (1 - weight) * wrap(accelX - predictedX));
      y = wrap(predictedY + (1 - weight) * wrap(accelY - predictedY));
    }
    return true;
  }
};

struct Stats {
  uint32_t n = 0;
  float mean = 0, m2 = 0;
  void add(float value) {
    ++n;
    const float delta = value - mean;
    mean += delta / n;
    m2 += delta * (value - mean);
  }
  float deviation() const { return n > 1 ? std::sqrt(m2 / (n - 1)) : 0; }
};

struct Calibration {
  Stats gyro[3];
  float firstAccel[3] = {};
  uint32_t count = 0;
  bool rejected = false;
  void reset() { *this = Calibration{}; }
  bool add(const Raw& raw) {
    if (rejected) return false;
    const float accel[3] = {raw.ax / ACCEL_SCALE, raw.ay / ACCEL_SCALE, raw.az / ACCEL_SCALE};
    const float rate[3] = {raw.gx / GYRO_SCALE, raw.gy / GYRO_SCALE, raw.gz / GYRO_SCALE};
    const float norm = std::sqrt(accel[0]*accel[0]+accel[1]*accel[1]+accel[2]*accel[2]);
    if (norm < 0.8f || norm > 1.2f) rejected = true;
    for (int i = 0; i < 3; ++i) {
      if (!count) firstAccel[i] = accel[i];
      if (std::fabs(accel[i] - firstAccel[i]) > 0.035f || std::fabs(rate[i]) > 10.0f ||
          (count > 20 && std::fabs(rate[i] - gyro[i].mean) > 3.0f)) rejected = true;
    }
    if (rejected) return false;
    for (int i = 0; i < 3; ++i) gyro[i].add(rate[i]);
    ++count;
    return true;
  }
  bool finish(float bias[3]) const {
    if (rejected || count < 2000) return false;
    for (const auto& axis : gyro) if (axis.deviation() > 0.5f) return false;
    for (int i = 0; i < 3; ++i) bias[i] = gyro[i].mean;
    return true;
  }
};
}  // namespace ImuMath
