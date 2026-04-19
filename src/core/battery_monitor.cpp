#include "core/battery_monitor.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

// Static member initialization
bool BatteryMonitor::enabled = false;
uint8_t BatteryMonitor::percentage = 0;
float BatteryMonitor::voltage = 0.0f;
BatteryState BatteryMonitor::state = BATTERY_DISCHARGING;
float BatteryMonitor::charge_rate = 0.0f;

void BatteryMonitor::init() {
#ifdef BATTERY_ENABLED
    // Detect MAX17048 on the I2C bus
    if (!detectChip()) {
        enabled = false;
        Serial.println("[Battery] MAX17048 not found on I2C bus - battery monitor disabled");
        return;
    }

    enabled = true;

    // Take initial readings
    voltage = readCellVoltage();
    percentage = readSOC();
    charge_rate = readChargeRate();

    // Determine initial state
    if (charge_rate > BATTERY_CHARGE_RATE_THRESHOLD) {
        state = (percentage >= 99) ? BATTERY_CHARGED : BATTERY_CHARGING;
    } else {
        state = BATTERY_DISCHARGING;
    }

    Serial.printf("[Battery] MAX17048 detected - voltage: %.2fV, SOC: %d%%, rate: %.1f%%/hr\n",
                  voltage, percentage, charge_rate);
#else
    enabled = false;
    Serial.println("[Battery] Battery monitoring disabled in config");
#endif
}

void BatteryMonitor::update() {
#ifdef BATTERY_ENABLED
    if (!enabled) return;

    // Read all values from MAX17048
    voltage = readCellVoltage();
    percentage = readSOC();
    charge_rate = readChargeRate();

    // Determine charging state from charge rate
    // CRATE > threshold = charging, near zero + high SOC = charged, otherwise discharging
    if (charge_rate > BATTERY_CHARGE_RATE_THRESHOLD) {
        if (percentage >= 99) {
            state = BATTERY_CHARGED;
        } else {
            state = BATTERY_CHARGING;
        }
    } else if (percentage >= 99 && charge_rate >= 0.0f) {
        // Battery full and not discharging (could be on USB power)
        state = BATTERY_CHARGED;
    } else {
        state = BATTERY_DISCHARGING;
    }
#endif
}

bool BatteryMonitor::detectChip() {
    // Try to communicate with MAX17048 at its I2C address
    Wire.beginTransmission(MAX17048_ADDR);
    uint8_t error = Wire.endTransmission();

    if (error != 0) {
        return false;
    }

    // Read VERSION register to verify it's actually a MAX17048/MAX17049
    uint16_t version = readRegister(REG_VERSION);
    if (version == 0 || version == 0xFFFF) {
        Serial.printf("[Battery] Invalid MAX17048 version: 0x%04X\n", version);
        return false;
    }

    Serial.printf("[Battery] MAX17048 version: 0x%04X\n", version);
    return true;
}

uint16_t BatteryMonitor::readRegister(uint8_t reg) {
    Wire.beginTransmission(MAX17048_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0;
    }

    if (Wire.requestFrom((uint8_t)MAX17048_ADDR, (uint8_t)2) != 2) {
        return 0;
    }

    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    return ((uint16_t)msb << 8) | lsb;
}

float BatteryMonitor::readCellVoltage() {
    // VCELL register (0x02): 16-bit value, resolution = 78.125uV per unit
    // Voltage = raw * 78.125e-6
    uint16_t raw = readRegister(REG_VCELL);
    return raw * 78.125e-6f;
}

uint8_t BatteryMonitor::readSOC() {
    // SOC register (0x04): MSB = whole percent, LSB = 1/256th percent
    uint16_t raw = readRegister(REG_SOC);
    uint8_t whole = (uint8_t)(raw >> 8);

    // Clamp to 0-100
    if (whole > 100) whole = 100;
    return whole;
}

float BatteryMonitor::readChargeRate() {
    // CRATE register (0x16): signed 16-bit, units of 0.208%/hr
    // Positive = charging, negative = discharging
    uint16_t raw = readRegister(REG_CRATE);
    int16_t signed_raw = (int16_t)raw;
    return signed_raw * 0.208f;
}

const char* BatteryMonitor::getBatterySymbol() {
    if (state == BATTERY_CHARGING) {
        return LV_SYMBOL_CHARGE;
    }
    if (percentage >= 80) return LV_SYMBOL_BATTERY_FULL;
    if (percentage >= 55) return LV_SYMBOL_BATTERY_3;
    if (percentage >= 30) return LV_SYMBOL_BATTERY_2;
    if (percentage >= 10) return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}
