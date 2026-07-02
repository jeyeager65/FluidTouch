#include "network/fluidnc_client.h"
#include "network/xmodem_sender.h"
#include "ui/ui_common.h"
#include "ui/tabs/control/ui_tab_control_probe.h"
#include "debug_log.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#ifdef HARDWARE_ADVANCE
#include <HardwareSerial.h>
#include <SD.h>
static HardwareSerial wiredSerial(2);  // UART1 (board label): RX=GPIO19, TX=GPIO20
// "USB CDC" mode on the Elecrow CrowPanel actually uses UART0 (GPIO43 TX / GPIO44 RX)
// routed through the on-board CH340 USB-UART bridge to the USB-C jack. FluidNC's USB
// host sees the CH340 as a USB CDC ACM device. The ESP32-S3's native USB peripheral
// on GPIO19/20 is NOT connected to the USB-C jack on this board, so we use Serial
// (UART0) for this mode rather than the USBCDC class.
//
// While USB CDC mode is active, the global `g_serialMuted` flag (declared in
// debug_log.h) silences LOG_PRINT/LOG_PRINTLN/LOG_PRINTF across the firmware so
// stray debug bytes never reach FluidNC as garbage commands.
//
// Currently-active serial stream (set on connect, used by all read/write paths).
// nullptr means no serial connection is active (WebSocket mode or disconnected).
static Stream*       activeStream  = nullptr;
#endif

using namespace websockets;

// Static member initialization
WebsocketsClient FluidNCClient::webSocket;
FluidNCStatus FluidNCClient::currentStatus;
MachineConfig FluidNCClient::currentConfig;
uint32_t FluidNCClient::lastStatusRequestMs = 0;
bool FluidNCClient::initialized = false;
FluidNCMessageCallback FluidNCClient::messageCallback = nullptr;
FluidNCMessageCallback FluidNCClient::terminalCallback = nullptr;
bool FluidNCClient::autoReportingEnabled = false;
bool FluidNCClient::autoReportingAttempted = false;
uint32_t FluidNCClient::lastPollingMs = 0;
uint32_t FluidNCClient::lastGCodePollMs = 0;
uint32_t FluidNCClient::lastAutoReportAttemptMs = 0;
bool FluidNCClient::everConnectedSuccessfully = false;
bool FluidNCClient::isHandlingDisconnect = false;
ConnectionType FluidNCClient::activeSerialMode = CONN_WIFI;  // CONN_WIFI = "no serial active"
char FluidNCClient::uartRxBuffer[512] = {};
uint16_t FluidNCClient::uartRxPos = 0;
uint32_t FluidNCClient::uartBytesReceived = 0;
XModemTransferState FluidNCClient::xmodemTransferState = {};
bool FluidNCClient::isXModemTransfer = false;
TaskHandle_t FluidNCClient::xmodemTaskHandle = nullptr;

void FluidNCClient::init() {
    if (initialized) return;
    
    LOG_PRINTLN("[FluidNC] Initializing client");
    initialized = true;
}

bool FluidNCClient::connect(const MachineConfig &config) {
    if (!initialized) {
        LOG_PRINTLN("[FluidNC] Error: Client not initialized");
        return false;
    }
    
    // Store config
    currentConfig = config;
    
#ifdef HARDWARE_ADVANCE
    if (config.connection_type == CONN_UART) {
        LOG_PRINTF("[FluidNC] Connecting via UART1 (RX=19, TX=20) at %d baud\n", config.uart_baud_rate);
        activeSerialMode = CONN_UART;
        uartRxPos = 0;
        autoReportingEnabled = false;
        autoReportingAttempted = false;
        everConnectedSuccessfully = false;
        uartBytesReceived = 0;
        wiredSerial.begin(config.uart_baud_rate, SERIAL_8N1, 19, 20);  // RX=19, TX=20
        activeStream = &wiredSerial;
        // Request firmware version and enable auto-reporting
        wiredSerial.print("$Build/Info\n");
        wiredSerial.print("$Report/Interval=250\n");
        autoReportingAttempted = true;
        autoReportingEnabled = false;
        lastAutoReportAttemptMs = millis();
        lastPollingMs = millis() - 1000;
        lastGCodePollMs = millis() - 10000;
        currentStatus.is_connected = false;  // Set true on first status report
        LOG_PRINTLN("[FluidNC] UART1 opened, waiting for status reports...");
        return true;
    }
    if (config.connection_type == CONN_USB_CDC) {
        // USB CDC over the on-board USB-C jack goes through a CH340 USB-UART
        // bridge to ESP32 UART0. The baud rate here must match what FluidNC's
        // `uart3: usb_host: baud:` is set to in config.yaml. Default 115200.
        uint32_t baud = (config.uart_baud_rate > 0) ? config.uart_baud_rate : 115200;

        // Pre-mute *before* touching Serial so any LOG_* call from a concurrent
        // task (or from Serial.end() / Serial.begin() internals) can't escape
        // onto the wire as garbage to FluidNC.
        LOG_PRINTF("[FluidNC] Connecting via USB CDC (UART0 / on-board USB-UART bridge) at %u baud\n",
                   (unsigned)baud);
        LOG_PRINTLN("[FluidNC] NOTE: Serial debug output is suppressed until disconnect");
        Serial.flush();
        delay(50);          // Let the last debug bytes drain physically
        g_serialMuted = true;

        // Re-open UART0 at the FluidNC USB CDC baud rate with a larger RX buffer
        // so back-to-back status reports + WCO + GCode state don't overflow the
        // default 256-byte ring at 115200.
        Serial.end();
        delay(100);          // Brief gap so FluidNC's USB host sees a clean re-enumeration
        Serial.setRxBufferSize(2048);
        Serial.begin(baud);

        activeSerialMode = CONN_USB_CDC;
        uartRxPos = 0;
        autoReportingEnabled = false;
        autoReportingAttempted = false;
        everConnectedSuccessfully = false;
        uartBytesReceived = 0;
        activeStream = &Serial;

        // FluidNC's USB host typically needs ~500-1500 ms to enumerate the CH340
        // and mount its CDC endpoint. Sending commands before that is just throwing
        // bytes into a void. Wait for the link to settle, drain any startup garbage
        // (the CH340 emits a break on open and FluidNC echoes a welcome banner),
        // then send the report-interval request. The auto-report attempt timer
        // doesn't start until after this settle, so the 2 s fallback-polling timeout
        // is measured from a meaningful baseline.
        uint32_t settleEnd = millis() + 1500;
        while (millis() < settleEnd) {
            while (Serial.available()) {
                (void)Serial.read();
                uartBytesReceived++;
            }
            delay(10);
        }

        // Send a soft "are you there" then enable auto-reporting. If FluidNC
        // missed the first $Report/Interval (still mid-mount), we'll resend
        // it from loop() on a state change or via fallback polling.
        Serial.print("\n");                       // Flush any partial line
        Serial.print("$Build/Info\n");
        Serial.print("$Report/Interval=250\n");
        autoReportingAttempted = true;
        autoReportingEnabled = false;
        lastAutoReportAttemptMs = millis();
        lastPollingMs = millis() - 1000;
        lastGCodePollMs = millis() - 10000;
        currentStatus.is_connected = false;
        return true;
    }
#endif

    // Reset serial-mode flag for WebSocket connections
    activeSerialMode = CONN_WIFI;
#ifdef HARDWARE_ADVANCE
    activeStream = nullptr;
#endif

    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        LOG_PRINTLN("[FluidNC] Error: WiFi not connected");
        return false;
    }
    
    LOG_PRINTF("[FluidNC] Connecting to %s:%d via WebSocket\n", 
                  config.fluidnc_url, config.websocket_port);
    
    // Resolve hostname if needed (mDNS support)
    String resolvedHost = String(config.fluidnc_url);
    IPAddress serverIP;
    if (resolvedHost.indexOf('.') == -1 || resolvedHost.endsWith(".local")) {
        // It's a hostname or mDNS name, try to resolve it
        LOG_PRINTF("[FluidNC] Resolving hostname: %s\n", resolvedHost.c_str());
        
        // Try resolving with retries (mDNS can be slow to respond)
        bool resolved = false;
        for (int attempt = 0; attempt < 5 && !resolved; attempt++) {
            if (attempt > 0) {
                LOG_PRINTF("[FluidNC] Retry attempt %d/5...\n", attempt + 1);
                delay(1000);  // Longer delay between retries for mDNS
            }
            
            if (resolvedHost.endsWith(".local")) {
                // Use MDNS.queryHost() for .local hostnames (strip the .local suffix)
                String hostname = resolvedHost.substring(0, resolvedHost.length() - 6);
                LOG_PRINTF("[FluidNC] Using mDNS query for: %s\n", hostname.c_str());
                serverIP = MDNS.queryHost(hostname);
            } else {
                // Use standard DNS for non-.local hostnames
                WiFi.hostByName(resolvedHost.c_str(), serverIP);
            }
            
            // Validate that we got a real IP address (not 0.0.0.0)
            if (serverIP != IPAddress(0, 0, 0, 0)) {
                LOG_PRINTF("[FluidNC] Resolved on attempt %d to IP: %s\n", attempt + 1, serverIP.toString().c_str());
                resolved = true;
            } else {
                LOG_PRINTF("[FluidNC] Attempt %d returned invalid IP (0.0.0.0)\n", attempt + 1);
            }
        }
        
        if (!resolved) {
            LOG_PRINTF("[FluidNC] Failed to resolve hostname: %s after 5 attempts\n", resolvedHost.c_str());
            LOG_PRINTLN("[FluidNC] Tip: Try using the IP address instead, or check that mDNS is working on your network");
            return false;
        }
        resolvedHost = serverIP.toString();
        LOG_PRINTF("[FluidNC] Using resolved IP: %s\n", resolvedHost.c_str());
    }
    
    // Set up event callbacks
    webSocket.onMessage(onMessageCallback);
    webSocket.onEvent(onEventsCallback);
    
    // Connect to WebSocket (ws://hostname:port/)
    char wsUrl[128];
    snprintf(wsUrl, sizeof(wsUrl), "ws://%s:%d/", resolvedHost.c_str(), config.websocket_port);
    bool connected = webSocket.connect(wsUrl);
    
    if (!connected) {
        LOG_PRINTLN("[FluidNC] Initial connection failed");
        currentStatus.is_connected = false;
        return false;
    }
    
    LOG_PRINTLN("[FluidNC] WebSocket connection initiated");
    currentStatus.is_connected = false;  // Will be set to true when first message received
    
    return true;
}

void FluidNCClient::disconnect() {
    LOG_PRINTLN("[FluidNC] Disconnecting");
#ifdef HARDWARE_ADVANCE
    if (activeSerialMode == CONN_UART) {
        wiredSerial.end();
        activeSerialMode = CONN_WIFI;
        activeStream = nullptr;
        uartRxPos = 0;
    } else if (activeSerialMode == CONN_USB_CDC) {
        // Re-open Serial at the default debug baud so the console comes back to life.
        Serial.end();
        delay(50);
        Serial.begin(115200);
        activeSerialMode = CONN_WIFI;
        activeStream = nullptr;
        uartRxPos = 0;
        // Unmute *after* Serial is back at the debug baud so the first print lands
        // on the host terminal cleanly.
        g_serialMuted = false;
        Serial.println("[FluidNC] USB CDC disconnected, debug console restored");
    } else
#endif
    if (webSocket.available()) {
        webSocket.close();
    }
    currentStatus.is_connected = false;
    currentStatus.state = STATE_DISCONNECTED;
    autoReportingEnabled = false;
    autoReportingAttempted = false;
}

void FluidNCClient::stopReconnectionAttempts() {
    LOG_PRINTLN("[FluidNC] Stopping reconnection attempts");
    // Don't call close() if we're already handling a disconnect event
    // This prevents re-entrant calls that cause stack overflow
    if (!isHandlingDisconnect && webSocket.available()) {
        webSocket.close();
    }
    currentStatus.is_connected = false;
    currentStatus.state = STATE_DISCONNECTED;
}

bool FluidNCClient::isConnected() {
#ifdef HARDWARE_ADVANCE
    if (activeSerialMode == CONN_UART || activeSerialMode == CONN_USB_CDC) {
        return currentStatus.is_connected;
    }
#endif
    return currentStatus.is_connected && webSocket.available();
}

bool FluidNCClient::isAutoReporting() {
    return autoReportingEnabled;
}

bool FluidNCClient::isUartMode() {
#ifdef HARDWARE_ADVANCE
    return activeSerialMode == CONN_UART;
#else
    return false;
#endif
}

bool FluidNCClient::isUsbCdcMode() {
#ifdef HARDWARE_ADVANCE
    return activeSerialMode == CONN_USB_CDC;
#else
    return false;
#endif
}

bool FluidNCClient::isSerialMode() {
#ifdef HARDWARE_ADVANCE
    return activeSerialMode == CONN_UART || activeSerialMode == CONN_USB_CDC;
#else
    return false;
#endif
}

bool FluidNCClient::isWiFiMode() {
#ifdef HARDWARE_ADVANCE
    // On Advance, "WiFi mode" means no serial stream is active. activeSerialMode
    // is the runtime source of truth (currentConfig may not match what's actually
    // open, e.g. after disconnect()).
    return activeSerialMode == CONN_WIFI;
#else
    return true;  // Basic hardware has only WiFi
#endif
}

uint32_t FluidNCClient::getUartBytesReceived() {
    return uartBytesReceived;
}

// ============================================================================
// XModem upload support (Advance hardware only)
// ============================================================================

#ifdef HARDWARE_ADVANCE
struct XModemTaskParams {
    char localPath[256];
    char remotePath[256];
};

void FluidNCClient::xmodemUploadTask(void* pvParams) {
    XModemTaskParams* params = (XModemTaskParams*)pvParams;
    LOG_PRINTF("[XModem] Task started: %s -> %s\n", params->localPath, params->remotePath);

    File file = SD.open(params->localPath);
    if (!file) {
        LOG_PRINTLN("[XModem] Failed to open local file");
        strncpy(xmodemTransferState.error, "Failed to open file on SD card",
                sizeof(xmodemTransferState.error) - 1);
        xmodemTransferState.success = false;
        xmodemTransferState.completed = true;
        xmodemTransferState.active = false;
        isXModemTransfer = false;
        free(params);
        vTaskDelete(nullptr);
        return;
    }

    size_t fileSize = file.size();
    xmodemTransferState.totalBytes = fileSize;
    xmodemTransferState.bytesSent  = 0;

    // Use whichever serial stream the user picked (UART or USB CDC)
    Stream* xmodemStream = activeStream;
    if (!xmodemStream) {
        LOG_PRINTLN("[XModem] No active serial stream");
        strncpy(xmodemTransferState.error, "No serial stream",
                sizeof(xmodemTransferState.error) - 1);
        xmodemTransferState.success   = false;
        xmodemTransferState.completed = true;
        xmodemTransferState.active    = false;
        isXModemTransfer = false;
        file.close();
        free(params);
        vTaskDelete(nullptr);
        return;
    }

    bool ok = XModemSender::sendFile(
        *xmodemStream,
        params->remotePath,
        file,
        fileSize,
        [](size_t sent, size_t total) {
            xmodemTransferState.bytesSent = sent;
        }
    );

    file.close();

    // Drain any bytes left in the serial buffer before resuming normal parsing
    uint32_t drainEnd = millis() + 300;
    while (millis() < drainEnd) {
        while (xmodemStream->available()) xmodemStream->read();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (ok) {
        xmodemTransferState.error[0] = '\0';
    } else if (xmodemTransferState.error[0] == '\0') {
        strncpy(xmodemTransferState.error, "Transfer failed",
                sizeof(xmodemTransferState.error) - 1);
    }
    xmodemTransferState.success   = ok;
    xmodemTransferState.completed = true;
    xmodemTransferState.active    = false;
    isXModemTransfer = false;  // Resume normal UART line parsing

    LOG_PRINTF("[XModem] Task complete: %s\n", ok ? "SUCCESS" : "FAILED");

    free(params);
    xmodemTaskHandle = nullptr;
    vTaskDelete(nullptr);
}
#endif // HARDWARE_ADVANCE

bool FluidNCClient::startXModemUpload(const char* localPath,
                                       const char* remotePath,
                                       const char* filename) {
#ifndef HARDWARE_ADVANCE
    LOG_PRINTLN("[XModem] Not supported on Basic hardware");
    return false;
#else
    if (activeSerialMode != CONN_UART && activeSerialMode != CONN_USB_CDC) {
        LOG_PRINTLN("[XModem] Not in a serial (UART or USB CDC) mode");
        return false;
    }
    if (xmodemTransferState.active) {
        LOG_PRINTLN("[XModem] Transfer already in progress");
        return false;
    }

    XModemTaskParams* params = (XModemTaskParams*)malloc(sizeof(XModemTaskParams));
    if (!params) {
        LOG_PRINTLN("[XModem] Out of memory for task params");
        return false;
    }
    strncpy(params->localPath,  localPath,  sizeof(params->localPath)  - 1);
    strncpy(params->remotePath, remotePath, sizeof(params->remotePath) - 1);
    params->localPath[sizeof(params->localPath)   - 1] = '\0';
    params->remotePath[sizeof(params->remotePath) - 1] = '\0';

    // Reset state
    memset((void*)&xmodemTransferState, 0, sizeof(xmodemTransferState));
    strncpy(xmodemTransferState.filename, filename, sizeof(xmodemTransferState.filename) - 1);
    xmodemTransferState.active = true;

    // Block normal UART parsing while the transfer runs
    isXModemTransfer = true;

    BaseType_t rc = xTaskCreatePinnedToCore(
        xmodemUploadTask,
        "xmodem_upload",
        8192,
        params,
        5,
        &xmodemTaskHandle,
        0  // Run on core 0; LVGL runs on core 1
    );

    if (rc != pdPASS) {
        LOG_PRINTLN("[XModem] xTaskCreate failed");
        free(params);
        memset((void*)&xmodemTransferState, 0, sizeof(xmodemTransferState));
        isXModemTransfer = false;
        return false;
    }

    LOG_PRINTF("[XModem] Upload task created: %s -> %s\n", localPath, remotePath);
    return true;
#endif
}

bool FluidNCClient::isXModemTransferActive() {
    return xmodemTransferState.active;
}

const XModemTransferState& FluidNCClient::getXModemState() {
    return xmodemTransferState;
}

// ============================================================================
// Main event loop
// ============================================================================

void FluidNCClient::loop() {
    if (!initialized) return;

#ifdef HARDWARE_ADVANCE
    if (activeStream != nullptr) {
        // While an XModem transfer task is running, skip normal line parsing
        if (isXModemTransfer) return;

        // Drain serial receive buffer, process complete lines
        while (activeStream->available()) {
            char c = (char)activeStream->read();
            uartBytesReceived++;
            if (c == '\n') {
                uartRxBuffer[uartRxPos] = '\0';
                if (uartRxPos > 0) {
                    processUartLine(uartRxBuffer);
                }
                uartRxPos = 0;
            } else if (c != '\r') {
                if (uartRxPos < sizeof(uartRxBuffer) - 1) {
                    uartRxBuffer[uartRxPos++] = c;
                }
            }
        }

        // Auto-report timeout check (same logic as WebSocket path)
        if (autoReportingAttempted && !autoReportingEnabled) {
            uint32_t now = millis();
            if (now - lastAutoReportAttemptMs >= 2000) {
                autoReportingAttempted = false;  // Switch to fallback polling
            }
        }
        // Fallback polling over active serial stream
        if (!autoReportingAttempted && !autoReportingEnabled) {
            uint32_t now = millis();
            if (now - lastPollingMs >= 1000) {
                activeStream->print("?");
                lastPollingMs = now;
            }
            if (now - lastGCodePollMs >= 10000) {
                activeStream->print("$G\n");
                lastGCodePollMs = now;
            }
        }
        return;
    }
#endif
    
    // Handle WebSocket events - ArduinoWebsockets handles polling internally
    webSocket.poll();
    
    // Only check auto-reporting and polling if WebSocket is connected
    if (!webSocket.available()) {
        return;
    }
    
    // Check if auto-reporting timed out (no status received within 2 seconds of attempt)
    if (autoReportingAttempted && !autoReportingEnabled) {
        uint32_t now = millis();
        if (now - lastAutoReportAttemptMs >= 2000) {
            // No status received - assume auto-reporting failed
            LOG_PRINTLN("[FluidNC] Auto-reporting timeout - switching to fallback polling");
            autoReportingAttempted = false;  // Allow fallback polling to start
        }
    }
    
    // Only perform fallback polling if:
    // 1. Auto-reporting attempt completed (either succeeded or timed out)
    // 2. Auto-reporting is not enabled
    if (!autoReportingAttempted && !autoReportingEnabled) {
        performFallbackPolling();
    }
}

const FluidNCStatus& FluidNCClient::getStatus() {
    return currentStatus;
}

void FluidNCClient::clearLastMessage() {
    currentStatus.last_message[0] = '\0';
}

void FluidNCClient::sendCommand(const char* command) {
    if (!currentStatus.is_connected) {
        LOG_PRINTLN("[FluidNC] Error: Not connected");
        return;
    }
    
    LOG_PRINTF("[FluidNC] Sending command: %s\n", command);
#ifdef HARDWARE_ADVANCE
    if (activeStream != nullptr) {
        activeStream->print(command);
        return;
    }
#endif
    webSocket.send(command);
}

void FluidNCClient::requestStatusReport() {
    if (!currentStatus.is_connected) return;
    
#ifdef HARDWARE_ADVANCE
    if (activeStream != nullptr) {
        activeStream->print("?");
        return;
    }
#endif
    // Send status query command (realtime command)
    webSocket.send("?");
}

String FluidNCClient::getMachineIP() {
    if (!currentStatus.is_connected) return "";
    
    // Get URL from config
    String url = String(currentConfig.fluidnc_url);
    
    // Extract IP from URL (may already be just an IP address)
    // If it starts with a protocol, remove it
    int protocolEnd = url.indexOf("://");
    if (protocolEnd >= 0) {
        url = url.substring(protocolEnd + 3);
    }
    
    // Remove port if present
    int portStart = url.indexOf(":");
    if (portStart > 0) {
        url = url.substring(0, portStart);
    }
    
    // Remove path if present
    int pathStart = url.indexOf("/");
    if (pathStart > 0) {
        url = url.substring(0, pathStart);
    }
    
    return url;
}

void FluidNCClient::setMessageCallback(FluidNCMessageCallback callback) {
    messageCallback = callback;
    LOG_PRINTLN("[FluidNC] Message callback registered");
}

void FluidNCClient::clearMessageCallback() {
    messageCallback = nullptr;
    LOG_PRINTLN("[FluidNC] Message callback cleared");
}

void FluidNCClient::setTerminalCallback(FluidNCMessageCallback callback) {
    terminalCallback = callback;
    LOG_PRINTLN("[FluidNC] Terminal callback registered");
}

void FluidNCClient::clearTerminalCallback() {
    terminalCallback = nullptr;
    LOG_PRINTLN("[FluidNC] Terminal callback cleared");
}

void FluidNCClient::onMessageCallback(WebsocketsMessage message) {
    const char* payload = message.c_str();
    
    // Only log non-status messages to reduce serial spam
    if (payload[0] != '<') {
        LOG_PRINTF("[FluidNC] Received: %s\n", payload);
    }
    
    // Call message callback if registered (for file lists, etc.)
    if (messageCallback) {
        messageCallback(payload);
    }
    
    // Call terminal callback if registered (for terminal display)
    // Terminal tab will filter out status messages (starting with '<')
    if (terminalCallback) {
        terminalCallback(payload);
    }
    
    // Parse different message types
    if (payload[0] == '<') {
        // Status report: <Idle|MPos:0.000,0.000,0.000|WPos:0.000,0.000,0.000|...>
        parseStatusReport(payload);
    } else if (strncmp(payload, "[GC:", 4) == 0) {
        // GCode parser state: [GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]
        parseGCodeState(payload);
    } else if (payload[0] == '[') {
        // Other realtime feedback: [MSG:...], [G92:...], etc.
        parseRealtimeFeedback(payload);
    } else if (strncmp(payload, "error:", 6) == 0 || strncmp(payload, "ALARM:", 6) == 0) {
        // Plain-text error/alarm lines, e.g. "error:9" or "ALARM:2"
        strncpy(currentStatus.last_message, payload, sizeof(currentStatus.last_message) - 1);
        currentStatus.last_message[sizeof(currentStatus.last_message) - 1] = '\0';
    }
}

void FluidNCClient::onEventsCallback(WebsocketsEvent event, String data) {
    switch(event) {
        case WebsocketsEvent::ConnectionOpened:
            LOG_PRINTLN("[FluidNC] WebSocket connected");
            // Don't set is_connected yet - wait for first status report
            currentStatus.state = STATE_IDLE;
            currentStatus.last_update_ms = millis();
            
            // Initialize polling timestamps to allow immediate fallback polling if auto-reporting fails
            lastPollingMs = millis() - 1000;
            lastGCodePollMs = millis() - 10000;
            
            // Attempt to enable automatic status reporting
            attemptEnableAutoReporting();
            
            // Request firmware version info
            webSocket.send("$Build/Info\n");
            break;
            
        case WebsocketsEvent::ConnectionClosed:
            LOG_PRINTLN("[FluidNC] WebSocket disconnected");
            
            // Set flag to prevent re-entrant close() calls
            isHandlingDisconnect = true;
            
            // Only show popup if we've ever successfully received a status report
            if (everConnectedSuccessfully) {
                // Build error message
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), 
                        "Lost connection to machine.\n\nCheck network connection and\nmachine power, then restart.");
                
                // Show error dialog
                UICommon::showConnectionErrorDialog("Machine Disconnected", error_msg);
            }
            
            currentStatus.is_connected = false;
            currentStatus.state = STATE_DISCONNECTED;
            
            // Clear flag after handling disconnect
            isHandlingDisconnect = false;
            break;
            
        case WebsocketsEvent::GotPing:
            LOG_PRINTLN("[FluidNC] Received ping");
            break;
            
        case WebsocketsEvent::GotPong:
            LOG_PRINTLN("[FluidNC] Received pong");
            break;
    }
}

void FluidNCClient::parseStatusReport(const char* message) {
    // Example: <Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>
    // Or with WCO: <Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
    
    // If we receive a status report while auto-reporting was attempted, mark it as enabled
    if (autoReportingAttempted && !autoReportingEnabled) {
        autoReportingEnabled = true;
        autoReportingAttempted = false;  // Clear the attempt flag
        LOG_PRINTLN("[FluidNC] ✓ Auto-reporting confirmed (status received)");
    }
    
    // If we receive ANY status report and not connected yet, mark as connected
    // This handles both auto-reporting and fallback polling
    if (!currentStatus.is_connected) {
        currentStatus.is_connected = true;
        LOG_PRINTLN("[FluidNC] ✓ Connection established (status received)");
        
        // Hide connecting popup
        UICommon::hideConnectingPopup();
        
        // Also hide error dialog if showing
        UICommon::hideConnectionErrorDialog();
    }
    
    // Only log status every 5 seconds to reduce spam
    static uint32_t lastStatusLog = 0;
    uint32_t now = millis();
    if (now - lastStatusLog >= 5000) {
        LOG_PRINTF("[FluidNC] Status update (5s): %s\n", message);
        lastStatusLog = now;
    }
    
    currentStatus.last_update_ms = millis();
    
    // Mark that we've successfully received at least one status report
    // This flag is used to distinguish between initial connection handshake failures
    // and actual disconnections after successful communication
    if (!everConnectedSuccessfully) {
        everConnectedSuccessfully = true;
        LOG_PRINTLN("[FluidNC] ✓ First status report received - connection validated");
    }
    
    // Track previous state for state change detection
    static MachineState previousState = STATE_DISCONNECTED;
    MachineState newState = currentStatus.state;
    
    // Parse machine state
    if (strstr(message, "<Idle")) {
        newState = STATE_IDLE;
    } else if (strstr(message, "<Run")) {
        newState = STATE_RUN;
    } else if (strstr(message, "<Hold")) {
        newState = STATE_HOLD;
    } else if (strstr(message, "<Jog")) {
        newState = STATE_JOG;
    } else if (strstr(message, "<Alarm")) {
        newState = STATE_ALARM;
    } else if (strstr(message, "<Door")) {
        newState = STATE_DOOR;
    } else if (strstr(message, "<Check")) {
        newState = STATE_CHECK;
    } else if (strstr(message, "<Home")) {
        newState = STATE_HOME;
    } else if (strstr(message, "<Sleep")) {
        newState = STATE_SLEEP;
    }
    
    // Detect state change to IDLE from HOLD or RUN - retry auto-reporting
    if (newState == STATE_IDLE && (previousState == STATE_HOLD || previousState == STATE_RUN)) {
        if (!autoReportingEnabled) {
            LOG_PRINTLN("[FluidNC] Machine returned to IDLE - retrying auto-reporting");
            attemptEnableAutoReporting();
        }
    }
    
    // Update state and track for next comparison
    currentStatus.state = newState;
    previousState = newState;
    
    // Parse machine position (MPos:x,y,z,a)
    const char* mpos = strstr(message, "MPos:");
    if (mpos) {
        // Try parsing 4 values (X,Y,Z,A), but allow 3 values (X,Y,Z) for machines without A-axis
        int parsed = sscanf(mpos + 5, "%f,%f,%f,%f",
                           &currentStatus.mpos_x, &currentStatus.mpos_y,
                           &currentStatus.mpos_z, &currentStatus.mpos_a);
        if (parsed == 3) {
            // Only 3 axes parsed - machine doesn't have A-axis, set to 0
            currentStatus.mpos_a = 0.0f;
        }
    }
    
    // Parse work coordinate offset (WCO:x,y,z,a) - sent periodically by FluidNC
    const char* wco = strstr(message, "WCO:");
    if (wco) {
        // Try parsing 4 values (X,Y,Z,A), but allow 3 values (X,Y,Z) for machines without A-axis
        int parsed = sscanf(wco + 4, "%f,%f,%f,%f",
                           &currentStatus.wco_x, &currentStatus.wco_y,
                           &currentStatus.wco_z, &currentStatus.wco_a);
        if (parsed == 3) {
            // Only 3 axes parsed - machine doesn't have A-axis, set to 0
            currentStatus.wco_a = 0.0f;
        }
        LOG_PRINTF("[FluidNC] WCO updated: (%.3f,%.3f,%.3f,%.3f)\n",
                      currentStatus.wco_x, currentStatus.wco_y, currentStatus.wco_z, currentStatus.wco_a);
    }
    
    // Calculate work position: WPos = MPos - WCO
    // FluidNC typically only sends MPos in every status report, but includes WCO periodically
    currentStatus.wpos_x = currentStatus.mpos_x - currentStatus.wco_x;
    currentStatus.wpos_y = currentStatus.mpos_y - currentStatus.wco_y;
    currentStatus.wpos_z = currentStatus.mpos_z - currentStatus.wco_z;
    currentStatus.wpos_a = currentStatus.mpos_a - currentStatus.wco_a;
    
    // Parse work position directly (WPos:x,y,z,a) - rarely sent, but handle it
    const char* wpos = strstr(message, "WPos:");
    if (wpos) {
        // Try parsing 4 values (X,Y,Z,A), but allow 3 values (X,Y,Z) for machines without A-axis
        int parsed = sscanf(wpos + 5, "%f,%f,%f,%f",
                           &currentStatus.wpos_x, &currentStatus.wpos_y,
                           &currentStatus.wpos_z, &currentStatus.wpos_a);
        if (parsed == 3) {
            // Only 3 axes parsed - machine doesn't have A-axis, set to 0
            currentStatus.wpos_a = 0.0f;
        }
    }
    
    // Parse feed and spindle (FS:feed,spindle)
    const char* fs = strstr(message, "FS:");
    if (fs) {
        sscanf(fs + 3, "%f,%f", &currentStatus.feed_rate, &currentStatus.spindle_speed);
        LOG_PRINTF("[FluidNC] Parsed FS: feed=%.0f, spindle=%.0f\n", 
                      currentStatus.feed_rate, currentStatus.spindle_speed);
    }
    
    // Parse overrides (Ov:feed,rapid,spindle)
    const char* ov = strstr(message, "Ov:");
    if (ov) {
        sscanf(ov + 3, "%f,%f,%f", &currentStatus.feed_override, &currentStatus.rapid_override, &currentStatus.spindle_override);
        LOG_PRINTF("[FluidNC] Parsed Ov: feed=%.0f%%, rapid=%.0f%%, spindle=%.0f%%\n", 
                      currentStatus.feed_override, currentStatus.rapid_override, currentStatus.spindle_override);
    }

    // Parse pin states (Pn:XYZA P) - field only present when pins are active
    currentStatus.pin_limit_x = false;
    currentStatus.pin_limit_y = false;
    currentStatus.pin_limit_z = false;
    currentStatus.pin_limit_a = false;
    currentStatus.pin_probe   = false;
    const char* pn = strstr(message, "Pn:");
    if (pn) {
        const char* p = pn + 3;
        while (*p && *p != '|' && *p != '>') {
            switch (*p) {
                case 'X': currentStatus.pin_limit_x = true; break;
                case 'Y': currentStatus.pin_limit_y = true; break;
                case 'Z': currentStatus.pin_limit_z = true; break;
                case 'A': currentStatus.pin_limit_a = true; break;
                case 'P': currentStatus.pin_probe   = true; break;
            }
            p++;
        }
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
            
            LOG_PRINTF("[FluidNC] SD Progress: %.1f%% - %s (Elapsed: %lums)\n",
                          percent, currentStatus.sd_filename, currentStatus.sd_elapsed_ms);
        }
    } else {
        // No SD: field means not printing from SD
        if (currentStatus.is_sd_printing) {
            LOG_PRINTLN("[FluidNC] SD file completed or stopped");
        }
        currentStatus.is_sd_printing = false;
        currentStatus.sd_percent = 0;
        currentStatus.sd_start_time_ms = 0;
        currentStatus.sd_elapsed_ms = 0;
        currentStatus.sd_filename[0] = '\0';
    }
    
    // Parse modal states (Pn:, WCO:, etc.)
    // Note: Full parser state might come in separate $G response
    
    LOG_PRINTF("[FluidNC] Status: State=%d, MPos=(%.3f,%.3f,%.3f,%.3f), WPos=(%.3f,%.3f,%.3f,%.3f)\n",
                  currentStatus.state,
                  currentStatus.mpos_x, currentStatus.mpos_y, currentStatus.mpos_z, currentStatus.mpos_a,
                  currentStatus.wpos_x, currentStatus.wpos_y, currentStatus.wpos_z, currentStatus.wpos_a);
}

void FluidNCClient::parseRealtimeFeedback(const char* message) {
    // Handle realtime feedback messages like [MSG:...], [G92:...], [PRB:...], etc.
    LOG_PRINTF("[FluidNC] Feedback: %s\n", message);
    
    // Check for probe result message: [PRB:x,y,z:success]
    // Example: [PRB:151.000,149.000,-137.505:1] (success=1) or [PRB:0.000,0.000,0.000:0] (failure=0)
    if (strncmp(message, "[PRB:", 5) == 0) {
        float x, y, z;
        int success;
        if (sscanf(message + 5, "%f,%f,%f:%d", &x, &y, &z, &success) == 4) {
            // Update probe tab result display with coordinates
            UITabControlProbe::updateResult(x, y, z, success != 0);
            
            LOG_PRINTF("[FluidNC] Probe %s at (%.3f, %.3f, %.3f)\n", 
                         success ? "SUCCESS" : "FAILED", x, y, z);
        }
    }
    
    // Parse FluidNC firmware version from $Build/Info response
    // Example: [VER:3.9 FluidNC v3.9.5:]
    if (strncmp(message, "[VER:", 5) == 0) {
        const char* v = strstr(message + 5, " v");
        if (v) {
            v += 2;  // skip " v"
            const char* end = strchr(v, ':');
            if (!end) end = strchr(v, ']');
            if (!end) end = v + strlen(v);
            size_t len = (size_t)(end - v);
            if (len >= sizeof(currentStatus.fluidnc_version)) len = sizeof(currentStatus.fluidnc_version) - 1;
            strncpy(currentStatus.fluidnc_version, v, len);
            currentStatus.fluidnc_version[len] = '\0';
            LOG_PRINTF("[FluidNC] Firmware version: %s\n", currentStatus.fluidnc_version);
        }
    }

    // Check for auto-report confirmation message
    if (strstr(message, "websocket auto report interval set") != nullptr) {
        LOG_PRINTLN("[FluidNC] ✓ Auto-report confirmed - automatic reporting enabled");
        autoReportingEnabled = true;
        
        if (!currentStatus.is_connected) {
            currentStatus.is_connected = true;
            LOG_PRINTLN("[FluidNC] ✓ Connection established");
            
            // Hide connecting popup
            UICommon::hideConnectingPopup();
            
            // Also hide error dialog if showing (connection succeeded after error)
            UICommon::hideConnectionErrorDialog();
        }
    }
    
    // Update last_message for [MSG:INFO...], [MSG:WARN...] and [MSG:ERR...] messages
    if (strncmp(message, "[MSG:INFO", 9) == 0 ||
        strncmp(message, "[MSG:WARN", 9) == 0 ||
        strncmp(message, "[MSG:ERR",  8) == 0) {
        const char* end = strchr(message, ']');
        if (end) {
            // Skip past "[MSG:LEVEL:" by finding the second colon
            const char* first_colon = strchr(message + 1, ':');
            const char* content = first_colon ? strchr(first_colon + 1, ':') : nullptr;
            if (content && content < end) {
                content++;  // skip the colon after the level tag
            } else {
                content = end;  // nothing to extract
            }

            // Trim leading spaces
            while (*content == ' ' && content < end) content++;

            // Strip "MSG" prefix + one extra character, then trim spaces
            if (content + 3 < end && strncmp(content, "MSG", 3) == 0) {
                content += 4;
                while (*content == ' ' && content < end) content++;
            }
            // Strip "PRINT" prefix + one extra character, then trim spaces
            else if (content + 5 < end && strncmp(content, "PRINT", 5) == 0) {
                content += 6;
                while (*content == ' ' && content < end) content++;
            }

            size_t len = (content < end) ? (size_t)(end - content) : 0;
            if (len >= sizeof(currentStatus.last_message)) {
                len = sizeof(currentStatus.last_message) - 1;
            }
            strncpy(currentStatus.last_message, content, len);
            currentStatus.last_message[len] = '\0';
        }
    }
}

void FluidNCClient::parseGCodeState(const char* message) {
    // Example: [GC:G0 G54 G17 G21 G90 G94 M5 M9 T0 F0 S0]
    // Parse modal states from GCode parser state report
    
    LOG_PRINTF("[FluidNC] GCode State: %s\n", message);
    
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
    
    // Parse coolant state (M7=mist, M8=flood, M9=off; both M7 and M8 can be active simultaneously)
    bool hasMist = strstr(ptr, "M7 ") != nullptr;
    bool hasFlood = strstr(ptr, "M8 ") != nullptr;
    if (hasMist && hasFlood) strcpy(currentStatus.modal_coolant, "M7 M8");
    else if (hasMist) strcpy(currentStatus.modal_coolant, "M7");
    else if (hasFlood) strcpy(currentStatus.modal_coolant, "M8");
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
    
    LOG_PRINTF("[FluidNC] Parsed modals: Motion=%s, WCS=%s, Plane=%s, Units=%s, Distance=%s, Spindle=%s, Coolant=%s, Tool=%s, Feed=%.0f, SpindleSpeed=%.0f\n",
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

#ifdef HARDWARE_ADVANCE
void FluidNCClient::processUartLine(char* line) {
    const char* payload = line;

    // Fire callbacks
    if (messageCallback) {
        messageCallback(payload);
    }
    if (terminalCallback) {
        terminalCallback(payload);
    }

    // Route to same parsers as WebSocket path
    if (payload[0] == '<') {
        parseStatusReport(payload);
    } else if (strncmp(payload, "[GC:", 4) == 0) {
        parseGCodeState(payload);
    } else if (payload[0] == '[') {
        parseRealtimeFeedback(payload);
    } else if (strncmp(payload, "error:", 6) == 0 || strncmp(payload, "ALARM:", 6) == 0) {
        strncpy(currentStatus.last_message, payload, sizeof(currentStatus.last_message) - 1);
        currentStatus.last_message[sizeof(currentStatus.last_message) - 1] = '\0';
    }
}
#else
void FluidNCClient::processUartLine(char* line) { (void)line; }
#endif

void FluidNCClient::attemptEnableAutoReporting() {
    LOG_PRINTLN("[FluidNC] Attempting to enable automatic reporting (250ms)");
#ifdef HARDWARE_ADVANCE
    if (activeStream != nullptr) {
        activeStream->print("$Report/Interval=250\n");
    } else
#endif
    {
        webSocket.send("$Report/Interval=250\n");
    }
    
    autoReportingAttempted = true;
    autoReportingEnabled = false;  // Will be set true when we receive status
    lastAutoReportAttemptMs = millis();
}

void FluidNCClient::performFallbackPolling() {
    // If auto-reporting is enabled, no need to poll
    if (autoReportingEnabled) {
        return;
    }
    
    uint32_t now = millis();
    
    // Send status poll ("?") every 1 second
    if (now - lastPollingMs >= 1000) {
        LOG_PRINTLN("[FluidNC] Fallback polling: sending '?'");
        webSocket.send("?");
        lastPollingMs = now;
    }
    
    // Send GCode parser state poll ("$G") every 10 seconds
    if (now - lastGCodePollMs >= 10000) {
        LOG_PRINTLN("[FluidNC] Fallback polling: sending '$G'");
        webSocket.send("$G\n");
        lastGCodePollMs = now;
    }
}

