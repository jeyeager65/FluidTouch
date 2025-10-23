# FluidTouch Connection Modes

## Overview

FluidTouch supports two connection modes for communicating with FluidNC:
- **WiFi Mode (WebSocket)**: Wireless connection via network
- **UART Mode (Direct Serial)**: Wired connection via UART0

## Hardware Constraints

### CrowPanel ESP32-S3 Specifications
- **Flash**: 4MB total
- **Available UART**: UART0 only (shared with USB programming)
- **Current firmware size**: ~1.48MB (48% of available 3MB)
- **Free flash**: ~1.6MB (52%)

### Why No OTA Updates?

**Flash Requirements:**
- OTA needs dual app partitions (~3.2-3.6MB total)
- Current available flash: ~3.6MB after system partitions
- Firmware growing with features
- **Conclusion**: Insufficient flash space for reliable OTA implementation

**Alternative:** Physical cable swap for firmware updates (see Update Process below)

## Connection Modes

### WiFi Mode (WebSocket) - Default

**Advantages:**
- ✅ Wireless convenience - no cable clutter
- ✅ Easy development and debugging via Serial Monitor
- ✅ Remote monitoring from anywhere in workshop
- ✅ Multiple clients can connect simultaneously
- ✅ Simple firmware updates via USB

**Disadvantages:**
- ❌ WiFi dropouts or interference possible
- ❌ Higher latency (10-50ms typical)
- ❌ Requires network configuration
- ❌ Security considerations (network access)

**Configuration:**
```
Settings → Connection → Mode: WiFi (WebSocket)
  - WiFi SSID: [network name]
  - WiFi Password: [password]
  - FluidNC IP: [e.g., 192.168.0.100]
  - WebSocket Port: [default: 81]
```

### UART Mode (Direct Serial) - Production

**Advantages:**
- ✅ Maximum reliability - no dropouts
- ✅ Lower latency (~1ms)
- ✅ Physical security (no network access)
- ✅ No network configuration needed
- ✅ Faster data transfer (230400+ baud)
- ✅ Works immediately after cable connection

**Disadvantages:**
- ❌ Physical cable required (UART0 TX/RX to FluidNC)
- ❌ USB Serial Monitor unavailable (UART0 shared)
- ❌ Must disconnect to reprogram firmware

**Configuration:**
```
Settings → Connection → Mode: UART (Direct Serial)
  - Baud Rate: [115200 recommended]
  - Protocol: Serial
```

**Wiring:**
```
CrowPanel ESP32-S3         FluidNC Controller
─────────────────         ──────────────────
TX (UART0)        ───────→ RX
RX (UART0)        ←─────── TX
GND               ───────── GND
```

## Mode Selection Implementation

The system uses **mutually exclusive modes** - only one active at a time:

```cpp
// Startup logic (conceptual)
if (connection_mode == UART_MODE) {
    // UART Mode
    Serial.begin(115200);  // UART0 for FluidNC communication
    // WiFi remains disabled
    // USB Serial unavailable for debugging
} else {
    // WiFi Mode (default)
    Serial.begin(115200);  // UART0 for USB debug/programming
    WiFi.begin(ssid, pass);
    // Connect to FluidNC via WebSocket
}
```

## Firmware Update Process

### WiFi Mode (Easy)
1. Connect USB cable to computer
2. Upload firmware via PlatformIO
3. Done - device continues in WiFi mode

### UART Mode (Requires Cable Swap)
1. **Power off** CrowPanel
2. **Disconnect** FluidNC cable from UART0 TX/RX pins
3. **Connect** USB cable to computer
4. **Upload** new firmware via PlatformIO
5. **Disconnect** USB cable
6. **Reconnect** FluidNC cable to UART0 TX/RX pins
7. **Power on** CrowPanel

**Note:** This is a 5-minute process during occasional firmware updates versus consuming 50%+ of remaining flash space permanently for OTA.

## Recommended Usage

### Development Phase
- **Use WiFi Mode**
- Enables Serial Monitor debugging
- Easy code iterations via USB
- Test FluidNC communication wirelessly

### Production Use
- **Option A: WiFi Mode** - Convenience for daily operation
- **Option B: UART Mode** - Maximum reliability for critical work
- **Switch as needed** based on requirements

### When to Use Each Mode

**Use WiFi Mode when:**
- Setting up and testing the system
- You want mobility and wireless convenience
- Multiple people need to monitor the machine
- Network reliability is good in your shop

**Use UART Mode when:**
- Running critical production jobs
- Your shop has WiFi interference issues
- You need guaranteed low-latency communication
- Physical security is important
- Maximum reliability is required

## Future Considerations

### Not Planned (Flash Constraints)
- ❌ OTA firmware updates - insufficient flash space
- ❌ Simultaneous WiFi + UART - only one UART available
- ❌ Software Serial - no available GPIO pins

### Possible Enhancements
- ✅ Auto-detection of FluidNC on UART at startup
- ✅ Connection status monitoring and auto-reconnect
- ✅ Baud rate auto-detection
- ✅ Connection quality indicators in UI
- ✅ Fallback mode switching on connection failure

## Technical Notes

### UART0 Sharing
UART0 on ESP32-S3 is shared between:
- USB Serial (programming and Serial Monitor)
- External TX/RX pins (for FluidNC connection)

Only one can be active at a time - this is a hardware limitation, not a software choice.

### Flash Partition Layout (Current)
```
Bootloader:     ~30KB
Partition Table: ~3KB
NVS (Settings):  ~20KB
App (Firmware):  ~3MB (1.48MB used, 48%)
SPIFFS/Future:   ~1MB available
```

### Why Not Software Serial?
- CrowPanel has minimal unused GPIO pins
- Software UART unreliable at high baud rates (>57600)
- Adds CPU overhead for real-time CNC communication
- Hardware UART0 is more than sufficient with mode switching

## Connection Mode Settings UI

The Settings → Connection tab includes:

```
┌─────────────────────────────────────┐
│ Connection Mode                      │
│ ● WiFi (WebSocket)                   │
│ ○ UART (Direct Serial)               │
│                                      │
│ ─── WiFi Settings ───                │
│ WiFi SSID:     [______________]      │
│ WiFi Password: [______________]      │
│ FluidNC IP:    [______________]      │
│ WS Port:       [81___________]       │
│                                      │
│ ─── UART Settings ───                │
│ Baud Rate:     [115200 ▼]            │
│ Protocol:      [Serial ▼]            │
│                                      │
│ [Save Settings] [Connect] [Disconnect]│
│                                      │
│ Status: Connected via WiFi           │
└─────────────────────────────────────┘
```

## Summary

The two-mode approach provides:
- ✅ Flexibility for different use cases
- ✅ No hardware limitations (works with single UART)
- ✅ Maximum flash space for features
- ✅ Simple, reliable firmware updates
- ✅ Clear separation between development and production
- ✅ User choice based on their specific needs

This design prioritizes **reliability and practicality** over convenience features that would consume critical flash resources.
