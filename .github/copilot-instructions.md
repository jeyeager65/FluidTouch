# FluidTouch - AI Coding Agent Instructions

## Project Overview
FluidTouch is an ESP32-S3 embedded touchscreen CNC controller for FluidNC machines, running on the Elecrow CrowPanel 7" display (800x480). The project uses PlatformIO, LVGL 9.3 for UI, and LovyanGFX for hardware-accelerated display rendering.

**Status**: Active development - core architecture complete, features in progress.

## Architecture & Design Patterns

### Hardware Platform (ESP32-S3-WROOM-1-N4R8)
- **Flash**: 4MB DIO mode
- **PSRAM**: 8MB Octal PSRAM (`dio_opi` memory type)
- **Display**: 800x480 RGB parallel interface (16-bit RGB565)
- **Touch**: GT911 I2C controller (address 0x5D, pins SDA=19, SCL=20, RST=38)
- **Critical**: ALL large buffers MUST use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` to allocate in PSRAM, never regular heap

### Module Organization Pattern
The codebase follows a **strict modular pattern** with clear separation:

1. **Driver Modules** (`src/` + `include/`):
   - `DisplayDriver` - LovyanGFX RGB parallel display with LVGL integration
   - `TouchDriver` - GT911 I2C touch controller with LVGL input device
   - `ScreenshotServer` - WiFi web server for remote screenshots via LovyanGFX `readRect()`

2. **UI Module Hierarchy** (all under `ui/` subdirectory):
   - `UICommon` - Shared status bar with machine state display
   - `UISplash` - Startup splash screen (2.5s duration)
   - `UITabs` - Main tabview orchestrator, delegates to tab modules
   - `UITab*` - Individual tab modules (`UITabStatus`, `UITabControl`, etc.)
   - **Nested tabs**: `UITabControl` contains sub-modules in `tabs/control/` (Actions, Jog, Joystick, Probe, Overrides)

3. **Module Naming Convention**:
   - Class files: `ui_tab_control.h/cpp` → class `UITabControl`
   - All UI classes use static `create(lv_obj_t *parent)` factory methods
   - Headers in `include/`, implementations in `src/` (matching structure)

### LVGL 9.3 Specifics
- **Color depth**: RGB565 (16-bit) via `LV_COLOR_DEPTH 16`
- **Memory**: 256KB LVGL heap allocated in PSRAM (see `lv_conf.h`)
- **Display buffers**: Dual 1/3-screen buffers (800×160 lines each) in PSRAM
- **Tick handling**: Manual `lv_tick_inc()` + `lv_timer_handler()` in main loop every 5ms
- **No scrolling**: Most tabs disable `LV_OBJ_FLAG_SCROLLABLE` for fixed layouts
- **Font**: Montserrat 18pt for tab buttons and primary UI text

### Critical Integration Points
1. **Main loop sequence** (`main.cpp`):
   ```cpp
   DisplayDriver::init() → TouchDriver::init() → ScreenshotServer::init()
   → UISplash::show() → UICommon::init() → UITabs::createTabs()
   ```

2. **Tab creation delegation**:
   - `UITabs` creates tabview structure, delegates content to `UITab*::create()`
   - Each tab module is responsible for its own layout and event handlers
   - Nested tabviews use `LV_DIR_LEFT` for vertical tabs (see `UITabControl`)

3. **Serial debugging**: All modules use `Serial.println/printf` at 115200 baud with heap/PSRAM monitoring

## Development Workflows

### Building & Flashing
```powershell
# Build project
pio run

# Upload to ESP32-S3 (via USB)
pio run -t upload

# Serial monitor (115200 baud)
pio device monitor -b 115200

# Clean build
pio run -t clean
```

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

### UI Styling Patterns
```cpp
// Main tab buttons (horizontal top bar)
lv_obj_set_style_bg_color(tabview, lv_color_hex(0x0078D7), LV_PART_ITEMS | LV_STATE_CHECKED);  // Blue

// Sub-tab buttons (vertical left bar in Control tab)
lv_obj_set_style_bg_color(sub_tabview, lv_color_hex(0x00AA88), LV_PART_ITEMS | LV_STATE_CHECKED);  // Teal

// Background colors
lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a1a), 0);  // Dark main background
lv_obj_set_style_bg_color(content, lv_color_hex(0x2a2a2a), 0);  // Lighter content areas
```

### Configuration Constants
All hardcoded values live in `include/config.h`:
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

- **`platformio.ini`**: Flash/PSRAM config (`dio_opi`), partition tables, build flags
- **`include/lv_conf.h`**: LVGL configuration (color depth, memory, features) - 1400+ lines
- **`src/main.cpp`**: Entry point, initialization sequence, main loop with LVGL tick handling
- **`src/display_driver.cpp`**: LovyanGFX RGB parallel setup (lines 11-63 are pin mappings)
- **`include/config.h`**: Central configuration for ALL hardcoded values
- **`src/screenshot_server.cpp`**: WiFi setup, BMP conversion from RGB565 frame buffer

## Common Pitfalls

1. **Memory allocation**: Never use `malloc()` for buffers >10KB - always use `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`
2. **LVGL tick**: Forgetting `lv_tick_inc()` breaks timers and input devices - must call every loop
3. **File structure**: UI files MUST be in `ui/` subdirectory (both `include/ui/` and `src/ui/`)
4. **Tab scrolling**: Most tabs have scrolling disabled - re-enable only if content exceeds screen height
5. **Display buffer size**: Changing `BUFFER_LINES` in `config.h` affects memory usage and performance
6. **RGB565 byte order**: LovyanGFX returns byte-swapped RGB565 - swap before decoding (see `screenshot_server.cpp:19-29`)

## External Dependencies

- **FluidNC**: CNC controller firmware (target device, not integrated yet)
- **LVGL 9.3.0**: UI library via PlatformIO lib_deps
- **LovyanGFX 1.2.7+**: Hardware display driver with RGB parallel support
- **ESP32 Arduino Framework**: Core platform APIs (Wire, WiFi, WebServer, Preferences)
