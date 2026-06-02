#ifndef WAVESHARE_IO_EXPANDER_H
#define WAVESHARE_IO_EXPANDER_H

#include <Arduino.h>

class WaveshareIOExpander
{
public:
  static bool init();
  static bool isInitialized();
  static void setBacklight(bool enabled);
  static void setTouchReset(bool enabled);
  static void setLCDReset(bool enabled);
  static void selectSDCard(bool selected);
  static void setUSBMode();

private:
  static bool initialized;
};

#endif // WAVESHARE_IO_EXPANDER_H