#include "Storage.h"
#include <Preferences.h>

namespace Storage {

static Preferences prefs;

void begin() {
    prefs.begin("balancer", false); // "balancer" namespace, false = read/write
}

void load(float& kp, float& ki, float& kd, float& setpoint, float& skp, float& ski, 
          float defKp, float defKi, float defKd, float defSetpoint, float defSkp, float defSki) {
    kp = prefs.getFloat("kp", defKp);
    ki = prefs.getFloat("ki", defKi);
    kd = prefs.getFloat("kd", defKd);
    setpoint = prefs.getFloat("sp", defSetpoint);
    skp = prefs.getFloat("skp", defSkp);
    ski = prefs.getFloat("ski", defSki);
}

void save(float kp, float ki, float kd, float setpoint, float skp, float ski) {
    prefs.putFloat("kp", kp);
    prefs.putFloat("ki", ki);
    prefs.putFloat("kd", kd);
    prefs.putFloat("sp", setpoint);
    prefs.putFloat("skp", skp);
    prefs.putFloat("ski", ski);
}

} // namespace Storage
