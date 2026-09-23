#pragma once
#include <Arduino.h>
#include "MotorPulsePlan.h"

namespace MotorTest {
struct Status {
  bool timerReady, active, armed;
  int side;  // 0=none, 1=left, 2=right
  uint32_t pulses, completions, isrTicks;
  MotorPulsePlan::Reason reason;
};
bool begin();
void heartbeat();  // Only after a valid, fresh sensor/filter update.
bool arm();        // One test only; expires after 30 seconds, never saved.
bool start(int side, int directionSign);
void stop();       // Both EN HIGH, both STEP LOW; clears arm.
Status status();
const char* reasonName(MotorPulsePlan::Reason reason);
}
