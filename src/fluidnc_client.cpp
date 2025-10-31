#include "fluidnc_client.h"
#include "ui/ui_common.h"
#include "ui/tabs/control/ui_tab_control_probe.h"
#include <WiFi.h>

// Static member initialization
WebSocketsClient FluidNCClient::webSocket;
FluidNCStatus FluidNCClient::currentStatus;
MachineConfig FluidNCClient::currentConfig;
uint32_t FluidNCClient::lastStatusRequestMs = 0;
bool FluidNCClient::initialized = false;
MessageCallback FluidNCClient::messageCallback = nullptr;
MessageCallback FluidNCClient::terminalCallback = nullptr;

void FluidNCClient::init() {
    if (initialized) return;
    
    Serial.println("[FluidNC] Initializing client");
    initialized = true;
}

bool FluidNCClient::connect(const MachineConfig &config) {
    if (!initialized) {
        Serial.println("[FluidNC] Error: Client not initialized");
        return false;
    }
    
    // Store config
    currentConfig = config;
    
    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[FluidNC] Error: WiFi not connected");
        return false;
    }
    
    Serial.printf("[FluidNC] Connecting to %s:%d via WebSocket\n", 
                  config.fluidnc_url, config.websocket_port);
    
    // Configure WebSocket with 1-second reconnect for initial connection (will change to 24h once connected)
    webSocket.begin(config.fluidnc_url, config.websocket_port, "/");
    webSocket.onEvent(onWebSocketEvent);
    webSocket.setReconnectInterval(1000);  // 1 second for initial connection attempts
    webSocket.enableHeartbeat(15000, 3000, 2);  // Ping every 15s, timeout 3s, 2 disconnects
    
    currentStatus.is_connected = false;
    
    return true;
}

void FluidNCClient::disconnect() {
    Serial.println("[FluidNC] Disconnecting");
    webSocket.disconnect();
    currentStatus.is_connected = false;
    currentStatus.state = STATE_DISCONNECTED;
}

bool FluidNCClient::isConnected() {
    return currentStatus.is_connected;
}

void FluidNCClient::loop() {
    if (!initialized) return;
    
    // Handle WebSocket events
    webSocket.loop();
    
    // Automatic reporting is enabled via $Report/Interval=250\n command
    // No need for polling - FluidNC will send status updates automatically
}

const FluidNCStatus& FluidNCClient::getStatus() {
    return currentStatus;
}

void FluidNCClient::sendCommand(const char* command) {
    if (!currentStatus.is_connected) {
        Serial.println("[FluidNC] Error: Not connected");
        return;
    }
    
    Serial.printf("[FluidNC] Sending command: %s\n", command);
    webSocket.sendTXT(command);
}

void FluidNCClient::requestStatusReport() {
    if (!currentStatus.is_connected) return;
    
    // Send status query command (realtime command)
    webSocket.sendTXT("?");
}

void FluidNCClient::setMessageCallback(MessageCallback callback) {
    messageCallback = callback;
    Serial.println("[FluidNC] Message callback registered");
}

void FluidNCClient::clearMessageCallback() {
    messageCallback = nullptr;
    Serial.println("[FluidNC] Message callback cleared");
}

void FluidNCClient::setTerminalCallback(MessageCallback callback) {
    terminalCallback = callback;
    Serial.println("[FluidNC] Terminal callback registered");
}

void FluidNCClient::clearTerminalCallback() {
    terminalCallback = nullptr;
    Serial.println("[FluidNC] Terminal callback cleared");
}

void FluidNCClient::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[FluidNC] WebSocket disconnected");
            
            // If we were previously connected, show error
            if (currentStatus.is_connected) {
                // Build error message
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), 
                        "Lost connection to machine.\n\nCheck network connection and\nmachine power, then restart.");
                
                // Show error dialog
                UICommon::showConnectionErrorDialog("Machine Disconnected", error_msg);
            }
            
            currentStatus.is_connected = false;
            currentStatus.state = STATE_DISCONNECTED;
            break;
            
        case WStype_CONNECTED:
            Serial.printf("[FluidNC] WebSocket connected to: %s\n", payload);
            // Don't set is_connected yet - wait for auto-report confirmation
            currentStatus.state = STATE_IDLE;
            currentStatus.last_update_ms = millis();
            
            // Now that we're connected, set reconnect interval to 24 hours to effectively disable auto-reconnect
            webSocket.setReconnectInterval(86400000);  // 24 hours
            Serial.println("[FluidNC] Connected - auto-reconnect disabled (24h interval)");
            
            // Enable automatic status reporting at 250ms intervals
            Serial.println("[FluidNC] Enabling automatic reporting (250ms)");
            webSocket.sendTXT("$Report/Interval=250\n");
            
            // Save for later when implementing manual polling fallback - not needed for auto-reporting
            // Small delay to let the command process
            //delay(50);
            
            // Request initial status with realtime command
            //Serial.println("[FluidNC] Requesting initial status");
            //webSocket.sendTXT("?");
            
            // Request initial GCode parser state
            //webSocket.sendTXT("$G\n");
            break;
            
        case WStype_TEXT:
        case WStype_BIN:
            {
                // FluidNC sends status reports as binary frames
                char* message = (char*)payload;
                
                // Only log non-status messages to reduce serial spam
                if (message[0] != '<') {
                    Serial.printf("[FluidNC] Received (%s): %s\n", 
                                 type == WStype_TEXT ? "TEXT" : "BIN", message);
                }
                
                // Call message callback if registered (for file lists, etc.)
                if (messageCallback) {
                    messageCallback(message);
                }
                
                // Call terminal callback if registered (for terminal display)
                // Terminal tab will filter out status messages (starting with '<')
                if (terminalCallback) {
                    terminalCallback(message);
                }
                
                // Parse different message types
                if (message[0] == '<') {
                    // Status report: <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000|...>
                    parseStatusReport(message);
                } else if (strncmp(message, "[GC:", 4) == 0) {
                    // GCode parser state: [GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]
                    parseGCodeState(message);
                } else if (message[0] == '[') {
                    // Other realtime feedback: [MSG:...], [G92:...], etc.
                    parseRealtimeFeedback(message);
                }
                // Other message types can be added here
            }
            break;
            
        case WStype_ERROR:
            Serial.printf("[FluidNC] WebSocket error: %s\n", payload);
            break;
            
        case WStype_PING:
            Serial.println("[FluidNC] Received ping");
            break;
            
        case WStype_PONG:
            Serial.println("[FluidNC] Received pong");
            break;
            
        default:
            break;
    }
}

void FluidNCClient::parseStatusReport(const char* message) {
    // Example: <Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>
    // Or with WCO: <Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
    
    // Only log status every 5 seconds to reduce spam
    static uint32_t lastStatusLog = 0;
    uint32_t now = millis();
    if (now - lastStatusLog >= 5000) {
        Serial.printf("[FluidNC] Status update (5s): %s\n", message);
        lastStatusLog = now;
    }
    
    currentStatus.last_update_ms = millis();
    
    // Parse machine state
    if (strstr(message, "<Idle")) {
        currentStatus.state = STATE_IDLE;
    } else if (strstr(message, "<Run")) {
        currentStatus.state = STATE_RUN;
    } else if (strstr(message, "<Hold")) {
        currentStatus.state = STATE_HOLD;
    } else if (strstr(message, "<Jog")) {
        currentStatus.state = STATE_JOG;
    } else if (strstr(message, "<Alarm")) {
        currentStatus.state = STATE_ALARM;
    } else if (strstr(message, "<Door")) {
        currentStatus.state = STATE_DOOR;
    } else if (strstr(message, "<Check")) {
        currentStatus.state = STATE_CHECK;
    } else if (strstr(message, "<Home")) {
        currentStatus.state = STATE_HOME;
    } else if (strstr(message, "<Sleep")) {
        currentStatus.state = STATE_SLEEP;
    }
    
    // Parse machine position (MPos:x,y,z)
    const char* mpos = strstr(message, "MPos:");
    if (mpos) {
        sscanf(mpos + 5, "%f,%f,%f", &currentStatus.mpos_x, &currentStatus.mpos_y, &currentStatus.mpos_z);
    }
    
    // Parse work coordinate offset (WCO:x,y,z) - sent periodically by FluidNC
    const char* wco = strstr(message, "WCO:");
    if (wco) {
        sscanf(wco + 4, "%f,%f,%f", &currentStatus.wco_x, &currentStatus.wco_y, &currentStatus.wco_z);
        Serial.printf("[FluidNC] WCO updated: (%.3f,%.3f,%.3f)\n", 
                      currentStatus.wco_x, currentStatus.wco_y, currentStatus.wco_z);
    }
    
    // Calculate work position: WPos = MPos - WCO
    // FluidNC typically only sends MPos in every status report, but includes WCO periodically
    currentStatus.wpos_x = currentStatus.mpos_x - currentStatus.wco_x;
    currentStatus.wpos_y = currentStatus.mpos_y - currentStatus.wco_y;
    currentStatus.wpos_z = currentStatus.mpos_z - currentStatus.wco_z;
    
    // Parse work position directly (WPos:x,y,z) - rarely sent, but handle it
    const char* wpos = strstr(message, "WPos:");
    if (wpos) {
        sscanf(wpos + 5, "%f,%f,%f", &currentStatus.wpos_x, &currentStatus.wpos_y, &currentStatus.wpos_z);
    }
    
    // Parse feed and spindle (FS:feed,spindle)
    const char* fs = strstr(message, "FS:");
    if (fs) {
        sscanf(fs + 3, "%f,%f", &currentStatus.feed_rate, &currentStatus.spindle_speed);
        Serial.printf("[FluidNC] Parsed FS: feed=%.0f, spindle=%.0f\n", 
                      currentStatus.feed_rate, currentStatus.spindle_speed);
    }
    
    // Parse overrides (Ov:feed,rapid,spindle)
    const char* ov = strstr(message, "Ov:");
    if (ov) {
        sscanf(ov + 3, "%f,%f,%f", &currentStatus.feed_override, &currentStatus.rapid_override, &currentStatus.spindle_override);
        Serial.printf("[FluidNC] Parsed Ov: feed=%.0f%%, rapid=%.0f%%, spindle=%.0f%%\n", 
                      currentStatus.feed_override, currentStatus.rapid_override, currentStatus.spindle_override);
    }
    
    // Parse SD card file progress (SD:percent,filename)
    const char* sd = strstr(message, "SD:");
    if (sd) {
        // Extract percent and filename
        float percent = 0;
        char filename_buf[128] = {0};
        
        // Parse: SD:12.5,filename.gcode or SD:100.0,file.nc
        const char* comma = strchr(sd + 3, ',');
        if (comma) {
            sscanf(sd + 3, "%f", &percent);
            strncpy(filename_buf, comma + 1, sizeof(filename_buf) - 1);
            
            // Remove any trailing > or whitespace
            char* end = strchr(filename_buf, '>');
            if (end) *end = '\0';
            end = strchr(filename_buf, '|');
            if (end) *end = '\0';
            
            // Update status
            currentStatus.is_sd_printing = true;
            currentStatus.sd_percent = percent;
            strncpy(currentStatus.sd_filename, filename_buf, sizeof(currentStatus.sd_filename) - 1);
            currentStatus.sd_filename[sizeof(currentStatus.sd_filename) - 1] = '\0';
            
            // Track start time and calculate elapsed time
            if (currentStatus.sd_start_time_ms == 0) {
                currentStatus.sd_start_time_ms = millis();
            }
            currentStatus.sd_elapsed_ms = millis() - currentStatus.sd_start_time_ms;
            
            Serial.printf("[FluidNC] SD Progress: %.1f%% - %s (Elapsed: %lums)\n",
                          percent, currentStatus.sd_filename, currentStatus.sd_elapsed_ms);
        }
    } else {
        // No SD: field means not printing from SD
        if (currentStatus.is_sd_printing) {
            Serial.println("[FluidNC] SD file completed or stopped");
        }
        currentStatus.is_sd_printing = false;
        currentStatus.sd_percent = 0;
        currentStatus.sd_start_time_ms = 0;
        currentStatus.sd_elapsed_ms = 0;
        currentStatus.sd_filename[0] = '\0';
    }
    
    // Parse modal states (Pn:, WCO:, etc.)
    // Note: Full parser state might come in separate $G response
    
    Serial.printf("[FluidNC] Status: State=%d, MPos=(%.3f,%.3f,%.3f), WPos=(%.3f,%.3f,%.3f)\n",
                  currentStatus.state,
                  currentStatus.mpos_x, currentStatus.mpos_y, currentStatus.mpos_z,
                  currentStatus.wpos_x, currentStatus.wpos_y, currentStatus.wpos_z);
}

void FluidNCClient::parseRealtimeFeedback(const char* message) {
    // Handle realtime feedback messages like [MSG:...], [G92:...], [PRB:...], etc.
    Serial.printf("[FluidNC] Feedback: %s\n", message);
    
    // Check for probe result message: [PRB:x,y,z:success]
    // Example: [PRB:151.000,149.000,-137.505:1] (success=1) or [PRB:0.000,0.000,0.000:0] (failure=0)
    if (strncmp(message, "[PRB:", 5) == 0) {
        float x, y, z;
        int success;
        if (sscanf(message + 5, "%f,%f,%f:%d", &x, &y, &z, &success) == 4) {
            char result[256];
            if (success) {
                snprintf(result, sizeof(result), 
                        "Probe SUCCESS\n\nContact at:\nX: %.3f mm\nY: %.3f mm\nZ: %.3f mm", 
                        x, y, z);
            } else {
                snprintf(result, sizeof(result), 
                        "Probe FAILED\n\nNo contact detected within\nmax distance");
            }
            
            // Update probe tab result display
            UITabControlProbe::updateResult(result);
            
            Serial.printf("[FluidNC] Probe %s at (%.3f, %.3f, %.3f)\n", 
                         success ? "SUCCESS" : "FAILED", x, y, z);
        }
    }
    
    // Check for auto-report confirmation message
    if (strstr(message, "websocket auto report interval set") != nullptr) {
        if (!currentStatus.is_connected) {
            currentStatus.is_connected = true;
            Serial.println("[FluidNC] ✓ Auto-report confirmed - connection established");
            
            // Hide connecting popup
            UICommon::hideConnectingPopup();
            
            // Also hide error dialog if showing (connection succeeded after error)
            UICommon::hideConnectionErrorDialog();
        }
    }
    
    // Extract message content from [MSG:...] format
    if (strncmp(message, "[MSG:", 5) == 0) {
        // Find the closing bracket
        const char* end = strchr(message, ']');
        if (end) {
            // Copy message content (skip "[MSG:" and "]")
            size_t len = end - (message + 5);
            if (len >= sizeof(currentStatus.last_message)) {
                len = sizeof(currentStatus.last_message) - 1;
            }
            strncpy(currentStatus.last_message, message + 5, len);
            currentStatus.last_message[len] = '\0';
        }
    } else {
        // For other feedback messages, store the whole thing
        strncpy(currentStatus.last_message, message, sizeof(currentStatus.last_message) - 1);
        currentStatus.last_message[sizeof(currentStatus.last_message) - 1] = '\0';
    }
}

void FluidNCClient::parseGCodeState(const char* message) {
    // Example: [GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]
    // Parse modal states from GCode parser state report
    
    Serial.printf("[FluidNC] GCode State: %s\n", message);
    
    // Extract modal values by searching for specific patterns
    const char* ptr = message + 4;  // Skip "[GC:"
    
    // Parse motion mode (G0, G1, G2, G3, G38.2, G38.3, G38.4, G38.5, G80)
    if (const char* g0 = strstr(ptr, "G0 ")) strcpy(currentStatus.modal_motion, "G0");
    else if (const char* g1 = strstr(ptr, "G1 ")) strcpy(currentStatus.modal_motion, "G1");
    else if (const char* g2 = strstr(ptr, "G2 ")) strcpy(currentStatus.modal_motion, "G2");
    else if (const char* g3 = strstr(ptr, "G3 ")) strcpy(currentStatus.modal_motion, "G3");
    else if (const char* g80 = strstr(ptr, "G80")) strcpy(currentStatus.modal_motion, "G80");
    
    // Parse work coordinate system (G54-G59)
    if (strstr(ptr, "G54")) strcpy(currentStatus.modal_wcs, "G54");
    else if (strstr(ptr, "G55")) strcpy(currentStatus.modal_wcs, "G55");
    else if (strstr(ptr, "G56")) strcpy(currentStatus.modal_wcs, "G56");
    else if (strstr(ptr, "G57")) strcpy(currentStatus.modal_wcs, "G57");
    else if (strstr(ptr, "G58")) strcpy(currentStatus.modal_wcs, "G58");
    else if (strstr(ptr, "G59")) strcpy(currentStatus.modal_wcs, "G59");
    
    // Parse plane (G17, G18, G19)
    if (strstr(ptr, "G17")) strcpy(currentStatus.modal_plane, "G17");
    else if (strstr(ptr, "G18")) strcpy(currentStatus.modal_plane, "G18");
    else if (strstr(ptr, "G19")) strcpy(currentStatus.modal_plane, "G19");
    
    // Parse units (G20=inches, G21=mm)
    if (strstr(ptr, "G20")) strcpy(currentStatus.modal_units, "G20");
    else if (strstr(ptr, "G21")) strcpy(currentStatus.modal_units, "G21");
    
    // Parse distance mode (G90=absolute, G91=incremental)
    if (strstr(ptr, "G90")) strcpy(currentStatus.modal_distance, "G90");
    else if (strstr(ptr, "G91")) strcpy(currentStatus.modal_distance, "G91");
    
    // Parse spindle state (M3=CW, M4=CCW, M5=off)
    if (strstr(ptr, "M3 ")) strcpy(currentStatus.modal_spindle, "M3");
    else if (strstr(ptr, "M4 ")) strcpy(currentStatus.modal_spindle, "M4");
    else if (strstr(ptr, "M5")) strcpy(currentStatus.modal_spindle, "M5");
    
    // Parse coolant state (M7=mist, M8=flood, M9=off)
    if (strstr(ptr, "M7 ")) strcpy(currentStatus.modal_coolant, "M7");
    else if (strstr(ptr, "M8 ")) strcpy(currentStatus.modal_coolant, "M8");
    else if (strstr(ptr, "M9")) strcpy(currentStatus.modal_coolant, "M9");
    
    // Parse tool number (T0, T1, etc.)
    const char* tool = strstr(ptr, " T");
    if (tool) {
        int toolNum;
        if (sscanf(tool, " T%d", &toolNum) == 1) {
            snprintf(currentStatus.modal_tool, sizeof(currentStatus.modal_tool), "T%d", toolNum);
        }
    }
    
    // Parse feed rate (F) - programmed feed rate in mm/min
    const char* feed = strstr(ptr, " F");
    if (feed) {
        float feedValue;
        if (sscanf(feed, " F%f", &feedValue) == 1) {
            // Only update if not already set by status report
            if (currentStatus.feed_rate == 0.0f) {
                currentStatus.feed_rate = feedValue;
            }
        }
    }
    
    // Parse spindle speed (S) - programmed spindle speed in RPM
    const char* spindle = strstr(ptr, " S");
    if (spindle) {
        float spindleValue;
        if (sscanf(spindle, " S%f", &spindleValue) == 1) {
            // Only update if not already set by status report
            if (currentStatus.spindle_speed == 0.0f) {
                currentStatus.spindle_speed = spindleValue;
            }
        }
    }
    
    Serial.printf("[FluidNC] Parsed modals: Motion=%s, WCS=%s, Plane=%s, Units=%s, Distance=%s, Spindle=%s, Coolant=%s, Tool=%s, Feed=%.0f, SpindleSpeed=%.0f\n",
                  currentStatus.modal_motion, currentStatus.modal_wcs, currentStatus.modal_plane,
                  currentStatus.modal_units, currentStatus.modal_distance, currentStatus.modal_spindle,
                  currentStatus.modal_coolant, currentStatus.modal_tool,
                  currentStatus.feed_rate, currentStatus.spindle_speed);
}

float FluidNCClient::extractFloat(const char* str, const char* key) {
    const char* pos = strstr(str, key);
    if (!pos) return 0.0f;
    
    return atof(pos + strlen(key));
}

void FluidNCClient::extractString(const char* str, const char* key, char* dest, size_t maxLen) {
    const char* pos = strstr(str, key);
    if (!pos) {
        dest[0] = '\0';
        return;
    }
    
    pos += strlen(key);
    const char* end = strchr(pos, '|');
    if (!end) end = strchr(pos, '>');
    if (!end) end = pos + strlen(pos);
    
    size_t len = end - pos;
    if (len >= maxLen) len = maxLen - 1;
    
    strncpy(dest, pos, len);
    dest[len] = '\0';
}
