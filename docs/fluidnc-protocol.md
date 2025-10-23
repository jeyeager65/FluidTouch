# FluidNC Communication Protocol Reference

## Overview

This document details the FluidNC communication protocol for implementing FluidTouch connectivity. FluidNC is Grbl-compatible CNC firmware for ESP32 boards that supports multiple communication interfaces.

**Official Wiki**: http://wiki.fluidnc.com/

## Communication Interfaces

FluidNC supports three main communication methods:

### 1. USB Serial (UART0)
- **Default baud rate**: 115200
- **Supported rates**: 9600, 19200, 38400, 57600, 115200, 230400, 460800
- **Protocol**: Standard Grbl line-based commands
- **Use case**: Direct wired connection, programming, debugging

### 2. Telnet (TCP)
- **Default port**: 23 (configurable via `$Telnet/Port`)
- **Protocol**: Same as serial - line-based Grbl commands
- **Enable setting**: `$Telnet/Enable=True`
- **Connection**: Raw TCP socket to FluidNC IP
- **Use case**: Simple network control, scripting

### 3. WebSocket
- **Default port**: 81 (HTTP port + 1, configurable via `$HTTP/Port`)
- **Protocol**: Real-time bidirectional WebSocket with Grbl commands
- **Enable setting**: `$HTTP/Enable=True`
- **URL format**: `ws://[FluidNC-IP]:81`
- **Use case**: **Recommended for FluidTouch** - modern, reliable, WebUI-compatible

## WebSocket Protocol Details

### Connection Establishment

```python
# Example WebSocket connection
import websocket
ws = websocket.WebSocket()
ws.connect("ws://192.168.1.100:81")
```

**FluidTouch implementation notes:**
- Use ESP32 WebSocket client library
- Connect to `ws://[IP]:[port]` from Settings
- Handle connection failures gracefully
- Implement auto-reconnect on disconnect

### Message Format

**Command Structure:**
- **Realtime commands**: Single character, no line terminator (e.g., `?`, `~`, `!`)
- **Line commands**: String terminated with `\n` (e.g., `G0 X10\n`, `$G\n`)

**Response Format:**
- Multiple lines possible per command
- Lines end with `\n`
- Parse each line separately
- Broadcast messages sent to all connected clients

### Threading Requirements

Reception **must** be in separate thread - responses are asynchronous:
```cpp
// Conceptual implementation
void websocket_receive_task() {
    while (connected) {
        String response = ws.receive();
        parse_response(response);
    }
}
```

## Grbl Protocol Compatibility

FluidNC follows the [Grbl v1.1 protocol](https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface) with extensions.

### Key Protocol Rules

**CRITICAL**: You cannot blindly send commands. Must implement proper flow control:

1. **Command Buffer Management**
   - FluidNC has limited command buffer
   - Wait for `ok` before sending next line command
   - Track pending commands
   - Handle `error:N` responses

2. **Response Types**
   - `ok` - Command accepted and completed
   - `error:N` - Command failed (see error codes)
   - `[MSG:...]` - Informational messages
   - `<...>` - Status reports (from `?` command)
   - Startup messages on connect

## Command Categories

### Real-Time Commands (No Buffer, Immediate)

These are single-character commands sent without line terminator:

| Command | Description | Use Case |
|---------|-------------|----------|
| `?` | Status query | Get current position, state, feed rate |
| `~` | Cycle start/resume | Start job or resume from feed hold |
| `!` | Feed hold | Pause motion immediately |
| `0x18` (Ctrl-X) | Soft reset | Emergency stop, clear all |
| `0x84` | Door/Safety | Safety door open |
| `0x85` | Jog cancel | Cancel current jog command |

**FluidTouch usage:**
- Send `?` every 250-500ms for status updates
- Use `!` for pause/hold button
- Use `~` for resume button
- Use `0x18` for emergency stop

### Status Reports

Send `?` (no line terminator) to get status:

**Response format:**
```
<Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>
```

**Fields:**
- **State**: `Idle`, `Run`, `Hold`, `Jog`, `Alarm`, `Door`, `Check`, `Home`, `Sleep`
- **MPos**: Machine position (X, Y, Z, A, B, C)
- **WPos**: Work position (alternative to MPos)
- **FS**: Feed rate (mm/min), Spindle speed (RPM)
- **Ov**: Overrides - Feed%, Rapids%, Spindle% (all 0-200)

**Configure reporting with `$10`:**
- `$10=0` - WPos only
- `$10=1` - MPos only
- `$10=2` - Both WPos and MPos

### System Commands

Configuration and status queries (send with `\n`):

| Command | Description | Response |
|---------|-------------|----------|
| `$$` | View Grbl settings | List of `$number` settings |
| `$#` | View coordinate offsets | G54-G59, G28, G30, G92, TLO |
| `$G` | View parser state | Active G-codes and M-codes |
| `$I` | View build info | Version, build date |
| `$N` | View startup blocks | Auto-execute lines |
| `$X` | Unlock (kill alarm) | Clear alarm state |
| `$H` | Run homing cycle | Home all axes |
| `$C` | Check mode toggle | Simulate without motion |

**FluidTouch usage:**
- Send `$I` on connect to verify FluidNC version
- Use `$X` to clear alarms
- Use `$H` for home button
- Query `$#` to display work offsets

### FluidNC-Specific Commands

FluidNC extends Grbl with `$/` tree-structured commands:

```
$/axes/x        - Query X axis config
$/spindle       - Query spindle config  
$Wifi/ListAPs   - List WiFi networks
$Sta/SSID       - View WiFi SSID
```

**Configuration (via `$` commands):**
- `$Sta/SSID=MyNetwork` - Set WiFi SSID
- `$Sta/Password=mypass` - Set WiFi password
- `$HTTP/Port=80` - Set HTTP port
- `$Telnet/Enable=True` - Enable telnet

### Motion Commands (G-codes)

All motion commands require `\n` terminator and must wait for `ok`:

#### Rapid Positioning
```
G0 X10 Y20 Z5    ; Rapid move to position
```

#### Linear Motion
```
G1 F500 X10      ; Move to X10 at 500mm/min feed rate
```

#### Jogging (v1.1+)
```
$J=G91 X10 F1000   ; Jog +10mm in X at 1000mm/min
$J=G91 Y-5 F500    ; Jog -5mm in Y at 500mm/min
```

**Important**: Jog commands are special:
- Use `$J=` prefix
- Motion is immediate (like realtime)
- Cancel with `0x85` character
- Perfect for FluidTouch jog panel

#### Coordinate Systems
```
G54              ; Select work coordinate system 1
G55              ; Select work coordinate system 2
G10 L20 P0 X0 Y0 Z0  ; Zero current work position
```

#### Homing
```
$H               ; Home all axes (system command)
```

### Spindle/Laser Control

```
M3 S12000        ; Spindle CW at 12000 RPM (or laser power)
M4 S5000         ; Spindle CCW at 5000 RPM (laser dynamic mode)
M5               ; Spindle/laser off
```

### Probing

```
G38.2 Z-20 F80   ; Probe toward workpiece, stop on contact
G38.2 Z-20 F80 P4.985  ; Probe and set Z to plate thickness
```

**FluidTouch Probe Panel:**
- Use G38.2/G38.3 for touch probe operations
- `P` parameter sets Z work offset (plate thickness)
- Feed rate typically 80-150mm/min
- Parse probe result from status/response

## Work Coordinates vs Machine Coordinates

FluidNC tracks two coordinate systems:

### Machine Coordinates (MPos)
- Absolute position from machine home (0,0,0)
- Set by homing cycle (`$H`)
- Never changes with work offsets
- Used for `G53` absolute moves

### Work Coordinates (WPos)
- Position relative to current work zero (G54-G59)
- Set by user with `G10 L20` or touch-off
- Affected by G92 offsets
- Used for part programs

**Relationship:**
```
WPos = MPos - WorkOffset - G92Offset - ToolLengthOffset
```

**FluidTouch Display:**
Show both MPos and WPos on Status tab (parse from `?` response).

## Error Handling

### Error Response Format
```
error:N
```

### Common Error Codes
| Code | Description | Solution |
|------|-------------|----------|
| 1 | G-code Command Letter Not Found | Check command syntax |
| 2 | G-code Command Value Invalid | Check numeric values |
| 3 | Grbl Not Idle (when required) | Wait for Idle state |
| 5 | Setting Disabled | Feature not compiled in |
| 9 | G-code Locked Out | Unlock with `$X` |
| 20 | Unsupported Command | FluidNC doesn't support it |
| 22 | Feed Rate Not Set | Send F value first |
| 33 | Invalid Target | Arc calculation failed |

**FluidTouch error handling:**
```cpp
if (response.startsWith("error:")) {
    int error_code = parse_error_code(response);
    display_error_message(error_code);
    // Query error text: send "$e=N\n"
}
```

## Machine States

FluidNC reports current state in status reports:

| State | Description | Actions Available |
|-------|-------------|-------------------|
| `Idle` | Ready for commands | All commands valid |
| `Run` | Executing G-code | Hold (!), Stop (0x18), Status (?) |
| `Hold` | Motion paused | Resume (~), Stop (0x18) |
| `Jog` | Jogging | Cancel (0x85), Stop (0x18) |
| `Alarm` | Error state, locked | Unlock ($X), Home ($H) |
| `Door` | Safety door open | Close door to resume |
| `Check` | Check mode active | All motion simulated |
| `Home` | Homing in progress | Wait or Stop (0x18) |
| `Sleep` | Low power mode | Send any char to wake |

**State Machine for FluidTouch:**
```
Startup → Alarm → (Unlock or Home) → Idle ⇄ Run ⇄ Hold
                                      ↓
                                     Jog
```

## Feed Rate Overrides

Real-time override commands (since Grbl v1.1):

### Feed Rate
- `0x90` - Set 100%
- `0x91` - Increase 10%
- `0x92` - Decrease 10%
- `0x93` - Increase 1%
- `0x94` - Decrease 1%

### Rapids Override
- `0x95` - Set 100%
- `0x96` - Set 50%
- `0x97` - Set 25%

### Spindle Override
- `0x99` - Set 100%
- `0x9A` - Increase 10%
- `0x9B` - Decrease 10%
- `0x9C` - Increase 1%
- `0x9D` - Decrease 1%

### Spindle Stop
- `0x9E` - Toggle spindle stop

**FluidTouch Overrides Panel:**
Use these for feed rate sliders/buttons. Current values reported in status: `|Ov:100,100,100|`

## WiFi Configuration Commands

For FluidTouch Settings → Connection tab:

```
$Sta/SSID=YourNetwork       ; Set station SSID
$Sta/Password=YourPass      ; Set station password  
$Sta/IPMode=DHCP            ; Use DHCP (or Static)
$WiFi/Mode=STA              ; Station mode (or AP, STA>AP)
$Telnet/Enable=True         ; Enable telnet
$HTTP/Enable=True           ; Enable HTTP/WebSocket
$HTTP/Port=80               ; Set HTTP port (WebSocket=port+1)
```

**Query current values:**
```
$STA        ; Show all station settings
$AP         ; Show all AP settings
$Wifi       ; Show all WiFi settings
```

## Communication Best Practices

### 1. Connection Initialization
```cpp
// Pseudo-code for FluidTouch startup
1. Open WebSocket to ws://[IP]:81
2. Start receive thread
3. Wait for startup messages
4. Send "$I\n" to verify FluidNC version
5. Send "$G\n" to get parser state
6. Send "?\n" to get initial status
7. Start periodic status queries (every 250-500ms)
```

### 2. Command Queue Management
```cpp
// Conceptual command queue
Queue<String> pending_commands;
int commands_in_buffer = 0;
const int MAX_BUFFER = 10;  // Conservative

void send_command(String cmd) {
    if (commands_in_buffer < MAX_BUFFER) {
        ws.send(cmd + "\n");
        pending_commands.push(cmd);
        commands_in_buffer++;
    } else {
        queue_for_later(cmd);
    }
}

void on_response(String response) {
    if (response == "ok" || response.startsWith("error:")) {
        pending_commands.pop();
        commands_in_buffer--;
        send_next_queued_command();
    }
}
```

### 3. Status Query Loop
```cpp
// Run in separate task
void status_query_task() {
    while (connected) {
        ws.send("?");  // No \n for realtime!
        delay(250);    // 4 Hz update rate
    }
}
```

### 4. Jog Command Pattern
```cpp
void jog_x_positive() {
    String cmd = "$J=G91 X" + String(jog_distance) + " F" + String(jog_feedrate);
    ws.send(cmd + "\n");
}

void jog_stop() {
    ws.send(char(0x85));  // Jog cancel character
}
```

## Implementation Roadmap for FluidTouch

### Phase 1: Basic Connection
- [ ] WebSocket client implementation
- [ ] Connection UI in Settings tab
- [ ] Handle connect/disconnect
- [ ] Display connection status
- [ ] Parse startup messages

### Phase 2: Status Display
- [ ] Implement `?` status query loop
- [ ] Parse status report format
- [ ] Display MPos and WPos
- [ ] Show machine state
- [ ] Update feed rate and spindle speed

### Phase 3: Basic Control
- [ ] Implement jog panel with `$J=` commands
- [ ] Emergency stop (0x18)
- [ ] Feed hold/resume (!, ~)
- [ ] Home command ($H)
- [ ] Unlock ($X)

### Phase 4: Advanced Features
- [ ] Work coordinate zeroing (G10 L20)
- [ ] Feed rate overrides (0x90-0x94)
- [ ] Spindle overrides (0x99-0x9D)
- [ ] Probing commands (G38.2)
- [ ] Macro execution

### Phase 5: File Operations
- [ ] G-code file streaming
- [ ] Command queue management
- [ ] Progress tracking
- [ ] Pause/resume during file run

## Testing Strategy

### Test Environment Setup
1. Install FluidNC on test ESP32 board
2. Configure WiFi with `$Sta/SSID` and `$Sta/Password`
3. Enable WebSocket: `$HTTP/Enable=True`
4. Note IP address from startup messages
5. Connect FluidTouch to same network

### Test Cases
1. **Connection**: Can connect to WebSocket, see startup messages
2. **Status Query**: `?` returns valid status every 250ms
3. **Jog Commands**: `$J=` moves axes correctly
4. **Emergency Stop**: `0x18` halts motion immediately
5. **State Transitions**: Idle → Jog → Idle works correctly
6. **Error Handling**: Invalid commands return proper errors
7. **Reconnection**: Handles disconnect/reconnect gracefully

## References

- **FluidNC Wiki**: http://wiki.fluidnc.com/
- **Grbl Protocol**: https://github.com/gnea/grbl/wiki/Grbl-v1.1-Interface
- **WebSocket RFC**: https://tools.ietf.org/html/rfc6455
- **LinuxCNC G-codes**: http://linuxcnc.org/docs/stable/html/gcode/g-code.html
- **WebUI Source** (example WebSocket usage): https://github.com/luc-github/ESP3D-WEBUI

## ESP32 WebSocket Library Options

For FluidTouch implementation:
- **ArduinoWebsockets** - https://github.com/gilmaimon/ArduinoWebsockets
- **WebSockets by Markus Sattler** - Arduino library manager
- **ESP32 built-in** - `WebSocketsClient.h`

Choose library with:
- ✅ ESP32 support
- ✅ SSL/TLS support (future-proofing)
- ✅ Automatic reconnection
- ✅ Ping/pong keep-alive
- ✅ Good error handling

## Conclusion

FluidNC provides a robust Grbl-compatible protocol over multiple interfaces. For FluidTouch:
- **Use WebSocket** (port 81) as primary connection method
- **Implement proper command buffering** per Grbl protocol
- **Query status regularly** with `?` command
- **Use `$J=` for jogging** (not G0/G1)
- **Handle all machine states** (Idle, Run, Hold, Alarm, etc.)
- **Implement error recovery** with proper state management

The protocol is well-documented and widely used in the CNC community, making FluidTouch compatible with thousands of existing FluidNC installations.
