# FluidTouch Configuration Guide

> Detailed configuration options and settings

## Table of Contents

- [WiFi Configuration](#wifi-configuration)
- [Machine Configuration](#machine-configuration)
- [Jog Settings](#jog-settings)
- [Probe Settings](#probe-settings)
- [Preferences Storage](#preferences-storage)

---

## Machine Configuration

### Adding a Machine

**Machine Selection Screen → Edit → Add Machine**

1. **Name:** Descriptive name (e.g., "CNC Router", "Pen Plotter")
   - Up to 32 characters
   - Used for identification in status bar

2. **Connection** Connection Type - currently only Wireless is implemented.

3. **WiFi SSID** Wireless Network SSID.
   -- SSIDs are case sensitive.

4. **Password** WiFi Password

5. **FluidNC URL:** FluidNC network address
   - IP address format: `192.168.1.100`
   - Hostname format: `fluidnc.local` (mDNS)

6. **Port:** WebSocket port number
   - Default: `81`
   - Port 81 is used by WebUI v2
   - Port 82 is used by WebUI v3.  Connected to the WebUI will disconnect the pendant (and vice versa).

### Editing a Machine

**Machine Selection Screen → Edit**

1. Select machine in list
2. Tap **Edit** button
3. Modify any settings
4. Tap **Save**
5. Click **Done** to return to Machine Selection Screen

### Deleting a Machine

1. Select machine in list
2. Tap **Delete** button
3. Confirm deletion
4. Machine removed from storage

### Reordering Machines

Use **Up/Down** buttons to:
- Move frequently used machines to top
- Organize by usage or location
- Group similar machines together

### Machine Storage

- Maximum: 4 machines
- Stored in ESP32 NVS (non-volatile storage)
- Survives power cycles and firmware updates
- Macros are configurable after connection

---

## Jog Settings

**Settings → Jog**

Configure default jogging behavior:

### XY Max Feed Rate

- **Range:** 1-10000 mm/min
- **Default:** 3000 mm/min
- **Usage:**
  - Maximum speed for XY jogging
  - Used by joystick (0-100% scaling)
  - Used by jog buttons with selected step size
  
### Z Max Feed Rate

- **Range:** 1-5000 mm/min
- **Default:** 500 mm/min
- **Usage:**
  - Maximum speed for Z-axis jogging
  - Used by joystick (0-100% scaling)
  - Used by Z jog buttons

---

## Probe Settings

**Settings → Probe**

Default values for touch probe operations:

### Feed Rate

- **Range:** 1-1000 mm/min
- **Default:** 100 mm/min
- **Usage:** Probing speed toward target

### Max Distance

- **Range:** 1-500 mm
- **Default:** 50 mm
- **Usage:** Maximum travel distance while probing

### Retract Distance

- **Range:** 0-50 mm
- **Default:** 2 mm
- **Usage:** Pull-off distance after probe contact

### Probe Thickness

- **Range:** 0-50 mm
- **Default:** 0 mm
- **Usage:** Thickness of probe plate/puck
  
**Usage:**
- Enter actual probe plate thickness
- FluidTouch compensates automatically
- Zero value for direct surface probing

---

## Preferences Storage

FluidTouch uses ESP32 NVS (Non-Volatile Storage) for persistent data:

### Stored Data

**Machine Configurations:**
- Up to 4 machine profiles
- Name, hostname/IP, port
- Selected machine index

**Jog Settings:**
- XY max feed rate
- Z max feed rate

**Probe Settings:**
- Feed rate
- Max distance
- Retract distance
- Probe thickness

### Clearing Preferences

To reset to factory defaults:

1. Flash new firmware with "Erase Flash" option
2. Or send erase command via esptool:
   ```bash
   esptool.py --chip esp32s3 erase_flash
   ```
3. Then re-flash firmware

⚠️ **Warning:** This erases all stored configurations!

---

*For usage instructions, see [Usage Guide](./usage.md)*
