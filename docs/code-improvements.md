# FluidTouch Code Improvements & Refactoring

*Last Updated: November 1, 2025*

This document outlines potential code improvements, refactoring opportunities, and architectural enhancements identified during pre-release code review. Items are prioritized for implementation.

---

---

## Medium Priority (Nice to Have)

### 1. Logging System with Debug Levels
**Problem:** `Serial.printf()` scattered throughout, no control over verbosity

**Current State:**
```cpp
Serial.printf("[Files] Loading...\n");
Serial.printf("[Probe] X- probe result: %.3f\n", result);
```

**Proposed Solution:**
```cpp
// In config.h:
#define DEBUG_LEVEL 2  // 0=none, 1=errors, 2=info, 3=verbose

// In new include/debug.h:
#if DEBUG_LEVEL >= 1
    #define LOG_ERROR(tag, fmt, ...)   Serial.printf("[ERROR][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
    #define LOG_ERROR(tag, fmt, ...)   ((void)0)
#endif

#if DEBUG_LEVEL >= 2
    #define LOG_INFO(tag, fmt, ...)    Serial.printf("[INFO][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
    #define LOG_INFO(tag, fmt, ...)    ((void)0)
#endif

#if DEBUG_LEVEL >= 3
    #define LOG_VERBOSE(tag, fmt, ...) Serial.printf("[DEBUG][%s] " fmt "\n", tag, ##__VA_ARGS__)
#else
    #define LOG_VERBOSE(tag, fmt, ...) ((void)0)
#endif

// Usage:
LOG_INFO("Files", "Loading file list for: %s", path.c_str());
LOG_VERBOSE("Probe", "X- probe result: %.3f", result);
LOG_ERROR("FluidNC", "Connection failed: %s", error);
```

**Benefits:**
- Control debug output verbosity at compile time
- Zero overhead when disabled
- Consistent log format
- Tagged output for filtering

## Low Priority (Post-Release)

### 2. File Structure Reorganization
**Current Structure:**
```
include/
  ├── config.h
  ├── display_driver.h
  ├── fluidnc_client.h
  ├── fluidnc_logo.h
  ├── jetbrains_mono_16.h
  ├── lv_conf.h
  ├── screenshot_server.h
  ├── touch_driver.h
  └── ui/
      ├── machine_config.h
      ├── ui_*.h
      └── tabs/
```

**Proposed Structure:**
```
include/
  ├── config.h              # Global configuration
  ├── version.h             # Version management
  ├── debug.h               # Logging macros
  ├── core/                 # Core system
  │   ├── display_driver.h
  │   └── touch_driver.h
  ├── hardware/             # Hardware-specific
  │   ├── fluidnc_logo.h
  │   └── screenshot_server.h
  ├── network/              # Network/Communication
  │   └── fluidnc_client.h
  ├── ui/                   # All UI (existing structure good)
  │   ├── ui_constants.h    # NEW: UI-related constants
  │   ├── ui_dialogs.h      # NEW: Shared dialog utilities
  │   ├── machine_config.h
  │   └── ...
  └── lvgl/                 # LVGL configuration
      └── lv_conf.h
```

**Benefits:**
- Clearer separation of concerns
- Easier to navigate for new contributors
- Logical grouping of related functionality

**Caution:** Breaking change - update all `#include` paths

---

## Notes & Considerations

### Breaking Changes
Items marked as "breaking changes" should be done between major versions:
- File structure reorganization (#9)
- API changes to existing modules
- Configuration format changes

### Testing After Changes
Any code changes should include:
- Build test (verify compilation)
- Flash and boot test (verify no runtime errors)
- Feature test (verify affected features work)
- Memory test (check heap/PSRAM usage)

### Future Enhancements Not Listed
These improvements focus on code quality and architecture. Feature enhancements (new tabs, new functionality) should be tracked separately in a features/roadmap document.

---

## Implementation Checklist

**Quick Wins (Can do now):**
- [x] Remove TODO comments in joystick file

**Requires More Work:**
- [ ] Implement debug logging system

**Post-Release:**
- [ ] Consider file structure reorganization

---

*Generated from pre-release code review - November 2025*
