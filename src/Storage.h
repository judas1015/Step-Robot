#pragma once
#include <Arduino.h>

namespace Storage {
    // Initializes the NVS.
    void begin();

    // Load parameters from flash. If not found, uses default values.
    void load(float& kp, float& ki, float& kd, float& setpoint, float& skp, float& ski, 
              float defKp, float defKi, float defKd, float defSetpoint, float defSkp, float defSki);

    // Save parameters to flash.
    void save(float kp, float ki, float kd, float setpoint, float skp, float ski);
}
