#include "core/waveshare_io_expander.h"

#ifdef HARDWARE_WAVESHARE_7

#include "config.h"
#include <Wire.h>

namespace
{
  // CH422G uses command-address protocol:
  // - I2C address byte encodes the register/command
  // - Payload contains only the data byte (no register byte in payload)
  constexpr uint8_t CH422G_REG_MODE = 0x24;
  constexpr uint8_t CH422G_MODE_OUTPUT = 0x01; // Enable output mode for pins 0..7
  constexpr uint8_t CH422G_REG_OUT = 0x38;     // Output data register for pins 0..7
  constexpr uint8_t CH422G_WRITE_RETRIES = 3;

  uint8_t output_state = 0xFF;

  bool writeCommand(uint8_t command_addr, uint8_t value)
  {
    for (uint8_t attempt = 1; attempt <= CH422G_WRITE_RETRIES; ++attempt)
    {
      Wire.beginTransmission(command_addr);
      Wire.write(value);
      uint8_t result = Wire.endTransmission();
      if (result == 0)
      {
        return true;
      }

      Serial.printf("[WaveshareIOExpander] I2C command write failed: cmd=0x%02X value=0x%02X result=%u (attempt %u/%u)\n",
                    command_addr, value, result, attempt, CH422G_WRITE_RETRIES);

      // Re-prime shared I2C bus between retries in case another device left it busy.
      Wire.begin(TOUCH_SDA, TOUCH_SCL);
      Wire.setClock(100000);
      Wire.setTimeOut(100);
      delay(2);
    }

    return false;
  }

  bool writeOutputState()
  {
    return writeCommand(CH422G_REG_OUT, output_state);
  }

  bool setOutputPin(uint8_t pin, bool high)
  {
    if (pin > 7)
    {
      Serial.printf("[WaveshareIOExpander] Invalid CH422G pin: %u\n", pin);
      return false;
    }

    if (high)
    {
      output_state |= (1U << pin);
    }
    else
    {
      output_state &= ~(1U << pin);
    }

    return writeOutputState();
  }

  bool ensureExpanderReady()
  {
    if (WaveshareIOExpander::isInitialized())
    {
      return true;
    }

    return WaveshareIOExpander::init();
  }
}

bool WaveshareIOExpander::initialized = false;

bool WaveshareIOExpander::init()
{
  if (initialized)
  {
    return true;
  }

  Serial.println("[WaveshareIOExpander] Initializing CH422G...");

  // Waveshare's CH422G helper uses a simple two-byte protocol at I2C address
  // 0x24: register 0x02 sets output mode, and register 0x03 writes IO0..IO7.
  // Initialize the shared touch/expander I2C bus before LovyanGFX claims it.
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(100);

  Serial.printf("[WaveshareIOExpander] CH422G command mode: MODE=0x%02X OUT=0x%02X\n", CH422G_REG_MODE, CH422G_REG_OUT);

  // Prepare safe initial output levels before enabling outputs.
  output_state = 0xFF;
  output_state = (output_state & ~(1U << CH422G_USB_SEL_PIN)) | (CH422G_USB_MODE ? (1U << CH422G_USB_SEL_PIN) : 0U);
  output_state = (output_state & ~(1U << CH422G_SD_CS_PIN)) | (CH422G_SD_DESELECTED ? (1U << CH422G_SD_CS_PIN) : 0U);
  // Keep panel visible at boot: initialize backlight to ON immediately.
  // This avoids a black screen if later non-critical writes momentarily fail.
  output_state = (output_state & ~(1U << CH422G_LCD_BL_PIN)) | (CH422G_BACKLIGHT_ON ? (1U << CH422G_LCD_BL_PIN) : 0U);
  output_state |= (1U << CH422G_LCD_RST_PIN);
  output_state |= (1U << CH422G_TOUCH_RST_PIN);

  // Configure output mode first, then drive output levels.
  if (!writeCommand(CH422G_REG_MODE, CH422G_MODE_OUTPUT) || !writeOutputState())
  {
    Serial.println("[WaveshareIOExpander] CH422G init failed");
    return false;
  }

  // Perform proper reset sequence for both the LCD controller and GT911 touch controller.
  // After enabling CH422G outputs the output state may be stale, so drive RST LOW
  // explicitly to guarantee a valid reset pulse — particularly important after ESP.restart()
  // where the GT911 remains powered but the ESP32 I2C bus is re-initialised.
  //
  // GT911 datasheet: RST must be held LOW for ≥5ms, then HIGH, then ≥50ms before I2C.
  setOutputPin(CH422G_LCD_RST_PIN, LOW);    // Assert LCD reset (active LOW)
  setOutputPin(CH422G_TOUCH_RST_PIN, LOW);  // Assert GT911 reset (active LOW)
  delay(10);                                // Hold both resets low (>5ms per GT911 spec)
  setOutputPin(CH422G_LCD_RST_PIN, HIGH);   // Release LCD reset
  delay(10);                                // Brief gap before releasing touch
  setOutputPin(CH422G_TOUCH_RST_PIN, HIGH); // Release GT911 reset
  delay(50);                                // GT911 boot time (≥50ms before I2C use)

  initialized = true;

  Serial.println("[WaveshareIOExpander] CH422G ready");
  return true;
}

bool WaveshareIOExpander::isInitialized()
{
  return initialized;
}

void WaveshareIOExpander::setBacklight(bool enabled)
{
  if (!ensureExpanderReady())
  {
    return;
  }

  setOutputPin(CH422G_LCD_BL_PIN, enabled ? CH422G_BACKLIGHT_ON : CH422G_BACKLIGHT_OFF);
}

void WaveshareIOExpander::setTouchReset(bool enabled)
{
  if (!ensureExpanderReady())
  {
    return;
  }

  setOutputPin(CH422G_TOUCH_RST_PIN, enabled ? HIGH : LOW);
}

void WaveshareIOExpander::setLCDReset(bool enabled)
{
  if (!ensureExpanderReady())
  {
    return;
  }

  setOutputPin(CH422G_LCD_RST_PIN, enabled ? HIGH : LOW);
}

void WaveshareIOExpander::selectSDCard(bool selected)
{
  if (!ensureExpanderReady())
  {
    return;
  }

  setOutputPin(CH422G_SD_CS_PIN, selected ? CH422G_SD_SELECTED : CH422G_SD_DESELECTED);
}

void WaveshareIOExpander::setUSBMode()
{
  if (!ensureExpanderReady())
  {
    return;
  }

  setOutputPin(CH422G_USB_SEL_PIN, CH422G_USB_MODE);
}

#else

bool WaveshareIOExpander::initialized = false;

bool WaveshareIOExpander::init() { return false; }
bool WaveshareIOExpander::isInitialized() { return false; }
void WaveshareIOExpander::setBacklight(bool enabled) { (void)enabled; }
void WaveshareIOExpander::setTouchReset(bool enabled) { (void)enabled; }
void WaveshareIOExpander::setLCDReset(bool enabled) { (void)enabled; }
void WaveshareIOExpander::selectSDCard(bool selected) { (void)selected; }
void WaveshareIOExpander::setUSBMode() {}

#endif