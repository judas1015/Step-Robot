#include "Imu.h"
#include "Config.h"
#include <Wire.h>

// MPU6050 register addresses
static constexpr uint8_t REG_PWR_MGMT1  = 0x6B;
static constexpr uint8_t REG_CONFIG     = 0x1A;
static constexpr uint8_t REG_GYRO_CFG  = 0x1B;
static constexpr uint8_t REG_ACCEL_CFG = 0x1C;
static constexpr uint8_t REG_ACCEL_OUT = 0x3B;  // 14 bytes: AX AY AZ TEMP GX GY GZ

// ---------------------------------------------------------------------------
void Imu::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

bool Imu::readBurst(uint8_t startReg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(_addr);
    Wire.write(startReg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((int)_addr, (int)len, 1);
    for (uint8_t i = 0; i < len; i++) {
        if (!Wire.available()) return false;
        buf[i] = Wire.read();
    }
    return true;
}

// ---------------------------------------------------------------------------
bool Imu::begin(uint8_t addr) {
    _addr = addr;

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(Config::I2C_CLOCK_HZ);

    // Verify device presence
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) return false;

    writeReg(REG_PWR_MGMT1,  0x80);  // Reset device
    delay(100);
    writeReg(REG_PWR_MGMT1,  0x01);  // Wake up, use PLL with X gyro
    delay(5);
    writeReg(REG_CONFIG,     0x03);  // DLPF 44 Hz  (reduces vibration noise)
    writeReg(REG_GYRO_CFG,  0x00);  // ±250 °/s  → scale 131 LSB/(°/s)
    writeReg(REG_ACCEL_CFG, 0x00);  // ±2 g       → scale 16384 LSB/g
    delay(5);

    _lastUs = micros();
    return true;
}

// ---------------------------------------------------------------------------
void Imu::calibrate(void (*progressCb)(uint32_t count)) {
    ImuMath::Calibration cal;
    cal.reset();

    Serial.println("[IMU] ---------------------------------------------------");
    Serial.println("[IMU] Dat robot THANG DUNG, YEN TINH tren nen phang.");
    Serial.println("[IMU] Dang do gyro bias (500 mau ~2.5 giay)...");
    Serial.println("[IMU] ---------------------------------------------------");
    const unsigned long startMs = millis();

    while (cal.count < 500) {
        // Safety: abort after 60 s
        if (millis() - startMs > 60000) {
            Serial.println("[IMU] WARN: Calibration timeout (60s). Gyro bias = 0.");
            _bias[0] = _bias[1] = _bias[2] = 0;
            break;
        }

        uint8_t buf[14];
        if (!readBurst(REG_ACCEL_OUT, buf, 14)) { delay(5); continue; }

        ImuMath::Raw r;
        r.ax = (int16_t)((buf[0]  << 8) | buf[1]);
        r.ay = (int16_t)((buf[2]  << 8) | buf[3]);
        r.az = (int16_t)((buf[4]  << 8) | buf[5]);
        r.gx = (int16_t)((buf[8]  << 8) | buf[9]);
        r.gy = (int16_t)((buf[10] << 8) | buf[11]);
        r.gz = (int16_t)((buf[12] << 8) | buf[13]);

        // ImuMath::Calibration rejects on movement; restart collection if so
        if (cal.rejected) {
            Serial.println("[IMU] WARN: Phat hien dao dong - giu robot yen hon.");
            cal.reset();
        }

        cal.add(r);

        if (progressCb && (cal.count % 100 == 0) && cal.count > 0)
            progressCb(cal.count);
        delayMicroseconds(5000);  // 200 Hz
    }

    if (cal.count >= 500) {
        // Force accept even if variance > 0.5 deg/s
        _bias[0] = cal.gyro[0].mean;
        _bias[1] = cal.gyro[1].mean;
        _bias[2] = cal.gyro[2].mean;
        _calibrated = true;
        Serial.printf("[IMU] Calibrate OK: bias=[%.3f, %.3f, %.3f] deg/s\n",
                      _bias[0], _bias[1], _bias[2]);
    }

    estimate.reset();
    _lastUs = micros();
}

// ---------------------------------------------------------------------------
bool Imu::update() {
    uint8_t buf[14];
    if (!readBurst(REG_ACCEL_OUT, buf, 14)) return false;

    raw.ax = (int16_t)((buf[0]  << 8) | buf[1]);
    raw.ay = (int16_t)((buf[2]  << 8) | buf[3]);
    raw.az = (int16_t)((buf[4]  << 8) | buf[5]);
    raw.gx = (int16_t)((buf[8]  << 8) | buf[9]);
    raw.gy = (int16_t)((buf[10] << 8) | buf[11]);
    raw.gz = (int16_t)((buf[12] << 8) | buf[13]);

    unsigned long nowUs = micros();
    float dt = (nowUs - _lastUs) * 1e-6f;
    _lastUs = nowUs;

    return estimate.update(raw, _bias, dt, Config::COMP_ALPHA);
}
