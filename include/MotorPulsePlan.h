#pragma once
#include <cstdint>

// Bounded, single-motor direction test only. Not the future balancing engine.
// Called by a 100 us timer ISR. Methods are forced inline for ISR residency.
struct MotorPulsePlan {
  static constexpr uint32_t PERIOD_US = 10000;  // at most 100 STEP/s
  static constexpr uint32_t HIGH_US = 100;     // DRV8825 minimum is 1.9 us
  static constexpr uint32_t SETTLE_US = 2000;  // DIR/enable settle before STEP
  static constexpr uint32_t MAX_RUN_US = 1000000;
  static constexpr uint32_t SENSOR_TIMEOUT_US = 25000;
  static constexpr uint32_t MAX_PULSES = 100;
  enum class Reason : uint8_t { NONE, COMPLETE, TIME_LIMIT, STOP, SENSOR_TIMEOUT };
  bool active = false, high = false;
  uint32_t started = 0, nextRise = 0, highSince = 0, pulses = 0;
  Reason reason = Reason::NONE;

  __attribute__((always_inline)) inline void start(uint32_t now) {
    active = true; high = false; pulses = 0; started = now;
    nextRise = now + SETTLE_US; reason = Reason::NONE;
  }
  __attribute__((always_inline)) inline void stop(Reason why) {
    active = false; high = false; reason = why;
  }
  __attribute__((always_inline)) inline void tick(uint32_t now, uint32_t heartbeat) {
    if (!active) return;
    if (now - heartbeat > SENSOR_TIMEOUT_US) { stop(Reason::SENSOR_TIMEOUT); return; }
    if (now - started >= MAX_RUN_US) { stop(Reason::TIME_LIMIT); return; }
    if (high) {
      if (now - highSince >= HIGH_US) {
        high = false;
        if (pulses >= MAX_PULSES) stop(Reason::COMPLETE);
      }
      return;
    }
    if (static_cast<int32_t>(now - nextRise) >= 0) {
      high = true; highSince = now; ++pulses;
      // Never catch up missed pulses in a burst after a delayed interrupt.
      nextRise = now + PERIOD_US;
    }
  }
};
