#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Global serial-debug mute
//
// On the Elecrow CrowPanel Advance, "USB CDC" mode talks to FluidNC over UART0
// (GPIO43 TX / GPIO44 RX) through the on-board CH340 USB-UART bridge - the SAME
// physical UART that the Arduino `Serial` debug console uses. While a USB CDC
// link to FluidNC is active, ANY stray Serial.print() would be delivered to
// FluidNC as garbage G-code/commands, corrupting the link.
//
// `g_serialMuted` is the single source of truth: FluidNCClient sets it true when
// a USB CDC connection becomes active and false again on disconnect. All runtime
// logging that can fire during an active session should go through the LOG_*
// macros below so it is automatically suppressed while muted.
//
// Boot-time / pre-connection logging (setup(), machine selection, settings) runs
// before any USB CDC link exists, so it prints normally regardless of this flag.
// ---------------------------------------------------------------------------
extern bool g_serialMuted;

#define LOG_PRINT(...)    do { if (!g_serialMuted) Serial.print(__VA_ARGS__);   } while (0)
#define LOG_PRINTLN(...)  do { if (!g_serialMuted) Serial.println(__VA_ARGS__); } while (0)
#define LOG_PRINTF(...)   do { if (!g_serialMuted) Serial.printf(__VA_ARGS__);  } while (0)

#endif // DEBUG_LOG_H
