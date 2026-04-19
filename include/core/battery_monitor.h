#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <cstdint>

// Battery charging state
enum BatteryState {
    BATTERY_DISCHARGING,  // Running on battery
    BATTERY_CHARGING,     // USB/DC power present, battery charging
    BATTERY_CHARGED       // USB/DC power present, battery full
};

// Battery monitoring via MAX17048 I2C fuel gauge
// Auto-detects the chip on the I2C bus at init time.
// If not found, battery monitoring is disabled gracefully.
class BatteryMonitor {
public:
    // Initialize battery monitor - detects MAX17048 on I2C bus
    static void init();

    // Read battery state - call periodically (every 1-2 seconds is sufficient)
    static void update();

    // Getters for current battery status
    static uint8_t getPercentage() { return percentage; }
    static float getVoltage() { return voltage; }
    static BatteryState getState() { return state; }
    static bool isEnabled() { return enabled; }

    // Get LVGL symbol string for current battery level
    static const char* getBatterySymbol();

private:
    static bool enabled;
    static uint8_t percentage;         // 0-100%
    static float voltage;              // Current cell voltage
    static BatteryState state;
    static float charge_rate;          // %/hour (positive = charging, negative = discharging)

    // MAX17048 register addresses
    static constexpr uint8_t REG_VCELL   = 0x02;  // Cell voltage
    static constexpr uint8_t REG_SOC     = 0x04;  // State of charge (%)
    static constexpr uint8_t REG_MODE    = 0x06;  // Mode register
    static constexpr uint8_t REG_VERSION = 0x08;  // IC version
    static constexpr uint8_t REG_CONFIG  = 0x0C;  // Configuration
    static constexpr uint8_t REG_CRATE   = 0x16;  // Charge rate (%/hr)

    // I2C helpers
    static uint16_t readRegister(uint8_t reg);
    static bool detectChip();
    static float readCellVoltage();
    static uint8_t readSOC();
    static float readChargeRate();
};

#endif // BATTERY_MONITOR_H
