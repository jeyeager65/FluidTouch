#ifndef FLUIDNC_CLIENT_H
#define FLUIDNC_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include "ui/machine_config.h"
#include <functional>

// Callback type for receiving FluidNC messages
typedef std::function<void(const char* message)> MessageCallback;

// FluidNC machine states
enum MachineState {
    STATE_IDLE = 0,
    STATE_RUN = 1,
    STATE_HOLD = 2,
    STATE_JOG = 3,
    STATE_ALARM = 4,
    STATE_DOOR = 5,
    STATE_CHECK = 6,
    STATE_HOME = 7,
    STATE_SLEEP = 8,
    STATE_DISCONNECTED = 9
};

// FluidNC status report structure
struct FluidNCStatus {
    // Machine state
    MachineState state;
    
    // Positions (X, Y, Z) in mm
    float mpos_x, mpos_y, mpos_z;  // Machine position
    float wpos_x, wpos_y, wpos_z;  // Work position
    float wco_x, wco_y, wco_z;     // Work coordinate offset (WPos = MPos - WCO)
    
    // Feed and spindle
    float feed_rate;        // Current feed rate (mm/min)
    float feed_override;    // Feed override percentage (0-200)
    float rapid_override;   // Rapid override percentage (0-200)
    float spindle_speed;    // Current spindle speed (RPM)
    float spindle_override; // Spindle override percentage (0-200)
    
    // Modal states (Grbl parser state)
    char modal_motion[8];    // G0, G1, G2, G3, etc.
    char modal_wcs[8];       // G54, G55, G56, etc.
    char modal_plane[8];     // G17, G18, G19
    char modal_units[8];     // G20 (inches), G21 (mm)
    char modal_distance[8];  // G90 (absolute), G91 (relative)
    char modal_feedrate[8];  // G93 (inverse time), G94 (units/min), G95 (units/rev)
    char modal_spindle[8];   // M3, M4, M5
    char modal_coolant[8];   // M7, M8, M9
    char modal_tool[8];      // T0, T1, T2, etc.
    
    // Last message from FluidNC
    char last_message[128]; // Store last [MSG:...] or feedback message
    
    // SD card file progress (when running from SD)
    bool is_sd_printing;        // True if running a file from SD card
    float sd_percent;           // Progress percentage (0.0-100.0)
    char sd_filename[64];       // Current file being run
    uint32_t sd_start_time_ms;  // Timestamp when file started (millis())
    uint32_t sd_elapsed_ms;     // Elapsed time since file started
    
    // Connection status
    bool is_connected;
    uint32_t last_update_ms;
    
    // Constructor
    FluidNCStatus() : state(STATE_DISCONNECTED), 
                     mpos_x(0), mpos_y(0), mpos_z(0),
                     wpos_x(0), wpos_y(0), wpos_z(0),
                     wco_x(0), wco_y(0), wco_z(0),
                     feed_rate(0), feed_override(100), rapid_override(100),
                     spindle_speed(0), spindle_override(100),
                     is_sd_printing(false), sd_percent(0), sd_start_time_ms(0), sd_elapsed_ms(0),
                     is_connected(false), last_update_ms(0) {
        strcpy(modal_motion, "G0");
        strcpy(modal_wcs, "G54");
        strcpy(modal_plane, "G17");
        strcpy(modal_units, "G21");
        strcpy(modal_distance, "G90");
        strcpy(modal_feedrate, "G94");
        strcpy(modal_spindle, "M5");
        strcpy(modal_coolant, "M9");
        strcpy(modal_tool, "T0");
        last_message[0] = '\0';  // Empty message initially
        sd_filename[0] = '\0';   // No file initially
    }
};

class FluidNCClient {
public:
    // Initialize the client
    static void init();
    
    // Connect to FluidNC using machine config
    static bool connect(const MachineConfig &config);
    
    // Disconnect from FluidNC
    static void disconnect();
    
    // Check if connected
    static bool isConnected();
    
    // Check if using auto-reporting (true) or fallback polling (false)
    static bool isAutoReporting();
    
    // Main loop - call regularly to handle WebSocket events
    static void loop();
    
    // Get current status
    static const FluidNCStatus& getStatus();
    
    // Send a command to FluidNC (e.g., "G0 X10", "$H", "!")
    static void sendCommand(const char* command);
    
    // Request status report (sends "?")
    static void requestStatusReport();
    
    // Set callback for receiving raw messages (for file list, etc.)
    static void setMessageCallback(MessageCallback callback);
    
    // Clear message callback
    static void clearMessageCallback();
    
    // Set callback for terminal display (excludes status reports starting with '<')
    static void setTerminalCallback(MessageCallback callback);
    
    // Clear terminal callback
    static void clearTerminalCallback();
    
private:
    static WebSocketsClient webSocket;
    static FluidNCStatus currentStatus;
    static MachineConfig currentConfig;
    static uint32_t lastStatusRequestMs;
    static bool initialized;
    static MessageCallback messageCallback;  // Optional callback for raw messages
    static MessageCallback terminalCallback; // Optional callback for terminal display
    
    // Auto-reporting and fallback polling
    static bool autoReportingEnabled;     // True if auto-reporting is active
    static bool autoReportingAttempted;   // True if we tried to enable auto-reporting
    static uint32_t lastPollingMs;        // Last time we sent "?" for fallback polling
    static uint32_t lastGCodePollMs;      // Last time we sent "$G" for GCode state
    static uint32_t lastAutoReportAttemptMs; // Last time we tried to enable auto-reporting
    
    // WebSocket event handler
    static void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    
    // Parse status report message
    static void parseStatusReport(const char* message);
    
    // Parse GCode parser state message
    static void parseGCodeState(const char* message);
    
    // Parse realtime feedback
    static void parseRealtimeFeedback(const char* message);
    
    // Auto-reporting and polling helpers
    static void attemptEnableAutoReporting();
    static void performFallbackPolling();
    
    // Helper to extract float value from status report
    static float extractFloat(const char* str, const char* key);
    
    // Helper to extract string value from status report
    static void extractString(const char* str, const char* key, char* dest, size_t maxLen);
};

#endif // FLUIDNC_CLIENT_H
