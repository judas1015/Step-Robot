#pragma once
#include <cstdint>

/**
 * Balancer – hardware-timer-based stepper driver for 2-wheel balance robot.
 *
 * Uses ESP32 hardware timer 1 at 20 kHz to generate variable-frequency
 * STEP pulses via DDS (Direct Digital Synthesis) accumulators.
 *
 *   setSpeeds(+N, +N) = both motors drive FORWARD  (robot body moves forward)
 *   setSpeeds(-N, -N) = both motors drive BACKWARD (robot body moves backward)
 *   setSpeeds(0, 0)   = coast (no steps, motors still enabled)
 *
 * Direction mapping (mirrored wheels):
 *   Left  motor: DIR=HIGH → forward
 *   Right motor: DIR=LOW  → forward
 */
namespace Balancer {

/** Initialize GPIO pins and start hardware timer ISR. */
void begin();

/** Enable or disable both motor drivers (EN pin). */
void enable(bool on);

/** Set drive speed in steps/s (signed) for left and right wheels independently. Values below DEADBAND are zeroed. */
void setSpeeds(int32_t speedLeft, int32_t speedRight);

/** Immediate stop: zero speed and disable motors. */
void stop();

bool    isEnabled();
int32_t currentSpeedL();
int32_t currentSpeedR();

}  // namespace Balancer
