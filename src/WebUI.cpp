#include "WebUI.h"
#include "Config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h> // Need to add to platformio.ini

namespace WebUI {

static WebServer server(Config::WEBSERVER_PORT);
static WebSocketsServer webSocket(Config::WEBSOCKET_PORT);

static SaveCallback cbSave = nullptr;
static UpdateCallback cbUpdate = nullptr;
static ToggleStopCallback cbToggleStop = nullptr;
static JoystickCallback cbJoystick = nullptr;

// We will send the initial values when a client connects
static float currentKp = 0;
static float currentKi = 0;
static float currentKd = 0;
static float currentSetpoint = 0;
static float currentSkp = 0;
static float currentSki = 0;

const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Xe Cân Bằng PID Tuner</title>
    <style>
        :root { --bg: #121212; --panel: #1e1e1e; --text: #e0e0e0; --accent: #bb86fc; --danger: #cf6679; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        h1 { margin-top: 0; font-size: 1.5rem; text-align: center; }
        .container { width: 100%; max-width: 600px; }
        .panel { background-color: var(--panel); border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 15px; }
        .label { flex: 1; font-weight: bold; }
        .slider { flex: 3; margin: 0 15px; }
        .num-input { flex: 1; max-width: 80px; background: #333; color: white; border: 1px solid #555; border-radius: 4px; padding: 5px; text-align: right; }
        .num-input:focus { outline: none; border-color: var(--accent); }
        button { background-color: var(--accent); color: #000; border: none; padding: 10px 20px; border-radius: 4px; font-weight: bold; cursor: pointer; flex: 1; margin: 0 5px; }
        button:active { opacity: 0.8; }
        .btn-stop { background-color: var(--danger); color: white; }
        .btn-group { display: flex; justify-content: space-between; margin-top: 10px; }
        .status { text-align: center; margin-top: -10px; margin-bottom: 15px; font-size: 0.9rem; color: #888; }
        .dpad { display: flex; flex-direction: column; align-items: center; margin: 20px auto; gap: 25px; }
        .dpad-row { display: flex; gap: 70px; }
        .dpad-btn { width: 60px; height: 60px; background: #333; border: 2px solid var(--accent); color: var(--accent); font-size: 24px; border-radius: 10px; cursor: pointer; user-select: none; touch-action: none; margin: 0; }
        .dpad-btn:active { background: var(--accent); color: #000; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Dashboard Cân Bằng</h1>
        <div class="status" id="ws-status">Connecting...</div>

        <div class="panel">
            <div class="row">
                <span class="label">Kp</span>
                <input type="range" class="slider" id="kp-slider" min="0" max="200" step="0.5">
                <input type="number" class="num-input" id="kp-input" step="0.5">
            </div>
            <div class="row">
                <span class="label">Ki</span>
                <input type="range" class="slider" id="ki-slider" min="0" max="10" step="0.1">
                <input type="number" class="num-input" id="ki-input" step="0.1">
            </div>
            <div class="row">
                <span class="label">Kd</span>
                <input type="range" class="slider" id="kd-slider" min="0" max="10" step="0.1">
                <input type="number" class="num-input" id="kd-input" step="0.1">
            </div>
            <div class="row">
                <span class="label">Góc (Sp)</span>
                <input type="range" class="slider" id="sp-slider" min="-10" max="10" step="0.1">
                <input type="number" class="num-input" id="sp-input" step="0.1">
            </div>
            <div class="row">
                <span class="label">Speed Kp</span>
                <input type="range" class="slider" id="skp-slider" min="0" max="0.1" step="0.001">
                <input type="number" class="num-input" id="skp-input" step="0.001">
            </div>
            <div class="row">
                <span class="label">Speed Ki</span>
                <input type="range" class="slider" id="ski-slider" min="0" max="0.005" step="0.0001">
                <input type="number" class="num-input" id="ski-input" step="0.0001">
            </div>
            <div class="btn-group">
                <button id="btn-save">LƯU VÀO FLASH</button>
                <button id="btn-stop" class="btn-stop">DỪNG/CHẠY</button>
            </div>
        </div>

        <div class="panel">
            <div style="display:flex; justify-content:space-between">
                <span>Góc: <b id="val-pitch">0.0</b>°</span>
                <span>Speed: <b id="val-spd">0</b></span>
            </div>
        </div>

        <div class="panel" style="text-align: center;">
            <h3 style="margin-top: 0;">Điều Khiển (D-Pad)</h3>
            <div class="dpad">
                <button id="btn-up" class="dpad-btn">▲</button>
                <div class="dpad-row">
                    <button id="btn-left" class="dpad-btn">◀</button>
                    <button id="btn-right" class="dpad-btn">▶</button>
                </div>
                <button id="btn-down" class="dpad-btn">▼</button>
            </div>
            <div class="status">Nhấn giữ để lái (Lên/Xuống: Chạy - Trái/Phải: Rẽ)</div>
        </div>
    </div>

    <script>
        const ws = new WebSocket(`ws://${window.location.hostname}:81/`);
        const statusEl = document.getElementById('ws-status');
        
        const kpS = document.getElementById('kp-slider'), kpI = document.getElementById('kp-input');
        const kiS = document.getElementById('ki-slider'), kiI = document.getElementById('ki-input');
        const kdS = document.getElementById('kd-slider'), kdI = document.getElementById('kd-input');
        const spS = document.getElementById('sp-slider'), spI = document.getElementById('sp-input');
        const skpS = document.getElementById('skp-slider'), skpI = document.getElementById('skp-input');
        const skiS = document.getElementById('ski-slider'), skiI = document.getElementById('ski-input');
        
        let preventSend = false;

        function sync(slider, input, isSlider) {
            if (isSlider) input.value = slider.value;
            else slider.value = input.value;
            sendUpdate();
        }

        [ [kpS, kpI], [kiS, kiI], [kdS, kdI], [spS, spI], [skpS, skpI], [skiS, skiI] ].forEach(([s, i]) => {
            s.addEventListener('input', () => sync(s, i, true));
            i.addEventListener('change', () => sync(s, i, false));
        });

        function sendUpdate() {
            if (preventSend || ws.readyState !== WebSocket.OPEN) return;
            const data = { 
                type: 'update', 
                kp: parseFloat(kpI.value), ki: parseFloat(kiI.value), kd: parseFloat(kdI.value), sp: parseFloat(spI.value),
                skp: parseFloat(skpI.value), ski: parseFloat(skiI.value)
            };
            ws.send(JSON.stringify(data));
        }

        document.getElementById('btn-save').onclick = () => {
            if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({type: 'save'}));
            alert('Đã lưu vào bộ nhớ Flash!');
        };
        document.getElementById('btn-stop').onclick = () => {
            if (ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify({type: 'stop'}));
        };

        ws.onopen = () => { statusEl.innerText = "Connected"; statusEl.style.color = "#4caf50"; };
        ws.onclose = () => { statusEl.innerText = "Disconnected"; statusEl.style.color = "#cf6679"; };


        ws.onmessage = (e) => {
            const msg = JSON.parse(e.data);
            if (msg.type === 'init') {
                preventSend = true;
                kpS.value = kpI.value = msg.kp;
                kiS.value = kiI.value = msg.ki;
                kdS.value = kdI.value = msg.kd;
                spS.value = spI.value = msg.sp;
                skpS.value = skpI.value = msg.skp;
                skiS.value = skiI.value = msg.ski;
                preventSend = false;
            } else if (msg.type === 'telemetry') {
                document.getElementById('val-pitch').innerText = msg.pitch.toFixed(1);
                document.getElementById('val-spd').innerText = msg.speed;
            }
        };

        // D-Pad logic
        function sendJoy(x, y) {
            if (ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({type: 'joy', x: x, y: y}));
            }
        }
        
        function bindDPad(id, x, y) {
            const btn = document.getElementById(id);
            const press = (e) => { e.preventDefault(); sendJoy(x, y); };
            const release = (e) => { e.preventDefault(); sendJoy(0, 0); };
            
            btn.addEventListener('mousedown', press);
            btn.addEventListener('touchstart', press, {passive: false});
            
            btn.addEventListener('mouseup', release);
            btn.addEventListener('mouseleave', release);
            btn.addEventListener('touchend', release);
        }
        
        bindDPad('btn-up', 0, 0.2);
        bindDPad('btn-down', 0, -0.2);
        bindDPad('btn-left', -0.2, 0);
        bindDPad('btn-right', 0.2, 0);
    </script>
</body>
</html>
)rawliteral";

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_CONNECTED) {
        // Send initial PID values to the client
        StaticJsonDocument<200> doc;
        doc["type"] = "init";
        doc["kp"] = currentKp;
        doc["ki"] = currentKi;
        doc["kd"] = currentKd;
        doc["sp"] = currentSetpoint;
        doc["skp"] = currentSkp;
        doc["ski"] = currentSki;
        String json;
        serializeJson(doc, json);
        webSocket.sendTXT(num, json);
    } else if (type == WStype_TEXT) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) return;

        const char* msgType = doc["type"];
        if (strcmp(msgType, "update") == 0) {
            currentKp = doc["kp"];
            currentKi = doc["ki"];
            currentKd = doc["kd"];
            currentSetpoint = doc["sp"];
            currentSkp = doc["skp"];
            currentSki = doc["ski"];
            if (cbUpdate) cbUpdate(currentKp, currentKi, currentKd, currentSetpoint, currentSkp, currentSki);
        } else if (strcmp(msgType, "save") == 0) {
            if (cbSave) cbSave(currentKp, currentKi, currentKd, currentSetpoint, currentSkp, currentSki);
        } else if (strcmp(msgType, "stop") == 0) {
            if (cbToggleStop) cbToggleStop();
        } else if (strcmp(msgType, "joy") == 0) {
            if (cbJoystick) cbJoystick(doc["x"], doc["y"]);
        }
    }
}

void begin(float kp, float ki, float kd, float setpoint, float skp, float ski) {
    currentKp = kp;
    currentKi = ki;
    currentKd = kd;
    currentSetpoint = setpoint;
    currentSkp = skp;
    currentSki = ski;

    // Set up AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(Config::WIFI_AP_SSID, Config::WIFI_AP_PASS);
    
    Serial.print("\n[WebUI] AP Started. SSID: ");
    Serial.println(Config::WIFI_AP_SSID);
    Serial.print("[WebUI] IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Setup WebServer
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", INDEX_HTML);
    });
    server.begin();

    // Setup WebSocket
    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
}

void loop() {
    server.handleClient();
    webSocket.loop();
}

void sendTelemetry(float pitch, float output) {
    StaticJsonDocument<100> doc;
    doc["type"] = "telemetry";
    doc["pitch"] = pitch;
    doc["speed"] = (int)output;
    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

void onSave(SaveCallback cb) { cbSave = cb; }
void onUpdate(UpdateCallback cb) { cbUpdate = cb; }
void onToggleStop(ToggleStopCallback cb) { cbToggleStop = cb; }
void onJoystick(JoystickCallback cb) { cbJoystick = cb; }

} // namespace WebUI
