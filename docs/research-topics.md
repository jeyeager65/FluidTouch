# FluidTouch Research Topics

This document outlines key research areas needed to complete the FluidTouch CNC controller project. Topics are prioritized based on criticality and implementation timeline.

---

## 1. ESP32 WebSocket Client Libraries ⚠️ HIGH PRIORITY

### Why Research This?
WebSocket communication is core to FluidNC connectivity. Need to choose the right library.

### Library Options to Evaluate

#### Option 1: ArduinoWebsockets
- **Repo**: https://github.com/gilmaimon/ArduinoWebsockets
- **Pros**: Modern API, good ESP32 support, SSL/TLS
- **Cons**: ?

#### Option 2: WebSockets by Markus Sattler
- **Library**: Available in Arduino Library Manager
- **Pros**: Mature, widely used
- **Cons**: ?

#### Option 3: ESP32 Built-in WebSocket
- **Include**: `WebSocketsClient.h`
- **Pros**: No external dependency
- **Cons**: ?

### Evaluation Criteria
- ✅ ESP32-S3 compatibility
- ✅ Thread-safety for use with LVGL tasks
- ✅ Automatic reconnection support
- ✅ Ping/pong keep-alive
- ✅ SSL/TLS support (future-proofing)
- ✅ Low memory footprint
- ✅ Good error handling
- ✅ Active maintenance

### Key Questions
1. Which library has best async/event-driven model for LVGL integration?
2. Can receive callbacks run in separate FreeRTOS task?
3. How to handle reconnection without blocking UI?
4. What's the RAM overhead per connection?

### Testing Plan
1. Create simple ESP32 sketch connecting to test WebSocket server
2. Measure connection time and memory usage
3. Test reconnection on WiFi disconnect
4. Verify thread-safety with dual tasks (UI + WebSocket)

### Success Criteria
- [ ] Connects to FluidNC WebSocket (port 81)
- [ ] Receives messages asynchronously
- [ ] Handles disconnect/reconnect gracefully
- [ ] Works alongside LVGL without blocking

---

## 2. LVGL Threading/Task Management ⚠️ CRITICAL

### Why Research This?
FluidTouch needs multiple concurrent tasks:
- **UI task**: LVGL timer handler every 5ms
- **WebSocket receive task**: Process incoming FluidNC messages
- **Status query task**: Send `?` command every 250ms
- **Touch input task**: GT911 polling

### FreeRTOS + LVGL Integration Patterns

#### Current Implementation
```cpp
// main.cpp - simple loop, no threading yet
void loop() {
    lv_tick_inc(5);
    lv_timer_handler();
    delay(5);
}
```

#### Target Architecture
```cpp
// UI Task (Core 1, priority 1)
void lvgl_task(void *pvParameter) {
    while (1) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// WebSocket Receive Task (Core 0, priority 2)
void websocket_receive_task(void *pvParameter) {
    while (1) {
        if (ws.available()) {
            String msg = ws.readString();
            xQueueSend(message_queue, &msg, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Status Query Task (Core 0, priority 1)
void status_query_task(void *pvParameter) {
    while (1) {
        ws.send("?");
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
```

### Thread-Safety Concerns

#### Problem: Updating LVGL Objects from WebSocket Task
```cpp
// ❌ UNSAFE - updating LVGL from non-UI task
void websocket_receive_task() {
    String status = ws.receive();
    lv_label_set_text(status_label, status.c_str());  // DANGER!
}
```

#### Solution: Use Mutex or Message Queue
```cpp
// ✅ SAFE - use mutex
SemaphoreHandle_t lvgl_mutex;

void websocket_receive_task() {
    String status = ws.receive();
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY)) {
        lv_label_set_text(status_label, status.c_str());
        xSemaphoreGive(lvgl_mutex);
    }
}

void lvgl_task() {
    if (xSemaphoreTake(lvgl_mutex, 0)) {
        lv_timer_handler();
        xSemaphoreGive(lvgl_mutex);
    }
}
```

### What to Research
- **LVGL locking mechanisms** - `lv_lock()` / `lv_unlock()` availability in LVGL 9.x
- **FreeRTOS task priorities** - Optimal priority assignment for responsiveness
- **Core affinity** - Which tasks on Core 0 vs Core 1?
- **Stack sizes** - How much stack for each task?
- **Watchdog timers** - Preventing task starvation

### Key Questions
1. Does LVGL 9.3/9.4 have built-in thread-safety or need manual mutex?
2. Should LVGL run on Core 0 or Core 1?
3. What's the minimum safe task delay for UI responsiveness?
4. How to safely pass data from WebSocket task to UI task?

### Resources
- LVGL threading documentation: https://docs.lvgl.io/master/porting/os.html
- ESP32 FreeRTOS guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
- LVGL forum: Search for "FreeRTOS" and "thread safety"

### Success Criteria
- [ ] LVGL updates without flickering/corruption
- [ ] WebSocket receives messages without blocking UI
- [ ] Touch input remains responsive during file streaming
- [ ] No deadlocks or race conditions

---

## 3. G-code Parser Implementation 📝

### Why Research This?
FluidNC sends various message formats that need parsing:
- Status reports: `<Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>`
- Error responses: `error:22`
- Coordinate offsets: `[G54:0.000,0.000,0.000]`
- System info: `[MSG:INFO: Connected]`

### Parser Requirements

#### Status Report Parsing
```cpp
struct MachineStatus {
    String state;           // "Idle", "Run", "Hold", "Alarm", etc.
    float mpos[6];          // Machine position X,Y,Z,A,B,C
    float wpos[6];          // Work position
    float feed_rate;        // Current feed rate
    float spindle_speed;    // Current spindle RPM
    int feed_override;      // Feed override percentage
    int rapid_override;     // Rapids override percentage
    int spindle_override;   // Spindle override percentage
};

bool parse_status_report(String response, MachineStatus &status);
```

#### Error Code Parsing
```cpp
int parse_error_code(String response) {
    // "error:22" → return 22
    if (response.startsWith("error:")) {
        return response.substring(6).toInt();
    }
    return 0;
}
```

#### Message Type Detection
```cpp
enum MessageType {
    MSG_STATUS_REPORT,   // <...>
    MSG_OK,              // ok
    MSG_ERROR,           // error:N
    MSG_ALARM,           // ALARM:N
    MSG_INFO,            // [MSG:...]
    MSG_SETTING,         // [G54:...]
    MSG_UNKNOWN
};

MessageType detect_message_type(String msg);
```

### Implementation Options

#### Option 1: State Machine Parser
- Char-by-char parsing
- Low memory overhead
- Fast, predictable performance

#### Option 2: String Tokenization
- Use `String.indexOf()` and `substring()`
- Easier to read/maintain
- More memory allocations

#### Option 3: Regular Expressions
- Powerful pattern matching
- Heavy memory usage
- May be too slow for ESP32

### What to Research
- **Best parsing approach** for embedded systems
- **String handling** - String vs char* for performance
- **Edge cases** - Malformed messages, buffer overflows
- **Error recovery** - Handling corrupt/truncated messages

### Key Questions
1. String manipulation performance on ESP32-S3 at 240MHz?
2. How much stack/heap for parsing buffers?
3. Should parser be blocking or async?
4. How to handle multi-line responses (like `$#` output)?

### Testing Plan
1. Collect real FluidNC message samples
2. Create test cases for all message types
3. Benchmark parsing performance
4. Test with intentionally corrupted messages

### Success Criteria
- [ ] Parses status reports at 4Hz (250ms interval)
- [ ] Extracts all fields correctly (state, position, overrides)
- [ ] Handles errors gracefully (no crashes on bad data)
- [ ] Low memory overhead (<1KB per parse)

---

## 4. Touch Calibration 🖱️ MEDIUM PRIORITY

### Why Research This?
Touch accuracy is critical for a CNC pendant. Need to verify GT911 calibration.

### Current Touch Setup
- **Controller**: GT911 I2C touch controller
- **I2C Address**: 0x5D
- **Pins**: SDA=19, SCL=20, RST=38
- **Screen**: 800x480 pixels
- **Driver**: TouchDriver class in `touch_driver.cpp`

### Questions to Answer
1. **Is GT911 factory-calibrated?**
   - Most GT911 modules come pre-calibrated for specific displays
   - CrowPanel likely has calibration burned into GT911 firmware
   - May not need software calibration

2. **How to verify accuracy?**
   - Create test screen with targets at corners and edges
   - Compare touch coordinates to expected positions
   - Measure deviation in pixels

3. **What if calibration is needed?**
   - GT911 supports 5-point calibration
   - Calibration data can be written to GT911 config registers
   - Need to understand GT911 register map

### Research Tasks
- **GT911 datasheet** - Configuration registers, calibration procedure
- **CrowPanel documentation** - Is touch pre-calibrated?
- **TouchDriver implementation** - Does it support calibration?
- **LVGL calibration** - Does LVGL need separate calibration?

### Test Screen Design
```
┌─────────────────────────┐
│ ×                     × │  Touch all 4 corners
│                         │  + center point
│           ×             │  Display deviation
│                         │  from target
│ ×                     × │
└─────────────────────────┘
```

### Implementation Plan
1. Create touch test tab/screen
2. Display targets and capture touch events
3. Calculate deviation (expected vs actual)
4. If deviation >10px, implement calibration
5. Store calibration matrix in Preferences

### Success Criteria
- [ ] Touch accuracy <5px deviation at all points
- [ ] Consistent accuracy across entire screen
- [ ] No dead zones at edges/corners
- [ ] Smooth diagonal gestures (no jitter)

---

## 5. Power Management & Display Backlight 🔋

### Why Research This?
Settings → General tab mentions:
- Screen brightness slider (10-100%)
- Screen timeout dropdown

Need to implement actual backlight control.

### Hardware Questions
1. **Which GPIO controls backlight?**
   - Need CrowPanel schematic or test all GPIO
   - Likely PWM-capable pin
   - May be inverted logic (LOW=bright)

2. **What voltage/current?**
   - Direct GPIO drive or requires transistor?
   - Current limit of GPIO output

3. **PWM frequency?**
   - Too low → visible flicker (<200Hz)
   - Recommended: 1-10kHz for LED backlight

### ESP32 LEDC Peripheral
```cpp
// Example backlight control
#define BACKLIGHT_PIN 45  // Example, need to verify
#define BACKLIGHT_CHANNEL 0
#define BACKLIGHT_FREQ 5000  // 5kHz
#define BACKLIGHT_RESOLUTION 8  // 8-bit (0-255)

void init_backlight() {
    ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION);
    ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CHANNEL);
    set_brightness(100);  // Full brightness
}

void set_brightness(int percent) {
    int duty = map(percent, 0, 100, 0, 255);
    ledcWrite(BACKLIGHT_CHANNEL, duty);
}
```

### Screen Timeout Implementation
```cpp
// Activity tracking
unsigned long last_touch_time = 0;
int screen_timeout_minutes = 5;

void on_touch_event() {
    last_touch_time = millis();
    if (screen_off) {
        wake_screen();
    }
}

void check_timeout() {
    if (millis() - last_touch_time > screen_timeout_minutes * 60000) {
        sleep_screen();
    }
}

void sleep_screen() {
    set_brightness(0);
    screen_off = true;
}

void wake_screen() {
    set_brightness(saved_brightness);
    screen_off = false;
}
```

### Research Tasks
- **CrowPanel schematic** - Identify backlight pin
- **GPIO testing** - Try common backlight pins (45, 46, etc.)
- **LEDC API** - ESP32 Arduino LEDC documentation
- **Power consumption** - Measure current at different brightness levels

### Success Criteria
- [ ] Brightness slider controls backlight smoothly
- [ ] No visible flicker at any brightness level
- [ ] Screen timeout works and wakes on touch
- [ ] Brightness setting persists across reboots

---

## 6. File System for G-code Storage 📁 MEDIUM PRIORITY

### Why Research This?
Potential features requiring file storage:
- **G-code files** - Upload and run from local storage
- **Macros** - Store frequently-used G-code snippets
- **Configuration backup** - Export/import settings
- **Tool library** - Tool dimensions and offsets

### Flash Partition Status
- **Total Flash**: 4MB
- **Current Usage**: 1.48MB (48%)
- **Available**: ~1.6MB
- **Potential for FS**: 500KB - 1MB

### File System Options

#### Option 1: SPIFFS (SPI Flash File System)
**Pros:**
- Simple API
- Built into ESP32 Arduino
- Good for small files
- Wear leveling

**Cons:**
- No directories (flat namespace)
- Slow on large files
- Being deprecated in favor of LittleFS

#### Option 2: LittleFS (Little File System)
**Pros:**
- Modern replacement for SPIFFS
- Directory support
- Better performance
- Power-fail safe
- Dynamic wear leveling

**Cons:**
- Slightly larger code footprint

#### Option 3: SD Card (External)
**Pros:**
- Large capacity (GB)
- Easy file transfer (remove card)
- No flash wear concerns

**Cons:**
- Requires SD card slot (does CrowPanel have one?)
- More complexity
- Mechanical failure point

### Partition Table Modification
```ini
# Current: default_4MB.csv or single_app_4MB.csv
# Custom partition table with file system:

# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000   # 20KB for settings
otadata,  data, ota,     0xe000,  0x2000   # 8KB (not used)
app0,     app,  ota_0,   0x10000, 0x2E0000 # ~3MB for firmware
spiffs,   data, spiffs,  0x2F0000,0x100000 # 1MB for files
```

### Use Cases

#### Scenario 1: Macro Storage
```
/macros/
  home_all.gcode
  probe_z.gcode
  tool_change.gcode
```

#### Scenario 2: Job Files
```
/jobs/
  part_001.nc
  toolpath.gcode
```

#### Scenario 3: Configuration
```
/config/
  settings.json
  tool_table.json
```

### API Example
```cpp
#include <LittleFS.h>

void init_filesystem() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }
    list_files("/");
}

void list_files(const char *dirname) {
    File root = LittleFS.open(dirname);
    File file = root.openNextFile();
    while (file) {
        Serial.println(file.name());
        file = root.openNextFile();
    }
}

String read_macro(const char *path) {
    File file = LittleFS.open(path, "r");
    String content = file.readString();
    file.close();
    return content;
}
```

### Key Questions
1. **Do we need file storage?** Or just execute from FluidNC's SD card?
2. **What files to store locally?** Macros only? Full G-code programs?
3. **File upload mechanism?** WebDAV? HTTP form? Serial transfer?
4. **File browser UI?** List view with scroll? Icon grid?

### Decision Matrix
| Feature | No FS | SPIFFS | LittleFS | SD Card |
|---------|-------|--------|----------|---------|
| Flash Usage | 0KB | 500KB | 500KB | 0KB |
| Complexity | Low | Medium | Medium | High |
| Capacity | N/A | 500KB | 500KB | 32GB+ |
| File Upload | N/A | WiFi | WiFi | Physical |
| Offline Use | ❌ | ✅ | ✅ | ✅ |

### Success Criteria (If Implemented)
- [ ] File system mounts reliably on boot
- [ ] Can store and retrieve macros
- [ ] UI for browsing/selecting files
- [ ] Doesn't impact firmware update process

---

## 7. Joystick/Virtual Joystick Widget 🕹️

### Why Research This?
`UITabControlJoystick` exists but is just a placeholder. Need to implement 2D joystick control.

### Virtual Joystick Design Options

#### Option 1: Arc-Based Joystick
- Use LVGL `lv_arc` widget as outer ring
- Draggable knob in center
- Returns X/Y position as percentage

#### Option 2: Custom Canvas Widget
- Draw joystick on `lv_canvas`
- Handle touch drag events manually
- More control over appearance

#### Option 3: Two Sliders (Simpler Alternative)
- Vertical slider for Y-axis
- Horizontal slider for X-axis
- Less intuitive but easier to implement

### Joystick Behavior

#### Continuous Jogging Mode
```cpp
// While finger is held on joystick
void joystick_drag_event(lv_event_t *e) {
    lv_point_t pos = get_joystick_position();
    
    // Convert position (-100 to +100) to jog velocity
    float x_velocity = map(pos.x, -100, 100, -jog_speed, jog_speed);
    float y_velocity = map(pos.y, -100, 100, -jog_speed, jog_speed);
    
    // Send continuous jog command
    String cmd = String("$J=G91 X") + x_velocity + " Y" + y_velocity + " F" + jog_feed_rate;
    send_fluidnc_command(cmd);
}

void joystick_release_event(lv_event_t *e) {
    // Stop jogging
    send_realtime_command(0x85);  // Jog cancel
}
```

#### Step Mode
```cpp
// Single tap in direction
void joystick_click_event(lv_event_t *e) {
    lv_point_t pos = get_joystick_position();
    
    // Determine direction (8 directions: N, NE, E, SE, S, SW, W, NW)
    float angle = atan2(pos.y, pos.x);
    
    // Execute single step jog
    String cmd = get_jog_command_for_direction(angle, step_distance);
    send_fluidnc_command(cmd);
}
```

### Visual Design
```
┌─────────────────────────┐
│                         │
│       Joystick          │
│      ╱────────╲         │
│     │   ┌─┐   │         │
│     │   │●│   │  ← Knob │
│     │   └─┘   │         │
│      ╲────────╱         │
│                         │
│  [Continuous] [Step]    │  ← Mode buttons
│  Speed: [====|===] 50%  │  ← Speed slider
└─────────────────────────┘
```

### Research Tasks
- **LVGL arc widget** - Can it be used as joystick outer ring?
- **Touch drag gestures** - Smooth tracking of finger movement
- **Return-to-center animation** - Spring-back effect when released
- **Dead zone** - Ignore small movements near center
- **Velocity curves** - Linear vs exponential response

### Key Questions
1. Should joystick return to center when released or stay where left?
2. Continuous jogging or discrete steps?
3. Support diagonal movements (X+Y simultaneous)?
4. Visual feedback for direction/speed?

### Success Criteria
- [ ] Smooth joystick control with finger drag
- [ ] Accurate mapping of position to jog commands
- [ ] Visual feedback (knob position updates in real-time)
- [ ] Mode switching (continuous vs step) works correctly

---

## 8. Macro System Design 💾

### Why Research This?
Macros tab exists but needs implementation. Macros are essential for repetitive CNC operations.

### FluidNC WebUI Macro Format
From the wiki, WebUI uses `macrocfg.json`:
```json
[
  {
    "name": "Home All",
    "glyph": "home",
    "filename": "/macro_home.g",
    "target": "ESP",
    "class": "btn-success",
    "index": 0
  },
  {
    "name": "Probe Z",
    "glyph": "star",
    "filename": "/macro_probe.g",
    "target": "ESP",
    "class": "btn-primary",
    "index": 1
  }
]
```

### FluidTouch Macro Requirements

#### Macro Definition
```cpp
struct Macro {
    String name;           // Display name
    String gcode;          // G-code content (multiline)
    String icon;           // Icon name or emoji
    uint32_t color;        // Button color
};
```

#### Storage Options

**Option 1: Preferences (Simple)**
```cpp
// Store up to 10 macros in Preferences
preferences.putString("macro_0_name", "Home All");
preferences.putString("macro_0_code", "$H");
preferences.putString("macro_1_name", "Probe Z");
preferences.putString("macro_1_code", "G38.2 Z-20 F80 P4.985");
```

**Option 2: LittleFS Files (Flexible)**
```
/macros/
  01_home_all.gcode
  02_probe_z.gcode
  03_tool_change.gcode
```

### UI Design

#### Grid Layout (Like WebUI)
```
┌──────┬──────┬──────┬──────┐
│ 🏠   │ ⭐   │ 🔧   │ ➕   │
│ Home │ Probe│Change│ Add  │
│ All  │  Z   │ Tool │ New  │
├──────┼──────┼──────┼──────┤
│ 📁   │ 🔄   │ ⏸️   │      │
│ Load │Reset │Pause │      │
│ Job  │ Zero │      │      │
└──────┴──────┴──────┴──────┘
```

#### List Layout (Alternative)
```
┌────────────────────────────┐
│ ▶ Home All Axes         ✏️ │
│ ▶ Probe Z with Plate    ✏️ │
│ ▶ Tool Change Sequence  ✏️ │
│ ▶ Reset Work Zero       ✏️ │
│ ➕ Add New Macro...        │
└────────────────────────────┘
```

### Macro Editor
Need UI for creating/editing macros:
```
┌────────────────────────────┐
│ Macro Name:                │
│ [Home All Axes_________]   │
│                            │
│ G-code:                    │
│ ┌────────────────────────┐ │
│ │ $H                     │ │
│ │ G53 G0 Z-1             │ │
│ │                        │ │
│ └────────────────────────┘ │
│                            │
│ Icon: [🏠 ▼]  Color: [🟢] │
│                            │
│ [Save]  [Test]  [Cancel]   │
└────────────────────────────┘
```

### Macro Execution
```cpp
void execute_macro(Macro &macro) {
    // Split macro into lines
    String lines[100];
    int line_count = split_gcode(macro.gcode, lines);
    
    // Queue commands
    for (int i = 0; i < line_count; i++) {
        send_command(lines[i]);
        wait_for_ok();  // Block until FluidNC confirms
    }
}
```

### Variable Substitution (Advanced)
Allow macros to use variables:
```gcode
; Probe Z with plate
G38.2 Z-20 F80 P{PLATE_THICKNESS}
```

Replace `{PLATE_THICKNESS}` with value from settings before execution.

### Research Tasks
- **Macro storage format** - Preferences vs files?
- **Macro execution** - Synchronous vs async?
- **Error handling** - What if macro fails mid-execution?
- **Macro sharing** - Import/export via QR code or file?

### Success Criteria
- [ ] Can create, edit, delete macros via UI
- [ ] Macros persist across reboots
- [ ] Execute macros with single button press
- [ ] Handle errors gracefully (pause, not crash)

---

## 9. Real-Time Plotting/Visualization 📊 LOW PRIORITY

### Why Research This?
Visual feedback during machining is valuable:
- Tool path preview
- Current position indicator
- Bounding box display
- Progress visualization

### Feasibility Analysis

#### Performance Constraints
- **CPU**: ESP32-S3 @ 240MHz (single core available, other for WiFi)
- **RAM**: 8MB PSRAM + 512KB SRAM
- **Display**: 800×480 @ 60fps = 23 million pixels/sec
- **LVGL overhead**: ~30% CPU for UI updates

#### Visualization Options

**Option 1: 2D Path Preview (Simplest)**
- Parse G-code to extract XY coordinates
- Draw lines on LVGL canvas
- Show current position as moving dot

**Option 2: Bounding Box Only**
- Calculate min/max X, Y, Z from G-code
- Display as rectangle overlay
- Much lighter than full path

**Option 3: 3D Wireframe (Too Heavy?)**
- Full 3D projection math
- Likely too slow for real-time on ESP32

### Example: Simple 2D Plot
```cpp
// G-code preprocessing
struct GcodeBounds {
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;
};

GcodeBounds calculate_bounds(String gcode) {
    // Parse G-code, track min/max for each axis
    // Return bounding box
}

void draw_toolpath(lv_obj_t *canvas, String gcode) {
    lv_canvas_fill_bg(canvas, UITheme::BG_MEDIUM, LV_OPA_COVER);
    
    // Scale factor to fit 800x480
    float scale = calculate_scale(bounds, 800, 480);
    
    // Draw each line segment
    for (each G1/G2/G3 command) {
        lv_point_t p1 = scale_point(start, scale);
        lv_point_t p2 = scale_point(end, scale);
        lv_canvas_draw_line(canvas, p1, p2, &line_style);
    }
}

void update_current_position(float x, float y) {
    lv_point_t pos = scale_point(x, y, scale);
    lv_canvas_draw_circle(canvas, pos.x, pos.y, 5, &pos_style);
}
```

### Memory Estimation
- **G-code file**: 100KB typical
- **Parsed segments**: ~5000 lines × 12 bytes = 60KB
- **Canvas buffer**: 800×480×2 bytes = 768KB (in PSRAM)
- **Total**: ~830KB (feasible)

### Performance Testing Needed
1. Benchmark G-code parsing speed
2. Measure canvas drawing frame rate
3. Test with real G-code files (100-1000 lines)
4. Evaluate impact on UI responsiveness

### Key Questions
1. Parse G-code on ESP32 or pre-parse on PC?
2. Real-time updates or static preview?
3. Worth the complexity vs just showing coordinates?
4. Can PSRAM be used for canvas buffer?

### Success Criteria (If Implemented)
- [ ] Parses G-code in <2 seconds
- [ ] Renders toolpath without blocking UI
- [ ] Updates current position at 4Hz
- [ ] Uses <1MB RAM total

### Recommendation
**Start with bounding box only**. If that works well and users want more, add full toolpath preview in later version.

---

## 10. Error Recovery & State Management 🚨

### Why Research This?
CNC controllers need robust error handling to prevent crashes and damage.

### Error Scenarios

#### Connection Errors
- WiFi disconnected during job
- WebSocket timeout
- FluidNC rebooted
- Network congestion

#### Machine Errors
- Alarm state (limit switch hit)
- Position lost (stepper skip)
- Probe failure
- Feed hold timeout

#### User Errors
- Invalid jog command
- Zero feed rate
- Out of soft limits
- Emergency stop during homing

### State Machine Design

```
┌─────────────────────────────────────────────────┐
│                                                 │
│  DISCONNECTED ──connect──→ CONNECTING           │
│       ↑                          │              │
│       │                     successful          │
│   timeout/error                  ↓              │
│       │                      CONNECTED          │
│       └──────────────────────────┘              │
│                                  │              │
│                            receive "Idle"       │
│                                  ↓              │
│                               IDLE              │
│                                  │              │
│                    ┌─────────────┼─────────────┐
│                    │             │             │
│               jog command   G-code run    alarm │
│                    ↓             ↓             ↓
│                 JOGGING       RUNNING      ALARM│
│                    │             │             │
│               jog cancel    hold/done    unlock │
│                    │             │             │
│                    └─────────────┴─────────────┘
│                                  │              │
│                                IDLE             │
│                                                 │
└─────────────────────────────────────────────────┘
```

### Connection Recovery
```cpp
class FluidNCConnection {
    enum State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    };
    
    State state = DISCONNECTED;
    int reconnect_attempts = 0;
    const int MAX_RECONNECT = 5;
    
    void handle_disconnect() {
        state = DISCONNECTED;
        if (reconnect_attempts < MAX_RECONNECT) {
            reconnect_attempts++;
            delay(1000 * reconnect_attempts);  // Exponential backoff
            attempt_reconnect();
        } else {
            show_error_dialog("Connection lost. Check WiFi.");
        }
    }
    
    void on_connect() {
        reconnect_attempts = 0;
        state = CONNECTED;
        query_initial_state();
    }
};
```

### Machine State Tracking
```cpp
class MachineState {
    String current_state = "Unknown";
    String previous_state = "Unknown";
    
    void update_state(String new_state) {
        if (new_state != current_state) {
            on_state_change(current_state, new_state);
            previous_state = current_state;
            current_state = new_state;
        }
    }
    
    void on_state_change(String from, String to) {
        if (to == "Alarm") {
            handle_alarm_state();
        } else if (from == "Run" && to == "Idle") {
            handle_job_complete();
        }
    }
};
```

### User Notification System
```cpp
// LVGL message box for errors
void show_error_dialog(String message) {
    lv_obj_t *mbox = lv_msgbox_create(lv_screen_active(), 
                                      "Error", 
                                      message.c_str(), 
                                      {"OK", NULL}, 
                                      false);
    lv_obj_set_style_bg_color(mbox, UITheme::STATE_ALARM, 0);
}

// Status bar notification
void show_warning(String message) {
    lv_label_set_text(status_bar_label, message.c_str());
    lv_obj_set_style_text_color(status_bar_label, UITheme::UI_WARNING, 0);
    
    // Auto-clear after 5 seconds
    lv_timer_create([](lv_timer_t *t) {
        lv_label_set_text(status_bar_label, "");
    }, 5000, NULL);
}
```

### Command Timeout Detection
```cpp
class CommandQueue {
    unsigned long last_command_time = 0;
    const unsigned long COMMAND_TIMEOUT = 5000;  // 5 seconds
    
    void check_timeout() {
        if (pending_commands.size() > 0) {
            if (millis() - last_command_time > COMMAND_TIMEOUT) {
                handle_command_timeout();
            }
        }
    }
    
    void handle_command_timeout() {
        // Clear queue, show error, reset connection
        pending_commands.clear();
        show_error_dialog("FluidNC not responding");
        attempt_reconnect();
    }
};
```

### Research Tasks
- **State machine patterns** - Finite state machine best practices
- **Error code mapping** - All FluidNC error codes and meanings
- **Recovery strategies** - When to auto-retry vs ask user
- **Watchdog integration** - ESP32 hardware watchdog for freezes

### Success Criteria
- [ ] Handles WiFi disconnect gracefully (auto-reconnect)
- [ ] Detects and reports alarm states
- [ ] Recovers from command timeouts
- [ ] Clear user feedback for all error states
- [ ] No crashes or infinite loops on errors

---

## 11. Settings Persistence Strategy 💾

### Why Research This?
Multiple tabs with settings that need to persist across reboots.

### Current Approach: ESP32 Preferences
```cpp
#include <Preferences.h>

Preferences preferences;

// Writing
preferences.begin("fluidtouch", false);  // Read-write
preferences.putString("wifi_ssid", "MyNetwork");
preferences.putInt("brightness", 75);
preferences.end();

// Reading
preferences.begin("fluidtouch", true);  // Read-only
String ssid = preferences.getString("wifi_ssid", "");
int brightness = preferences.getInt("brightness", 100);
preferences.end();
```

**Pros:**
- Simple key-value API
- Built into ESP32
- Fast random access
- Wear leveling handled automatically

**Cons:**
- Flat namespace (no hierarchy)
- 4KB storage limit per namespace
- No import/export
- No versioning

### Current Settings Inventory

#### WiFi Settings
- `wifi_ssid` (String)
- `wifi_pass` (String)

#### FluidNC Settings
- `fluidnc_ip` (String)
- `ws_port` (String)

#### General Settings (Placeholders)
- `brightness` (int, 0-100)
- `screensaver` (int, timeout index)
- `units` (int, 0=metric, 1=imperial)

### Alternative: JSON Configuration File
```cpp
#include <ArduinoJson.h>
#include <LittleFS.h>

struct Config {
    // WiFi
    String wifi_ssid;
    String wifi_pass;
    
    // FluidNC
    String fluidnc_ip;
    int ws_port;
    
    // Display
    int brightness;
    int timeout_minutes;
    
    // Units
    bool metric;  // true=mm, false=inches
};

void save_config(Config &cfg) {
    StaticJsonDocument<1024> doc;
    doc["wifi"]["ssid"] = cfg.wifi_ssid;
    doc["wifi"]["pass"] = cfg.wifi_pass;
    doc["fluidnc"]["ip"] = cfg.fluidnc_ip;
    doc["fluidnc"]["port"] = cfg.ws_port;
    doc["display"]["brightness"] = cfg.brightness;
    doc["display"]["timeout"] = cfg.timeout_minutes;
    doc["units"]["metric"] = cfg.metric;
    
    File file = LittleFS.open("/config.json", "w");
    serializeJson(doc, file);
    file.close();
}

Config load_config() {
    File file = LittleFS.open("/config.json", "r");
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, file);
    file.close();
    
    Config cfg;
    cfg.wifi_ssid = doc["wifi"]["ssid"].as<String>();
    cfg.wifi_pass = doc["wifi"]["pass"].as<String>();
    // ... load remaining fields
    return cfg;
}
```

**Pros:**
- Hierarchical structure
- Human-readable
- Easy import/export
- Can include metadata (version, timestamp)

**Cons:**
- Requires file system (LittleFS)
- Slower than Preferences
- More complex code

### Configuration Migration
```cpp
const int CONFIG_VERSION = 2;

void migrate_config() {
    preferences.begin("fluidtouch", false);
    int version = preferences.getInt("cfg_version", 0);
    
    if (version < 2) {
        // Migrate from v1 to v2
        // ... add new settings with defaults
        preferences.putInt("cfg_version", 2);
    }
    
    preferences.end();
}
```

### Factory Reset
```cpp
void factory_reset() {
    // Confirm with user first
    lv_obj_t *mbox = lv_msgbox_create(...);
    
    // If confirmed:
    preferences.begin("fluidtouch", false);
    preferences.clear();  // Erase all keys
    preferences.end();
    
    ESP.restart();  // Reboot to apply defaults
}
```

### Settings Import/Export

#### QR Code Export
```cpp
void export_settings_qr() {
    String json = serialize_config();
    generate_qr_code(json);  // Display QR code on screen
    // User scans with phone, saves JSON file
}
```

#### WiFi AP for Transfer
```cpp
void export_via_ap() {
    // Start temporary AP
    WiFi.softAP("FluidTouch-Config");
    
    // Serve HTTP download
    server.on("/config.json", []() {
        server.send(200, "application/json", serialize_config());
    });
}
```

### Research Tasks
- **Preferences API limits** - Maximum key length, value size
- **JSON library comparison** - ArduinoJson vs others
- **Configuration validation** - Detect corrupt settings
- **Secure storage** - Encrypt WiFi passwords?

### Key Questions
1. Will settings fit in 4KB Preferences limit?
2. Need hierarchical config or flat namespace OK?
3. Should WiFi password be encrypted in flash?
4. Import/export feature valuable enough to implement?

### Recommendation
- **Phase 1**: Use Preferences (simple, fast, built-in)
- **Phase 2**: If hit 4KB limit or need export, migrate to JSON + LittleFS

### Success Criteria
- [ ] All settings persist across reboots
- [ ] Factory reset clears everything
- [ ] Configuration migration works smoothly
- [ ] No corruption from power loss during save

---

## 12. Soft Limits & Safety Features ⚠️

### Why Research This?
CNC machines can crash into limits, break tools, or cause injury. FluidTouch should help prevent accidents.

### FluidNC Soft Limits
FluidNC has built-in soft limits that can be enabled:
```
$20=1    # Soft limits enabled
$21=1    # Hard limits enabled
```

When enabled, FluidNC will reject moves beyond max travel defined in config.

### FluidTouch Safety Features

#### 1. Pre-Check Jog Commands
```cpp
bool is_jog_safe(float x_delta, float y_delta, float z_delta) {
    // Get current MPos from last status report
    float current_x = machine_status.mpos[0];
    float current_y = machine_status.mpos[1];
    float current_z = machine_status.mpos[2];
    
    // Calculate target position
    float target_x = current_x + x_delta;
    float target_y = current_y + y_delta;
    float target_z = current_z + z_delta;
    
    // Check against soft limits (get from FluidNC config)
    if (target_x < soft_limit_min_x || target_x > soft_limit_max_x) {
        show_warning("Jog would exceed X limit");
        return false;
    }
    
    // ... check Y and Z
    
    return true;
}
```

#### 2. Emergency Stop Button
```
┌────────────────────────────┐
│                            │
│      [   E-STOP   ]        │  ← Large, red, always visible
│         🛑                 │
│                            │
└────────────────────────────┘
```

```cpp
void create_estop_button() {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 200, 80);
    lv_obj_set_style_bg_color(btn, UITheme::BTN_ESTOP, 0);
    lv_obj_add_event_cb(btn, estop_event, LV_EVENT_CLICKED, NULL);
}

void estop_event(lv_event_t *e) {
    // Send soft reset to FluidNC
    send_realtime_command(0x18);  // Ctrl-X
    
    // Show alert
    show_error_dialog("EMERGENCY STOP ACTIVATED\nPress UNLOCK to resume");
}
```

#### 3. Confirmation Dialogs
For dangerous operations:
- Homing cycle
- Probing
- Spindle start at high speed
- Work coordinate zeroing

```cpp
void confirm_home_cycle() {
    lv_obj_t *mbox = lv_msgbox_create(lv_screen_active(),
        "Confirm Homing",
        "Machine will move to limits. Clear workspace?",
        {"Proceed", "Cancel"},
        false);
    lv_obj_add_event_cb(mbox, confirm_home_callback, LV_EVENT_VALUE_CHANGED, NULL);
}
```

#### 4. Spindle Speed Warning
```cpp
void set_spindle_speed(int rpm) {
    if (rpm > 12000) {
        show_warning("High spindle speed! Check workholding.");
        delay(2000);  // Force user to see warning
    }
    String cmd = "S" + String(rpm);
    send_command(cmd);
}
```

#### 5. Probe Failure Detection
```cpp
void probe_z() {
    // Check probe is NOT triggered before probing
    String status = query_probe_status();
    if (status.indexOf(":1") > 0) {  // Probe already triggered
        show_error_dialog("Probe is already triggered!\nCheck probe connection.");
        return;
    }
    
    // Execute probe command
    send_command("G38.2 Z-20 F80 P4.985");
    
    // Wait for completion and check result
    // ...
}
```

#### 6. Position Lost Warning
```cpp
void check_position_validity() {
    if (machine_status.state == "Alarm") {
        show_warning("⚠️ Position may be lost. Home machine before use.");
    }
}
```

#### 7. Feed Override Limits
```cpp
void set_feed_override(int percent) {
    // Limit to safe range
    if (percent < 10) percent = 10;   // Min 10%
    if (percent > 200) percent = 200; // Max 200%
    
    // Send override commands
    send_feed_override_realtime(percent);
}
```

### Safety Checklist on Startup
```
┌────────────────────────────┐
│  Pre-Operation Checklist   │
│  ────────────────────────  │
│  ☐ Workpiece secured       │
│  ☐ Tool properly installed │
│  ☐ Work area clear         │
│  ☐ Emergency stop tested   │
│  ☐ Soft limits enabled     │
│                            │
│  [ Skip ] [ Confirm ]      │
└────────────────────────────┘
```

### Research Tasks
- **FluidNC alarm codes** - What each alarm means
- **Probe safety** - Best practices for touch probe usage
- **Spindle safety** - Recommended RPM limits by material
- **Interlock inputs** - Can FluidNC report door/cover status?

### Key Questions
1. Should FluidTouch enforce stricter limits than FluidNC?
2. How to handle user ignoring safety warnings?
3. Require PIN for safety-critical operations?
4. Log safety events for troubleshooting?

### Success Criteria
- [ ] Emergency stop accessible from all screens
- [ ] Confirmation required for destructive operations
- [ ] Soft limit violations prevented before sending command
- [ ] Clear visual indicators for unsafe states

---

## 13. Multi-Language Support 🌍 LOW PRIORITY

### Why Research This?
FluidTouch could be used internationally. LVGL has built-in i18n support.

### LVGL Internationalization

#### String Externalization
```cpp
// Instead of hardcoded strings:
lv_label_set_text(label, "Home All Axes");

// Use translation keys:
lv_label_set_text(label, _(LV_I18N_HOME_ALL_AXES));
```

#### Translation Files
```cpp
// en_US.h
#define LV_I18N_HOME_ALL_AXES "Home All Axes"
#define LV_I18N_JOG_SPEED     "Jog Speed"
#define LV_I18N_EMERGENCY_STOP "EMERGENCY STOP"

// es_ES.h
#define LV_I18N_HOME_ALL_AXES "Inicio de todos los ejes"
#define LV_I18N_JOG_SPEED     "Velocidad de avance"
#define LV_I18N_EMERGENCY_STOP "PARADA DE EMERGENCIA"
```

#### Runtime Language Selection
```cpp
void set_language(String lang) {
    if (lang == "en") {
        current_language = LANG_EN;
    } else if (lang == "es") {
        current_language = LANG_ES;
    }
    // ... more languages
    
    refresh_all_ui();  // Reload strings
}
```

### Flash Space Considerations
- **English**: ~5KB of strings
- **Per additional language**: ~5KB
- **5 languages**: 25KB total
- **Conclusion**: Affordable, but adds up

### Languages to Consider
1. **English** (default) - EN
2. **Spanish** - ES
3. **German** - DE
4. **French** - FR
5. **Chinese** - ZH
6. **Japanese** - JA

### Research Tasks
- **LVGL i18n API** - How to implement multi-language
- **Font support** - Unicode fonts for non-Latin scripts
- **String length** - Some languages longer (German) than English
- **RTL languages** - Right-to-left support (Arabic, Hebrew)

### Key Questions
1. Is multi-language worth the complexity for v1.0?
2. Can UI accommodate longer translated strings?
3. Who would translate the strings?
4. Need Unicode font or ASCII sufficient?

### Recommendation
**Defer to v2.0**. Focus on English for initial release. Design code with i18n in mind (use constants for strings), but don't implement translation infrastructure yet.

### Success Criteria (If Implemented)
- [ ] Language selection in Settings
- [ ] All UI strings translated
- [ ] Fonts support target languages
- [ ] UI layout accommodates longer strings

---

## 14. OTA Alternative: USB Bootloader 🔄

### Why Research This?
OTA firmware updates ruled out due to flash constraints. Need alternative update mechanism.

### Current Update Method
1. User connects USB cable
2. Computer recognizes ESP32-S3 as COM port
3. Upload via PlatformIO or esptool.py
4. Device reboots with new firmware

**Problem**: Requires PlatformIO/Arduino IDE knowledge, intimidating for non-developers.

### ESP32-S3 Boot Options

#### Native USB Support
ESP32-S3 has native USB (not just USB-to-serial like ESP32):
- USB-OTG peripheral
- Can act as USB device or host
- DFU (Device Firmware Update) mode possible

#### DFU Mode (Device Firmware Update)
```cpp
// Enter DFU mode from application
#include <USB.h>

void enter_dfu_mode() {
    USB.enableDFU();  // If supported
    ESP.restart();
}
```

User workflow:
1. Settings → System → "Enter Update Mode"
2. Device reboots into DFU mode
3. PC sees "ESP32-S3 DFU Device"
4. User drags firmware.bin to device (like USB drive?)

#### Bootloader Button Method (Current)
- Hold BOOT button (GPIO0) while powering on
- ESP32 enters ROM bootloader mode
- Can upload via esptool.py

### Alternative: Web-Based Flasher

#### ESP Web Tools
- https://esphome.github.io/esp-web-tools/
- Web page that uses WebUSB API to flash ESP32
- User connects USB, clicks "Install"
- Firmware downloads from GitHub and flashes

**Workflow:**
1. User goes to https://fluidtouch.github.io/install
2. Connects USB cable
3. Clicks "Install Latest Version"
4. Web page flashes firmware directly from browser
5. No software installation required

**Limitations:**
- Chrome/Edge only (WebUSB support)
- Requires HTTPS hosting
- User must still connect USB cable

### SD Card Bootloader (Advanced)
If CrowPanel has SD card slot:
1. User downloads `firmware.bin` from GitHub
2. Copies to SD card
3. Inserts SD card into CrowPanel
4. Settings → System → "Update from SD"
5. Custom bootloader reads firmware.bin and flashes

**Problem**: Requires implementing custom bootloader, significant complexity.

### Version Display
Regardless of update method, show version info:
```
┌────────────────────────────┐
│  System Information        │
│  ────────────────────────  │
│  FluidTouch Version: 0.2.1 │
│  Build Date: 2025-10-22    │
│  LVGL Version: 9.3.0       │
│  LovyanGFX: 1.2.7          │
│                            │
│  [Check for Updates]       │
└────────────────────────────┘
```

### Research Tasks
- **ESP32-S3 DFU support** - Can it enter DFU mode programmatically?
- **Web flasher compatibility** - Does CrowPanel work with ESP Web Tools?
- **SD card slot** - Does CrowPanel have one? (Check schematic)
- **Custom bootloader** - Feasibility of SD card update

### Key Questions
1. Is DFU mode supported on ESP32-S3 Arduino framework?
2. Would users prefer web flasher or traditional USB method?
3. Can we simplify current method with better documentation?
4. Is update frequency high enough to warrant complex solution?

### Recommendation
1. **Short-term**: Document current USB update process clearly (step-by-step with screenshots)
2. **Medium-term**: Create web flasher page using ESP Web Tools
3. **Long-term**: If user demand is high, investigate custom bootloader

### Success Criteria
- [ ] Clear update instructions in documentation
- [ ] Version number displayed in UI
- [ ] Update process works reliably
- [ ] Minimal risk of bricking device

---

## Recommended Research Priority

### Immediate (Next 1-2 Weeks)
1. ✅ **ESP32 WebSocket Library** - Choose library for FluidNC connection
2. ✅ **LVGL + FreeRTOS Threading** - Foundation for multi-tasking
3. ✅ **G-code Parser** - Parse FluidNC status reports

### Short-term (This Month)
4. ✅ **Touch Calibration** - Verify accuracy, calibrate if needed
5. ✅ **Display Backlight** - Implement brightness control

### Medium-term (Next 1-3 Months)
6. ✅ **File System Decision** - SPIFFS/LittleFS for macros
7. ✅ **Joystick Widget** - Virtual joystick for continuous jogging
8. ✅ **Macro System** - Storage and execution framework
9. ✅ **Error Recovery** - State machine and reconnection logic

### Long-term (Future Versions)
10. ⏰ **Real-Time Visualization** - If performance allows
11. ⏰ **Multi-Language Support** - v2.0 feature
12. ⏰ **Web Flasher** - Simplify firmware updates
13. ⏰ **Advanced Safety** - PIN protection, operation logging

### Continuous
- **Settings Persistence** - Evolves as features are added
- **Safety Features** - Implement incrementally with each control feature

---

## Next Actions

### Week 1: Foundation Research
- [ ] Test 3 WebSocket libraries with benchmark sketch
- [ ] Research FreeRTOS task patterns for LVGL
- [ ] Design G-code parser architecture

### Week 2: Prototyping
- [ ] Implement WebSocket connection to test FluidNC instance
- [ ] Build G-code parser with test cases
- [ ] Test touch calibration accuracy

### Week 3: Integration
- [ ] Integrate WebSocket into FluidTouch
- [ ] Add status query loop
- [ ] Update Status tab with live data

### Week 4: Control Features
- [ ] Implement jog panel with tested jogging
- [ ] Add emergency stop
- [ ] Test state transitions

---

## Research Documentation

As you research each topic, document findings in:
- `docs/research/` folder
- One markdown file per major topic
- Include code examples, benchmark results, decisions made
- Update this document with conclusions

### Template for Research Documents
```markdown
# [Topic Name]

## Research Question
What are we trying to learn?

## Options Evaluated
1. Option A - pros/cons
2. Option B - pros/cons

## Testing Results
Benchmark data, measurements, observations

## Decision
Which option chosen and why?

## Implementation Notes
Key points for implementation

## References
Links to documentation, forum posts, examples
```

---

## Conclusion

This research roadmap provides a structured approach to completing FluidTouch. Prioritize items marked as HIGH PRIORITY and CRITICAL first, as they form the foundation for all other features.

**Remember**: Perfect is the enemy of good. Start with minimum viable implementations and iterate based on real-world usage.

**Next Step**: Begin with WebSocket library selection and LVGL keyboard fixes, as these are blocking current development.
