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

## Power Settings

**Settings → Power**

Configure power management and display brightness:

### Power Management

- **Enable/Disable:** Toggle power saving features
- **Dim Timeout:** Time until screen dims (Disabled, 15s, 30s, 45s, 60s, 90s, 120s, 300s)
- **Sleep Timeout:** Time until screen turns off (Disabled, 60s, 120s, 300s, 600s, 900s, 1800s, 3600s)
- **Deep Sleep:** Time until deep sleep mode (Disabled, 5min, 10min, 15min, 30min, 60min, 90min)

### Brightness Control

- **Normal Brightness:** Active display brightness (25%, 50%, 75%, 100%)
- **Dim Brightness:** Dimmed display brightness (5%, 10%, 25%, 50%)

**Notes:**
- Power saving only applies in IDLE and DISCONNECTED states
- During RUN, ALARM, HOLD, or JOG states, display stays at full brightness
- Touch activity resets timers and restores full brightness
- Deep sleep mode requires reset button to wake

---

## Backup & Restore

### Exporting Settings

**Settings → Backup → Export Settings**

Creates a backup file on the Display SD card:

**Export File:**
- Location: `/fluidtouch_settings.json` (root of Display SD card)
- Format: JSON (human-readable, editable)
- Version: 1.0

**Exported Data:**
- Machine configurations (name, connection type, hostname/IP, port, SSID)
- Jog settings (XY feed rate, Z feed rate)
- Probe settings (feed rate, max distance, retract distance, thickness)
- Macros (up to 9 per machine)
- Power management settings (enabled, timeouts, brightness levels, deep sleep)
- UI preferences (folders on top)

**Security:**
- WiFi passwords are NOT included in the export
- Empty password fields are exported instead
- You must manually re-enter WiFi passwords after importing

### Importing Settings

**Automatic Import:**
1. Copy `fluidtouch_settings.json` to Display SD card root
2. Clear all settings (or start with fresh install)
3. Restart device
4. If no machines are configured, settings are automatically imported
5. Device restarts again after successful import

**Manual Import:**
1. Go to **Settings → General**
2. Tap **Clear All Settings**
3. Confirm warning dialog
4. Device restarts and auto-imports if settings file exists
5. Configure WiFi passwords for each machine

### Clearing All Settings

**Settings → General → Clear All Settings**

Permanently deletes all stored data:
- All machine configurations
- All macros
- Power management settings
- UI preferences
- System settings

**Process:**
1. Confirmation dialog warns of permanent deletion
2. Lists all data that will be removed
3. Suggests exporting settings first
4. Two buttons: "Clear & Restart" (left), "Cancel" (right)
5. Clearing triggers immediate device restart
6. Returns to initial setup state

⚠️ **Warning:** This action cannot be undone! Consider exporting settings before clearing.

---

## Preferences Storage

FluidTouch uses ESP32 NVS (Non-Volatile Storage) for persistent data:

### Stored Data

**Machine Configurations:**
- Up to 4 machine profiles
- Name, hostname/IP, port, WiFi SSID/password
- Selected machine index

**Jog Settings:**
- XY max feed rate
- Z max feed rate

**Probe Settings:**
- Feed rate
- Max distance
- Retract distance
- Probe thickness

**Power Settings:**
- Power management enabled/disabled
- Dim timeout, sleep timeout, deep sleep timeout
- Normal brightness (0-100%)
- Dim brightness (0-100%)

**Macros:**
- Up to 9 macros per machine
- Macro names and G-code content

**File Browser:**
- Folders on top setting

### Backup Options

**Export Settings (Recommended):**
- Creates JSON backup file on Display SD card
- See [Backup & Restore](#backup--restore) section above

### Clearing Preferences

**Option 1 - Clear All Settings (Settings Tab):**
1. Go to **Settings → General**
2. Tap **Clear All Settings**
3. Confirm deletion
4. Device restarts with factory defaults

**Option 2 - Flash Erase:**
1. Flash new firmware with "Erase Flash" option
2. Or send erase command via esptool:
   ```bash
   esptool.py --chip esp32s3 erase_flash
   ```
3. Then re-flash firmware

⚠️ **Warning:** This erases all stored configurations!

---

*For usage instructions, see [Usage Guide](./usage.md)*
