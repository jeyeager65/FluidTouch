#ifndef FLUIDNC_CLIENT_H
#define FLUIDNC_CLIENT_H

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include "ui/machine_config.h"
#include <functional>

// Shared state for an in-progress XModem file transfer.
// Written by the XModem FreeRTOS task; read by the LVGL main loop.
// All progress fields are volatile so the compiler never caches them.
struct XModemTransferState {
    volatile bool active;            // Transfer in progress
    volatile bool completed;         // Transfer finished (success or failure)
    volatile bool success;           // Result when completed == true
    volatile size_t bytesSent;       // Bytes successfully transferred
    volatile size_t totalBytes;      // Total file size in bytes
    char filename[128];              // Display filename (written before active=true)
    char error[128];                 // Error message (written before completed=true)
};

// Callback type for receiving FluidNC messages (renamed to avoid conflict with ArduinoWebsockets::MessageCallback)
typedef std::function<void(const char* message)> FluidNCMessageCallback;

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
    
    // Positions (X, Y, Z, A) in mm (or degrees for A)
    float mpos_x, mpos_y, mpos_z, mpos_a;  // Machine position
    float wpos_x, wpos_y, wpos_z, wpos_a;  // Work position
    float wco_x, wco_y, wco_z, wco_a;      // Work coordinate offset (WPos = MPos - WCO)
    
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
    char fluidnc_version[32];  // FluidNC firmware version, e.g. "3.9.5" (parsed from $Build/Info)

    // Pin states (Pn: field)
    bool pin_limit_x;
    bool pin_limit_y;
    bool pin_limit_z;
    bool pin_limit_a;
    bool pin_probe;
    
    // Constructor
    FluidNCStatus() : state(STATE_DISCONNECTED),
                     mpos_x(0), mpos_y(0), mpos_z(0), mpos_a(0),
                     wpos_x(0), wpos_y(0), wpos_z(0), wpos_a(0),
                     wco_x(0), wco_y(0), wco_z(0), wco_a(0),
                     feed_rate(0), feed_override(100), rapid_override(100),
                     spindle_speed(0), spindle_override(100),
                     is_sd_printing(false), sd_percent(0), sd_start_time_ms(0), sd_elapsed_ms(0),
                     is_connected(false), last_update_ms(0),
                     pin_limit_x(false), pin_limit_y(false), pin_limit_z(false), pin_limit_a(false),
                     pin_probe(false) {
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
        fluidnc_version[0] = '\0';  // Unknown until $Build/Info response received
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
    
    // Stop reconnection attempts (sets reconnect interval to 24 hours)
    static void stopReconnectionAttempts();
    
    // Check if connected
    static bool isConnected();
    
    // Check if using auto-reporting (true) or fallback polling (false)
    static bool isAutoReporting();

    // Check if using wired (UART) connection
    static bool isWiredMode();
    
    // Get count of bytes received over UART (for debugging wired connection)
    static uint32_t getUartBytesReceived();

    // Start an XModem file upload from the Display SD card to FluidNC over UART.
    // localPath  : full path on the Display SD card (e.g. "/myfile.nc")
    // remotePath : destination on FluidNC (e.g. "/sd/myfile.nc" or "/localfs/myfile.nc")
    // filename   : display name shown in the progress dialog
    // Returns false immediately if wired mode is not active or a transfer is already running.
    // Only available on Advance hardware (#ifdef HARDWARE_ADVANCE).
    static bool startXModemUpload(const char* localPath, const char* remotePath,
                                  const char* filename);

    // Returns true while an XModem transfer task is running.
    static bool isXModemTransferActive();

    // Access the shared transfer state (read-only from UI).
    static const XModemTransferState& getXModemState();
    
    // Main loop - call regularly to handle WebSocket events
    static void loop();
    
    // Get current status
    static const FluidNCStatus& getStatus();
    
    // Send a command to FluidNC (e.g., "G0 X10", "$H", "!")
    static void sendCommand(const char* command);
    
    // Clear the stored last message
    static void clearLastMessage();
    
    // Request status report (sends "?")
    static void requestStatusReport();
    
    // Get machine IP address (extracted from WebSocket URL)
    static String getMachineIP();
    
    // Set callback for receiving raw messages (for file list, etc.)
    static void setMessageCallback(FluidNCMessageCallback callback);
    
    // Clear message callback
    static void clearMessageCallback();
    
    // Set callback for terminal display (excludes status reports starting with '<')
    static void setTerminalCallback(FluidNCMessageCallback callback);
    
    // Clear terminal callback
    static void clearTerminalCallback();
    
private:
    static websockets::WebsocketsClient webSocket;
    static FluidNCStatus currentStatus;
    static MachineConfig currentConfig;
    static uint32_t lastStatusRequestMs;
    static bool initialized;
    static FluidNCMessageCallback messageCallback;  // Optional callback for raw messages
    static FluidNCMessageCallback terminalCallback; // Optional callback for terminal display
    
    // Auto-reporting and fallback polling
    static bool autoReportingEnabled;     // True if auto-reporting is active
    static bool autoReportingAttempted;   // True if we tried to enable auto-reporting
    static uint32_t lastPollingMs;        // Last time we sent "?" for fallback polling
    static uint32_t lastGCodePollMs;      // Last time we sent "$G" for GCode state
    static uint32_t lastAutoReportAttemptMs; // Last time we tried to enable auto-reporting
    
    // Connection tracking
    static bool everConnectedSuccessfully; // True once first status report received, never reset
    static bool isHandlingDisconnect;     // Guard to prevent re-entrant close() calls

    // Wired (UART) connection support - Advance hardware only
    static bool isWiredConnection;
    static char uartRxBuffer[512];
    static uint16_t uartRxPos;
    static uint32_t uartBytesReceived;  // Total bytes received over UART (debug counter)

    // XModem transfer state and task (Advance hardware only)
    static XModemTransferState xmodemTransferState;
    static bool isXModemTransfer;       // Pauses normal UART parsing while true
    static TaskHandle_t xmodemTaskHandle;
    static void xmodemUploadTask(void* pvParams);
    
    // WebSocket event handlers
    static void onMessageCallback(websockets::WebsocketsMessage message);
    static void onEventsCallback(websockets::WebsocketsEvent event, String data);
    
    // Parse status report message
    static void parseStatusReport(const char* message);
    
    // Parse GCode parser state message
    static void parseGCodeState(const char* message);
    
    // Parse realtime feedback
    static void parseRealtimeFeedback(const char* message);
    
    // Auto-reporting and polling helpers
    static void attemptEnableAutoReporting();
    static void performFallbackPolling();

    // Process a complete line received over UART (wired mode)
    static void processUartLine(char* line);
    
    // Helper to extract float value from status report
    static float extractFloat(const char* str, const char* key);
    
    // Helper to extract string value from status report
    static void extractString(const char* str, const char* key, char* dest, size_t maxLen);
};

#endif // FLUIDNC_CLIENT_H
