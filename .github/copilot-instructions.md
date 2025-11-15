# FluidTouch - AI Coding Agent Instructions

## Project Overview
FluidTouch is an ESP32-S3 embedded touchscreen CNC controller for FluidNC machines, running on Elecrow CrowPanel 7" displays (800x480). The project uses PlatformIO, LVGL 9.4 for UI, and LovyanGFX for hardware-accelerated display rendering.

**Supported Hardware**: Elecrow CrowPanel ESP32 7" HMI Display (Basic and Advance variants)

### Basic Version
- Product: https://www.elecrow.com/esp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html
- Display: 800x480 TN RGB TFT LCD with GT911 capacitive touch
- MCU: ESP32-S3-WROOM-1-N4R8 (4MB Flash + 8MB Octal PSRAM)
- Backlight: PWM on GPIO2
- Touch I2C: SDA=19, SCL=20, RST=38

### Advance Version  
- Product: https://www.elecrow.com/crowpanel-advance-7-0-hmi-esp32-ai-display-800x480-artificial-intelligent-ips-touch-screen-support-meshtastic-and-arduino-lvgl-micropython.html
- Display: 800x480 IPS RGB LCD with GT911 capacitive touch
- MCU: ESP32-S3-WROOM-1-N16R8 (16MB Flash + 8MB Octal PSRAM)
- Backlight: I2C controlled via STC8H1K28 microcontroller (address 0x30)
- Touch I2C: SDA=15, SCL=16 (RST handled by STC8H1K28 via I2C)
- **Key Differences**: Completely different RGB pin mapping, different sync pins, I2C backlight control

**Build Environments**: 
- `platformio run -e elecrow-crowpanel-7-basic` (4MB flash, PWM backlight)
- `platformio run -e elecrow-crowpanel-7-advance` (16MB flash, I2C backlight)

**Dual Hardware Support**: COMPLETE
- Conditional compilation via `#ifdef HARDWARE_ADVANCE` handles hardware-specific code
- Separate firmware builds for each hardware variant
- GitHub Actions builds both variants automatically
- Web installer allows users to select their hardware variant (manifest_basic.json / manifest_advance.json)
- Touch, display, and backlight fully working on both hardware variants

## Architecture & Design Patterns

### Hardware Platform
- **Flash**: 4MB (Basic) or 16MB (Advance), DIO/QIO mode
- **PSRAM**: 8MB Octal PSRAM (`dio_opi` or `qio_opi` memory type)
- **Display**: 800x480 RGB parallel interface (16-bit RGB565)
- **Touch**: GT911 I2C controller (address 0x5D)
- **Critical**: ALL large buffers MUST use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` to allocate in PSRAM, never regular heap

### Dual Hardware Support Pattern
The project uses conditional compilation (`#ifdef HARDWARE_ADVANCE`) to support both hardware variants:

1. **Display Driver** (`src/core/display_driver.cpp`):
   - Basic: GPIO pins 15,7,6,5,4 (B), 9,46,3,8,16,1 (G), 14,21,47,48,45 (R), sync pins 41,40,39,0
   - Advance: GPIO pins 21,47,48,45,38 (B), 9,10,11,12,13,14 (G), 7,17,18,3,46 (R), sync pins 42,41,40,39
   - Timing: Basic 10MHz / Advance 14MHz (reduced from 18MHz for stability)
   
2. **Backlight Control**:
   - Basic: Direct PWM on GPIO2 via `ledcWrite()`
   - Advance: I2C commands to STC8H1K28 (wake 0x19, config 0x10/0x18, brightness 0x00-0xF5)

3. **Touch Controller**:
   - Both use GT911 at 0x5D, but different I2C pins
   - Basic: SDA=19, SCL=20, RST=38
   - Advance: SDA=15, SCL=16, RST=-1 (handled by STC8H1K28 via I2C)
   - **Touch panel configured in LGFX class** (display_driver.cpp) with `lgfx::Touch_GT911 _touch_instance`
   - LovyanGFX provides GT911 initialization and touch reading via `lcd->getTouch()`
   - TouchDriver delegates to LovyanGFX using `lcd_instance->getTouch(&x, &y)` in callback

### Module Organization Pattern
The codebase follows a **strict modular pattern** with clear separation:

1. **Driver Modules** (`core/` subdirectory):
   - `DisplayDriver` - LovyanGFX RGB parallel display with LVGL integration and GT911 touch panel configuration (`core/display_driver.h/cpp`)
   - `TouchDriver` - LVGL input device that delegates touch reading to LovyanGFX (`core/touch_driver.h/cpp`)
   - `PowerManager` - Power management for battery-powered operation with three power states (`core/power_manager.h/cpp`)
     - **States**: FULL_BRIGHTNESS → DIMMED → SCREEN_OFF → DEEP_SLEEP (optional)
     - **Brightness Storage**: NVS stores percentages (0-100), DisplayDriver converts to hardware values (0-255)
     - **NVS Keys** (shortened to fit 15-char limit): `pm_enabled`, `pm_dim_to`, `pm_sleep_to`, `pm_deepsleep`, `pm_norm_bri`, `pm_dim_bri`
     - **State-aware**: Only applies power saving in IDLE and DISCONNECTED states - all other states (RUN, ALARM, HOLD, JOG) stay at full brightness
     - **User activity**: Touch events call `onUserActivity()` to reset timer and restore full brightness
     - **Deep sleep**: Enters ESP32 deep sleep after timeout (0 = disabled), only reset button wakes
     - **Display control**: Uses `DisplayDriver::setBacklight(percentage)` for dimming, `DisplayDriver::powerDown()` for sleep
     - **Brightness initialization**: Applied immediately on init after loading settings from NVS

2. **Network Modules** (`network/` subdirectory):
   - `ScreenshotServer` - WiFi web server for remote screenshots via LovyanGFX `readRect()` (`network/screenshot_server.h/cpp`)
   - `FluidNCClient` - WebSocket client for FluidNC communication with automatic status reporting (`network/fluidnc_client.h/cpp`)

3. **UI Module Hierarchy** (all under `ui/` subdirectory):
   - **Assets**:
     - `ui/fonts/` - Font assets (jetbrains_mono_16 for terminal)
     - `ui/images/` - Image assets (fluidnc_logo for splash screen)
   - **Core UI**:
     - `UICommon` - Shared status bar with machine/WiFi info and position displays
     - `UIMachineSelect` - Machine selection screen (appears after splash, before main UI)
     - `UISplash` - Startup splash screen (2.5s duration)
     - `UITabs` - Main tabview orchestrator, delegates to tab modules
   - **Tab Modules**:
     - `UITab*` - Individual tab modules (`UITabStatus`, `UITabControl`, `UITabTerminal`, etc.)
     - **Nested tabs**: `UITabControl` contains sub-modules in `tabs/control/` (Actions, Jog, Joystick, Probe, Overrides)
     - **Control sub-tabs**:
       - Actions: Machine control (Home, Zero, Unlock, Reset)
       - Jog: Button-based jogging with XY/Z sections, step selection, and feed rate controls
       - Joystick: Analog-style jogging with circular XY pad and vertical Z slider with quadratic response curve
       - Probe: Touch probe operations with axis-colored buttons, parameter inputs (feed rate, max distance, retract, thickness), and 2-line result display (16pt font)
       - Overrides: Feed/Rapid/Spindle override controls
     - **Terminal tab**: `UITabTerminal` - Raw WebSocket message display (currently disabled via commented callback)
     - **Files tab**: `UITabFiles` - File browser with three storage sources (FluidNC SD, FluidNC Flash, Display SD)
       - Three-source architecture with independent caching per source
       - Display SD reads from ESP32's local SD card slot via SPI
       - Upload functionality: Transfer files from Display SD to FluidNC via HTTP POST
       - Storage switching via dropdown, each source maintains its own navigation state
       - File operations: Play (run G-code), Delete, Upload (Display SD only)
       - Folder navigation with parent/child directory support

4. **Storage Modules** (`ui/` subdirectory):
   - `UploadManager` - Manages file uploads from Display SD card to FluidNC (`ui/upload_manager.h/cpp`)
     - SD card initialization with SPI configuration (hardware-specific pins)
     - File reading from Display SD with chunked transfer (8KB chunks)
     - HTTP POST to FluidNC `/upload` endpoint with multipart/form-data
     - Progress callback system for UI updates
     - 10MB file size limit for uploads
     - **SD Card Detection**: Uses `SD.cardType()` to check if card is present before operations
     - **Error Handling**: Shows appropriate error messages when card is not available

5. **Module Naming Convention**:
   - Class files: `ui_tab_control.h/cpp` → class `UITabControl`
   - All UI classes use static `create(lv_obj_t *parent)` factory methods
   - Headers in `include/`, implementations in `src/` (matching structure)

### LVGL 9.4 Specifics
- **Color depth**: RGB565 (16-bit) via `LV_COLOR_DEPTH 16`
- **Memory**: 256KB LVGL heap allocated in PSRAM (see `lv_conf.h`)
- **Display buffers**: Dual full-screen buffers (800×480 lines each) in PSRAM for smooth rendering
- **Tick handling**: Manual `lv_tick_inc()` + `lv_timer_handler()` in main loop every 5ms
- **No scrolling**: Most tabs disable `LV_OBJ_FLAG_SCROLLABLE` for fixed layouts
- **Font**: Montserrat 18pt for tab buttons and primary UI text
- **Event handling**: Use `LV_EVENT_CLICKED` (standard press+release) for all UI interactions - more forgiving of natural finger movement than `LV_EVENT_SHORT_CLICKED`
- **Label updates**: Use delta checking to prevent unnecessary redraws - only call `lv_label_set_text()` when values change

### Critical Integration Points
1. **Main loop sequence** (`main.cpp`):
   ```cpp
   DisplayDriver::init() → TouchDriver::init() → ScreenshotServer::init()
   → FluidNCClient::init() → UISplash::show() → UIMachineSelect::show() 
   → [user selects machine] → UICommon::createMainUI() → FluidNCClient::connect()
   → UICommon::createStatusBar() → UITabs::createTabs()
   ```

2. **FluidNC Communication**:
   - WebSocket client connects to FluidNC using machine configuration (IP/hostname + port, default 81)
   - Uses **automatic reporting** (`$Report/Interval=250\n`) - FluidNC pushes updates every 250ms, no polling needed
   - Parses three message types:
     - Status reports (binary frames): `<Idle|MPos:x,y,z|FS:feed,spindle|Ov:feed,rapid,spindle|WCO:x,y,z|SD:percent,filename>`
     - GCode parser state: `[GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]`
     - Realtime feedback: `[MSG:...]`, `[G92:...]`, etc.
   - **Work Position Calculation**: WPos = MPos - WCO (Work Coordinate Offset)
     - FluidNC sends MPos in every status report
     - WCO is sent periodically (not every report) to save bandwidth
     - WCO values are cached and used for continuous WPos calculation
   - **Feed/Spindle Values**: Parsed from both status reports (FS:) and GCode state (F/S)
     - Status report FS: values are current/actual feed and spindle
     - GCode state F/S values are programmed/commanded values (used as fallback)
   - **SD Card File Progress**: Parsed from status reports (SD:percent,filename)
     - Tracks elapsed time from file start using millis()
     - Calculates estimated completion time based on percentage and elapsed time
     - Displayed in Status tab header spanning columns 2-4 when file is running
     - Format: filename (truncated), progress bar, elapsed time (H:MM), estimated time (Est: H:MM)
   - **Delta Checking**: UI updates use cached values to prevent unnecessary redraws
     - Only updates labels when values actually change
     - Eliminates visual glitches from constant LVGL label redraws
     - Cached values initialized to sentinel values (-9999 for positions, -1 for rates)
   - UI updates every 250ms from FluidNC status in main loop
   - Connection initiated after machine selection in `UICommon::createMainUI()`

3. **State Popups (HOLD and ALARM)**:
   - `UICommon` manages modal popups for HOLD and ALARM states that appear across all tabs
   - **HOLD Popup**:
     - Appears when machine enters HOLD state (cyan/teal border, STATE_HOLD color)
     - Shows "HOLD" title (32pt) and last FluidNC message (24pt, 520px width, 60px top offset)
     - Buttons: "Close" (dismisses, prevents reappearance until state changes) and "Resume" (sends `~` cycle start)
     - Resume button does NOT close popup - waits for actual state change from FluidNC
   - **ALARM Popup**:
     - Appears when machine enters ALARM state (red border, STATE_ALARM color)
     - Shows "ALARM" title (32pt) and last FluidNC message (24pt, 520px width, 60px top offset)
     - Buttons: "Close" (dismisses, prevents reappearance) and "Clear Alarm" (sends `\x18` soft reset + `$X\n` unlock)
     - Clear Alarm button does NOT close popup - waits for actual state change from FluidNC
   - **Dismissal Logic**:
     - When user clicks "Close", popup disappears and sets dismissed flag
     - Popup will NOT reappear while still in same state (even though condition persists)
     - When state changes to anything else, dismissed flag is cleared
     - If machine returns to HOLD/ALARM, popup will appear again (because flag was reset)
     - Resume/Clear Alarm buttons do NOT set dismissal flag - just send commands
   - **Auto-hide**: Popups automatically close when state changes (tracked every 250ms in main loop)
   - Implementation: `UICommon::checkStatePopups()` called from main loop, manages show/hide based on `FluidNCStatus.state` and `last_message`

4. **Machine Selection**:
   - `UIMachineSelect` supports up to 5 machines with reordering (up/down buttons), edit, delete, and add functionality
   - Machines stored in Preferences as array under "machines" key (MachineConfig struct)
   - Selected machine stored in Preferences under "machine" key
   - Machine name displayed in status bar with connection symbol
   - **Edit Mode Layout**: 458px machine buttons + 60×60px control buttons (up/down/edit/delete) with consistent 5px gaps (matches macro list spacing)
   - **Add button**: Single button in upper right corner (green, 120×45px), only visible in edit mode
   - **Machine switching**: Clicking right side of status bar shows confirmation dialog, then restarts ESP32 to switch machines cleanly
   - **Add/Edit Dialog**: 780×460px modal with 2-column layout (64% left: Name/SSID/URL, 33% right: Connection/Password/Port)
     - All fields: 18pt font for improved readability
     - Text areas: 40px height, Connection dropdown: 48px height for alignment
     - Title: Uppercase gray (TEXT_DISABLED) matching settings section style
     - Layout: Absolute positioning for main container prevents keyboard-triggered shifts
     - Buttons: Fixed at bottom (y=370) with Save (left) and Cancel (right)

5. **Settings → General Tab**:
   - **FILES Section** (second column at x=360):
     - "Folders on Top" toggle switch controls file browser sorting behavior
     - Preference: `PREFS_SYSTEM_NAMESPACE`, key `"folders_on_top"` (bool, default: false)
     - When enabled: Folders appear first in Files tab (alphabetically, then files alphabetically)
     - When disabled: Files appear first, then folders (original behavior)
     - Implementation: Conditional `std::sort()` in `ui_tab_files.cpp` parseFileList()
     - Switch UI: Label + switch + description text, follows standard settings layout pattern

6. **Files Tab Architecture** (`ui_tab_files.h/cpp`):
   - **Three Storage Sources**:
     - `StorageSource::FLUIDNC_SD` - FluidNC SD card (default: `/sd/`)
     - `StorageSource::FLUIDNC_FLASH` - FluidNC flash filesystem (default: `/localfs/`)
     - `StorageSource::DISPLAY_SD` - ESP32 local SD card (default: `/`)
   - **Per-Source Caching**:
     - Each source maintains independent cache (`fluidnc_sd_cache`, `fluidnc_flash_cache`, `display_sd_cache`)
     - Cache includes: path, file list (name/size/is_directory), validity flag
     - Cache invalidated on navigation, refresh, or storage switch
   - **Display SD Card Handling**:
     - Initialized via `UploadManager::init()` on first access
     - Uses `isDisplaySDAvailable()` to check `SD.cardType() != CARD_NONE` before operations
     - If card not available: Shows "SD card not available" warning, clears file list, does NOT auto-switch storage
     - Card check performed on: storage switch (cached or new), navigation, refresh, upload operations
     - Hardware-specific SPI pins via `#ifdef HARDWARE_ADVANCE`
   - **File Operations**:
     - Play button: Sends `$SD/Run=` or `$LocalFS/Run=` command to FluidNC
     - Delete button: Shows confirmation dialog, sends `$SD/Delete=` or `$LocalFS/Delete=` command
     - Upload button (Display SD only): Opens upload dialog, initiates `UploadManager::uploadFile()`
   - **Upload Flow**:
     1. User clicks upload button → shows dialog with filename and size
     2. Confirm → creates progress dialog with progress bar and percentage label
     3. `UploadManager::uploadFile()` reads file in 8KB chunks, POST to FluidNC `/upload`
     4. Progress callback updates UI bar and label via `UITabFiles::updateUploadProgress()`
     5. Completion callback shows success/error, refreshes FluidNC file list
   - **UI Layout**: Storage dropdown + path label + refresh/up buttons + file list container (270px height, vertical scroll)

7. **Status Bar Layout** (60px height, 18pt font, split into clickable areas):
   - **Left area** (550px): Machine state (IDLE/RUN/ALARM) - 32pt uppercase, vertically centered, color-coded
     - Clicking navigates to Status tab (LV_EVENT_CLICKED)
   - **Center**: Work Position (top line, orange label) and Machine Position (bottom line, cyan label)
     - Separate labels for each axis (X/Y/Z) with axis-specific colors (X=cyan, Y=green, Z=magenta)
     - Format: "WPos:" then "X:0000.000 Y:0000.000 Z:0000.000" (4 digits before decimal)
   - **Right area** (240px): Machine name with symbol (top line, blue) and WiFi network (bottom line, cyan)
     - Clicking shows confirmation dialog "This will restart the controller. Continue?" with "⚡ Restart" button
     - Confirmation triggers ESP32 restart to cleanly reload with new machine selection (LV_EVENT_CLICKED)

7. **Tab creation delegation**:
   - `UITabs` creates tabview structure, delegates content to `UITab*::create()`
   - Each tab module is responsible for its own layout and event handlers
   - Nested tabviews use `LV_DIR_LEFT` for vertical tabs (see `UITabControl`)

8. **Serial debugging**: All modules use `Serial.println/printf` at 115200 baud with heap/PSRAM monitoring

## Development Workflows

### Building & Flashing
**CRITICAL**: On Windows, the `pio` shortcut often doesn't work. Use the full PlatformIO path with proper PowerShell quoting.

**ALWAYS upload after successful builds** - use `--target upload` instead of just building:

```powershell
# Windows: Full path to platformio.exe - MUST use quotes and call operator (&)
# ALWAYS use --target upload to flash after building

# Build and upload for Basic hardware (4MB flash, PWM backlight)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment elecrow-crowpanel-7-basic --target upload

# Build and upload for Advance hardware (16MB flash, I2C backlight)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --environment elecrow-crowpanel-7-advance --target upload

# Serial monitor (115200 baud)
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200

# Clean build
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t clean
```

**Windows PowerShell Note**: The call operator `&` and quotes around the path are REQUIRED because the path contains environment variables and backslashes. Without quotes, PowerShell will throw a parser error. If `platformio.exe` shortcut works in your PATH, you can use it directly without the full path.

### Screenshot Debugging
- WiFi credentials stored in ESP32 Preferences (`PREFS_NAMESPACE "fluidtouch"`)
- When connected, access via browser at `http://<ESP32-IP>/screenshot`
- Returns live screen as BMP (800×480 RGB888, ~1.2MB)
- Uses persistent PSRAM buffer to avoid allocation failures

### Memory Management Rules
- **Display buffers**: 2× 256KB in PSRAM (dual buffering)
- **Screenshot buffer**: 768KB in PSRAM (lazy allocated on first request)
- **LVGL heap**: 256KB in PSRAM via custom allocator (`LV_MEM_POOL_ALLOC`)
- **Heap monitoring**: Check logs for "Free heap" and "Free PSRAM" at startup and every 5s

## Project-Specific Conventions

### UI Theme System (`include/ui/ui_theme.h`)
**CRITICAL**: All colors MUST use the centralized `UITheme` namespace - NO hardcoded `lv_color_hex()` calls allowed.

Theme uses semantic naming organized by category:
```cpp
// Accent colors - use for primary/secondary selections
UITheme::ACCENT_PRIMARY          // Main tab selected (blue)
UITheme::ACCENT_SECONDARY        // Sub-tab selected (teal)

// Machine states - use for CNC machine status display
UITheme::STATE_IDLE              // Idle state (green)
UITheme::STATE_RUN               // Running state (cyan)
UITheme::STATE_ALARM             // Alarm/Error state (red)

// Axis colors - use for X/Y/Z axis displays
UITheme::AXIS_X                  // X-axis (cyan)
UITheme::AXIS_Y                  // Y-axis (green)
UITheme::AXIS_Z                  // Z-axis (magenta)

// Position displays
UITheme::POS_MACHINE             // Machine position label (cyan)
UITheme::POS_WORK                // Work position label (orange)

// Buttons - specific action colors
UITheme::BTN_PLAY                // Play/Success buttons (green)
UITheme::BTN_ESTOP               // Emergency stop (dark red)
UITheme::BTN_CONNECT             // Connect button (blue)

// UI feedback
UITheme::UI_SUCCESS              // Success messages (green)
UITheme::UI_WARNING              // Warning messages (orange)
UITheme::UI_INFO                 // Info text (cyan)

// Backgrounds, text, borders - standard grays
UITheme::BG_DARK, BG_MEDIUM, TEXT_LIGHT, BORDER_MEDIUM
```

**Adding new colors**: Update `ui_theme.h` with semantic naming (purpose-based, not color-based).

### Configuration Constants
All other hardcoded values live in `include/config.h`:
- Screen dimensions, pin mappings, I2C addresses
- UI layout constants (`STATUS_BAR_HEIGHT`, `TAB_BUTTON_HEIGHT`)
- Buffer sizes, timing values, feature flags

### Adding New UI Tabs
1. Create header in `include/ui/tabs/ui_tab_<name>.h`
2. Create implementation in `src/ui/tabs/ui_tab_<name>.cpp`
3. Add `static void create(lv_obj_t *tab)` factory method
4. Register in `UITabs::createTabs()` with `lv_tabview_add_tab()`
5. Delegate creation via `UITabs::create<Name>Tab()` private method

## Key Files & Patterns

**Core Structure**:
- **`platformio.ini`**: Flash/PSRAM config (`dio_opi`), partition tables, build flags, environment name `elecrow-crowpanel-7-basic`
- **`include/lv_conf.h`**: LVGL configuration (color depth, memory, features) - 1400+ lines
- **`include/config.h`**: Central configuration for ALL hardcoded values (BUFFER_LINES=480 for full-screen buffering)

**Hardware/Core Modules** (`core/`):
- **`src/core/display_driver.cpp`**: LovyanGFX RGB parallel setup (lines 11-63 are pin mappings) and GT911 touch panel configuration (I2C pins, address, panel linkage)
- **`src/core/touch_driver.cpp`**: LVGL input device that delegates to LovyanGFX's `lcd->getTouch()` method

**Network Modules** (`network/`):
- **`src/network/screenshot_server.cpp`**: WiFi setup, BMP conversion from RGB565 frame buffer
- **`src/network/fluidnc_client.cpp`**: FluidNC WebSocket client with automatic reporting (no polling), status parsing, WCO handling, F/S parsing from both status reports and GCode state, and SD card file progress tracking. Terminal callback currently disabled.
  - **Connection Flow**: 1s reconnect attempts until first successful status report, then 24h interval to effectively disable auto-reconnect
  - **Disconnect Handling**: Shows error dialog only if previously connected successfully (prevents false alarms during initial handshake)
  - **Keepalive**: 15s ping interval, 5s pong timeout, disconnects after 2 missed pongs

**UI Assets**:
- **`src/ui/fonts/jetbrains_mono_16.c`**: Monospace font for terminal display
- **`src/ui/images/fluidnc_logo.c`**: FluidNC logo for splash screen

**Main Application**:
- **`src/main.cpp`**: Entry point, initialization sequence, main loop with LVGL tick handling

**UI Modules** (`ui/`):
- **`src/ui/ui_common.cpp`**: Status bar implementation with separate axis labels, delta checking for smooth updates, clickable left/right areas for navigation and machine switching, and modal HOLD/ALARM state popups with dismissal tracking
- **`src/ui/ui_machine_select.cpp`**: Machine selection screen with reordering, edit, delete, and add functionality (up to 5 machines stored in Preferences)
- **`src/ui/tabs/ui_tab_status.cpp`**: Status tab with delta-checked position displays, feed/spindle rates with overrides, 8 modal state fields, message display, and SD card file progress (filename, progress bar, elapsed/estimated time)
- **`src/ui/tabs/ui_tab_terminal.cpp`**: Terminal tab with WebSocket message display, auto-scroll toggle, 8KB buffer with batched UI updates (currently disabled via commented callback in FluidNCClient)
- **`src/ui/tabs/ui_tab_files.cpp`**: File browser with three storage sources (FluidNC SD/Flash, Display SD), per-source caching, SD card detection, and upload functionality
- **`src/ui/upload_manager.cpp`**: SD card file upload manager with chunked HTTP POST to FluidNC, progress tracking, and 10MB file size limit

### Control Sub-Tabs Layout

#### Jog Tab (ui_tab_control_jog.cpp)
- **Layout**: XY section (left) + Z section (right)
- **Headers**: "XY JOG" (18pt, AXIS_XY color, x=167) and "Z JOG" (18pt, AXIS_Z color, x=467)
- **XY Section**:
  - Step label: "XY Step" at (5, 9) - 14pt font
  - Step buttons: 6 vertical buttons (0.1, 1, 10, 50, 100, 500) at x=10, starting y=30, 45×45px, 14pt font
  - Jog pad: 3×3 grid at (85, 30), 70×70px buttons, diagonal buttons with AXIS_XY color, N/S with AXIS_Y, E/W with AXIS_X
  - Center button: Step display (non-clickable, BG_DARKER)
  - Feed control: Label + value display + mm/min unit (14pt) at y=280, adjustment buttons (±100, ±1000) at y=300, 14pt font
- **Z Section**:
  - Step label: "Z Step" at (395, 9) - 14pt font
  - Step buttons: 3 vertical buttons (0.1, 1, 10) at x=395, starting y=30, 45×45px, 14pt font
  - Z+ button: 70×70px at (460, 30), AXIS_Z color
  - Step display: 70×70px at (460, 110), BG_DARKER background
  - Z- button: 70×70px at (460, 190), AXIS_Z color
  - Feed control: Label + value display + mm/min unit (14pt) at y=280, adjustment buttons at y=300, 14pt font
- **Cancel button**: Octagon "STOP" button at (560, 110), 70×70px

#### Joystick Tab (ui_tab_control_joystick.cpp)
- **Layout**: Horizontal flex layout (XY joystick, info center, Z slider)
- **Axis Selection**: Three mode buttons (XY, X, Y) below joystick with white 3px border indicating selected mode
  - MODE_XY: Circular joystick (220×220px) with crosshairs for simultaneous XY movement
  - MODE_X: Horizontal slider (220×80px) with center line for X-axis only movement
  - MODE_Y: Vertical slider (80×220px) with center line for Y-axis only movement
  - All controls centered in container for smooth visual transitions between modes
- **Dynamic Labels**:
  - xy_jog_label: Updates text ("XY JOG", "X JOG", "Y JOG") and color based on selected mode
  - xy_percent_label: Updates color (JOYSTICK_XY=cyan, AXIS_X=cyan, AXIS_Y=green)
- **Z Slider**: 80×220px vertical slider with draggable knob, quadratic response curve
- **Response Curve**: output = sign(input) × (input/100)² × 100 for fine control near center
- **Info Display**: Current percentage, feed rate (mm/min), and max feed rate from settings
- **Positioning**: Parent container 20px top padding, info container 50px top padding for vertical alignment

#### Probe Tab (ui_tab_control_probe.cpp)
- **Layout**: Left column (0-220px) for probe buttons, right column (260-620px) for parameters and results
- **Axis Sections**:
  - Headers: "X-AXIS", "Y-AXIS", "Z-AXIS" in TEXT_DISABLED color (gray)
  - Buttons: 100×50px, colored with axis colors (AXIS_X/Y/Z), only negative directions + Z- only
- **Parameters Section** (y=45-210):
  - Labels at x=260, 18pt font, vertically centered at y+11 from field top
  - Input fields: 120×45px at x=420, 18pt font
  - Units: 18pt font at x=550, vertically centered with fields
  - Fields: Feed Rate, Max Distance, Retract, Probe Thickness
- **Results Section** (y=295):
  - Field: 370×50px textarea, 16pt font, 3px padding for 2-line display
  - Format: "SUCCESS\nZ: -137.505 mm" or "FAILED\nNo contact detected"
  - Only shows the probed axis value, not all three axes

### Status Tab Layout (ui_tab_status.cpp)
- **Top Section**: STATE (left, x=0) + FILE PROGRESS (spans columns 2-4, x=230-780, hidden when not printing)
  - File progress shows: filename (truncated to 350px), progress bar (0-100%), elapsed time (H:MM), estimated completion time (Est: H:MM)
  - Progress container has dark gray background with border, appears only when `is_sd_printing` is true
- **Column Layout** (after separator line at y=60):
  - Column 1 (x=0): Work Position (X/Y/Z)
  - Column 2 (x=225): Machine Position (X/Y/Z)
  - Column 3 (x=475): Feed Rate + Override, Spindle + Override
  - Column 4 (x=615/735): Modal states (9 fields: WCS, PLANE, DIST, UNITS, MOTION, FEED, SPINDLE, COOLANT, TOOL)
- **Bottom Section** (y=325): MESSAGE label spanning columns 1-3 (x=0, width=550px)
- **Modal States**: 26px vertical spacing, 20pt font, teal labels (ACCENT_SECONDARY), semantic value colors
- **Position Format**: "X  0000.000" (4 digits before decimal, 3 after, double-space after axis letter)
- **Padding**: 10px tab padding for consistent margins

## Common Pitfalls

## Common Pitfalls

1. **Memory allocation**: Never use `malloc()` for buffers >10KB - always use `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`
2. **LVGL tick**: Forgetting `lv_tick_inc()` breaks timers and input devices - must call every loop
3. **File structure**: UI files MUST be in `ui/` subdirectory (both `include/ui/` and `src/ui/`)
4. **Tab scrolling**: Most tabs have scrolling disabled - re-enable only if content exceeds screen height
5. **Display buffer size**: BUFFER_LINES=480 for full-screen buffering - provides smooth rendering with 8MB PSRAM available
6. **RGB565 byte order**: LovyanGFX returns byte-swapped RGB565 - swap before decoding (see `screenshot_server.cpp:19-29`)
7. **Color usage**: NEVER use `lv_color_hex()` directly - always use `UITheme::*` constants for maintainability and consistency
8. **Event types**: Always use `LV_EVENT_CLICKED` for touch interactions - provides better UX than `LV_EVENT_SHORT_CLICKED` by being more tolerant of slight finger movement
9. **Label updates**: Always use delta checking - only call `lv_label_set_text()` when values actually change to prevent visual glitches
10. **Static label pointers**: UI update methods require static member pointers to labels - never use local variables for labels that need live updates
11. **Machine switching**: Use `ESP.restart()` to switch between machines - cleanly avoids LVGL memory fragmentation issues that can occur when rebuilding entire UI trees
12. **WCO caching**: Work position requires cached WCO values since FluidNC only sends WCO periodically - calculate WPos = MPos - WCO on every status update
13. **SD file progress**: FluidNC sends `SD:percent,filename` in status reports - track start time on first detection, calculate elapsed/estimated times based on percentage and elapsed duration
14. **No polling needed**: FluidNC automatic reporting (`$Report/Interval=250\n`) handles all status updates - no fallback polling required
15. **Terminal callback**: Terminal updates can be enabled/disabled by commenting/uncommenting the `terminalCallback()` call in `fluidnc_client.cpp` WebSocket event handler
16. **State popup buttons**: Resume and Clear Alarm buttons send commands but do NOT manually close popups - popups auto-close when FluidNC state actually changes, providing visual feedback that command was sent
17. **Connection error handling**: Only show disconnect error dialogs if `everConnectedSuccessfully` flag is true - prevents false alarms during initial connection handshake when connection attempts are still in progress
18. **Hardware-specific builds**: Always use correct environment (`-e elecrow-crowpanel-7-basic` or `-e elecrow-crowpanel-7-advance`) - RGB pin mappings are completely different and will not work if mixed
19. **Advance display timing**: 14MHz pixel clock provides best stability for Advance hardware - 18MHz (from Elecrow example) may cause glitching depending on signal integrity
20. **STC8H1K28 control**: Advance backlight and touch reset are controlled via I2C to STC8H1K28 at 0x30 - no direct GPIO manipulation needed
21. **Touch panel configuration**: GT911 touch panel MUST be configured in LGFX class (display_driver.cpp) - add `lgfx::Touch_GT911 _touch_instance`, configure with I2C pins, and call `_panel_instance.setTouch(&_touch_instance)`. Touch driver only delegates to LovyanGFX via `lcd->getTouch()` - it doesn't initialize GT911 itself
22. **Preferences usage**: Always read preferences at point of use rather than caching globally - prevents stale data when settings change. Examples: Files tab reads `folders_on_top` preference in parseFileList(), ensuring fresh value on every directory refresh
23. **Power management state awareness**: PowerManager only applies power saving (dim/sleep) when machine is in IDLE or DISCONNECTED states - all other states (RUN, ALARM, HOLD, JOG) keep full brightness for operator safety and visibility
24. **Display SD card handling**: Always check `isDisplaySDAvailable()` before Display SD operations - shows "SD card not available" without auto-switching storage. User can manually switch storage sources via dropdown. SD card state checked on: storage switch, navigation, refresh, and upload operations

## External Dependencies

- **FluidNC**: CNC controller firmware (WebSocket connection on port 81/82, automatic reporting enabled)
- **LVGL 9.4.0+**: UI library via PlatformIO lib_deps
- **LovyanGFX 1.2.7+**: Hardware display driver with RGB parallel support
- **WebSockets 2.5.4+**: WebSocket client library for FluidNC communication
- **ESP32 Arduino Framework**: Core platform APIs (Wire, WiFi, WebServer, Preferences)
