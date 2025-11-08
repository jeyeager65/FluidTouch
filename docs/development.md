# FluidTouch Development Guide

> Building, debugging, and contributing to FluidTouch

## Table of Contents

- [Development Environment](#development-environment)
- [Building from Source](#building-from-source)
- [Project Architecture](#project-architecture)
- [Code Style Guidelines](#code-style-guidelines)
- [Debugging](#debugging)
- [Contributing](#contributing)

---

## Development Environment

### Prerequisites

**Required:**
- [Visual Studio Code](https://code.visualstudio.com/)
- [PlatformIO IDE Extension](https://platformio.org/install/ide?install=vscode)
- [Git](https://git-scm.com/)
- USB-C cable with data support

**Optional:**
- [Elecrow CrowPanel 7" ESP32-S3 HMI Display](https://www.elecrow.com/esp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html)
- Serial terminal (PlatformIO includes one)
- Chrome/Edge browser (for ESP Web Tools testing)

### Repository Setup

```bash
# Clone repository
git clone https://github.com/jeyeager65/FluidTouch.git
cd FluidTouch

# Checkout development branch
git checkout dev

# Open in VS Code
code .
```

---

## Building from Source

### PlatformIO Commands

**Windows PowerShell:**
```powershell
# Build only
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run

# Build and upload
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload

# Clean build
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -t clean

# Serial monitor
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
```

**Linux/macOS:**
```bash
# Build only
platformio run

# Build and upload
platformio run --target upload

# Clean build
platformio run -t clean

# Serial monitor
platformio device monitor -b 115200
```

### Build Output

**Flash Usage:** Currently ~57.3% (1.76MB of 3.08MB)
**RAM Usage:** ~0.9% (80KB of 8.5MB)

**Binary Locations:**
- Firmware: `.pio/build/elecrow-crowpanel-7-basic/firmware.bin`
- Bootloader: `.pio/build/elecrow-crowpanel-7-basic/bootloader.bin`
- Partitions: `.pio/build/elecrow-crowpanel-7-basic/partitions.bin`

---

## Project Architecture

### Directory Structure

```
FluidTouch/
├── include/                 # Header files
│   ├── config.h            # Configuration constants
│   ├── core/               # Hardware drivers
│   ├── network/            # Network modules
│   └── ui/                 # UI headers
│       ├── ui_theme.h      # Centralized colors/constants
│       └── tabs/           # Tab UI headers
├── src/                    # Implementation files
│   ├── main.cpp           # Entry point
│   ├── core/              # Driver implementations
│   ├── network/           # Network implementations
│   └── ui/                # UI implementations
├── web/                    # ESP Web Tools installer
├── docs/                   # Documentation
└── platformio.ini         # Build configuration
```

### Module Organization

See `.github/copilot-instructions.md` for detailed architecture documentation.

**Key Principles:**
1. **Hardware Platform** - ESP32-S3 with 4MB flash + 8MB PSRAM
2. **Driver Modules** - DisplayDriver, TouchDriver (in `core/`)
3. **Network Modules** - ScreenshotServer, FluidNCClient (in `network/`)
4. **UI Hierarchy** - Modular tab system with static factory methods

---

## Code Style Guidelines

### Naming Conventions

**Classes:**
```cpp
class UITabControl {          // PascalCase
    static void create();      // camelCase methods
private:
    static lv_obj_t *button;   // snake_case for LVGL objects
};
```

**Files:**
- Headers: `ui_tab_control.h` (snake_case)
- Implementation: `ui_tab_control.cpp`
- Match class name pattern

**Constants:**
```cpp
// config.h
#define SCREEN_WIDTH 800

// ui_theme.h
namespace UITheme {
    static constexpr lv_color_t ACCENT_PRIMARY = ...;
}
```

### Memory Management

**Critical Rules:**
1. **Large buffers (>10KB):** Use `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`
2. **LVGL objects:** Use LVGL's allocator (automatic PSRAM via `lv_conf.h`)
3. **Delta checking:** Only update UI when values change
4. **Static pointers:** Required for UI elements needing live updates

### UI Development

**Theme Colors:**
```cpp
// NEVER use lv_color_hex() directly
lv_obj_set_style_bg_color(obj, UITheme::BG_MEDIUM, 0);  // ✅ Good
lv_obj_set_style_text_color(obj, lv_color_hex(0x00FF00), 0);  // ❌ Bad
```

**Event Handling:**
```cpp
// Use LV_EVENT_CLICKED for touch interactions
lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, nullptr);
```

**Label Updates:**
```cpp
// Delta checking to prevent unnecessary redraws
static float last_value = -9999.0f;
if (current_value != last_value) {
    lv_label_set_text_fmt(label, "%.3f", current_value);
    last_value = current_value;
}
```

### Module Structure

**Header Pattern:**
```cpp
#ifndef UI_TAB_EXAMPLE_H
#define UI_TAB_EXAMPLE_H

#include <lvgl.h>

class UITabExample {
public:
    static void create(lv_obj_t *parent);
    static void update();  // Optional, for dynamic updates
    
private:
    static lv_obj_t *label;
    static void event_handler(lv_event_t *e);
};

#endif
```

**Implementation Pattern:**
```cpp
#include "ui/tabs/ui_tab_example.h"
#include "ui/ui_theme.h"

// Static member initialization
lv_obj_t *UITabExample::label = nullptr;

void UITabExample::create(lv_obj_t *parent) {
    // Clear scrolling if fixed layout
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create UI elements
    label = lv_label_create(parent);
    lv_label_set_text(label, "Example");
    lv_obj_set_style_text_color(label, UITheme::TEXT_LIGHT, 0);
}
```

---

## Debugging

### Serial Debugging

**Monitor Output:**
```bash
platformio device monitor -b 115200
```

**Debug Macros:**
```cpp
Serial.println("Debug message");
Serial.printf("Value: %d\n", value);
Serial.printf("Free heap: %d, Free PSRAM: %d\n", 
    ESP.getFreeHeap(), ESP.getFreePsram());
```

### Screenshot Server

Access live display at `http://[ESP32-IP]`

**Benefits:**
- No serial connection needed
- See actual rendered UI
- Document UI states
- Debug layout issues
- Share visual bugs

### Memory Monitoring

**Check Heap:**
```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
Serial.printf("Min free heap: %d bytes\n", ESP.getMinFreeHeap());
```

**LVGL Memory:**
```cpp
lv_mem_monitor_t mon;
lv_mem_monitor(&mon);
Serial.printf("LVGL used: %d KB\n", mon.total_size - mon.free_size);
```

### Common Issues

**Out of Memory:**
- Check large buffers use PSRAM (`heap_caps_malloc`)
- Monitor LVGL heap usage
- Verify display buffers allocated correctly

**UI Not Updating:**
- Ensure `lv_tick_inc()` and `lv_timer_handler()` called in main loop
- Check static pointers initialized
- Verify event callbacks registered

**Touch Not Working:**
- Check serial log for GT911 initialization
- Verify I2C pins (SDA=19, SCL=20, RST=38)
- Test with screenshot server

---

## Contributing

### Development Workflow

1. **Fork** the repository
2. **Create branch** from `dev`:
   ```bash
   git checkout dev
   git pull origin dev
   git checkout -b feature/my-feature
   ```
3. **Make changes** following code style guidelines
4. **Test thoroughly** on hardware
5. **Commit** with clear messages:
   ```bash
   git commit -m "Add feature: description"
   ```
6. **Push** to your fork:
   ```bash
   git push origin feature/my-feature
   ```
7. **Create Pull Request** to `dev` branch

### Pull Request Guidelines

**Include:**
- Clear description of changes
- Screenshots/videos for UI changes
- Test results on hardware
- Any breaking changes noted
- Updated documentation if needed

**Requirements:**
- Code follows style guidelines
- No compiler warnings
- Flash usage remains reasonable (<70%)
- Tested on actual hardware preferred

### Branching Strategy

- **`main`:** Stable releases only
- **`dev`:** Active development
- **Feature branches:** `feature/description`
- **Bug fixes:** `fix/description`

### Testing Checklist

- [ ] Compiles without warnings
- [ ] Flash usage acceptable
- [ ] UI responsive on hardware
- [ ] Touch input works correctly
- [ ] FluidNC communication stable
- [ ] WiFi connects reliably
- [ ] No memory leaks
- [ ] Screenshot server accessible
- [ ] State transitions handled
- [ ] Error conditions tested

---

## Resources

**FluidTouch:**
- [GitHub Repository](https://github.com/jeyeager65/FluidTouch)
- [Issues](https://github.com/jeyeager65/FluidTouch/issues)

**Dependencies:**
- [LVGL Documentation](https://docs.lvgl.io/)
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [FluidNC](https://github.com/bdring/FluidNC)
- [ESP32-S3 Technical Reference](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)

**Hardware:**
- [Elecrow CrowPanel 7"](https://www.elecrow.com/esp32-display-7-inch-hmi-display-rgb-tft-lcd-touch-screen-support-lvgl.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

---

*For architecture details, see `.github/copilot-instructions.md`*
