#pragma once
#include <Arduino.h>

namespace WebUI {
    // Typedefs for callbacks
    typedef void (*SaveCallback)(float kp, float ki, float kd, float setpoint, float skp, float ski);
    typedef void (*UpdateCallback)(float kp, float ki, float kd, float setpoint, float skp, float ski);
    typedef void (*ToggleStopCallback)();
    typedef void (*JoystickCallback)(float x, float y);

    // Initialize Wi-Fi AP, WebServer, and WebSockets.
    // Passes the current PID parameters to initialize the UI state.
    void begin(float kp, float ki, float kd, float setpoint, float skp, float ski);

    // Call this in the main loop to handle network events
    void loop();

    // Send telemetry data to all connected WebSocket clients
    void sendTelemetry(float pitch, float output);

    // Register callbacks from main.cpp
    void onSave(SaveCallback cb);
    void onUpdate(UpdateCallback cb);
    void onToggleStop(ToggleStopCallback cb);
    void onJoystick(JoystickCallback cb);
}
