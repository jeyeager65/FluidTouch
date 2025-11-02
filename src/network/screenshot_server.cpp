#include "network/screenshot_server.h"
#include "config.h"
#include "core/display_driver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

#if ENABLE_SCREENSHOT_SERVER

static WebServer server(80);
static bool wifi_connected = false;
static DisplayDriver* display_driver_instance = nullptr;
static uint16_t* screenshot_buffer = nullptr;

// Convert RGB565 to RGB888 for BMP format
static void rgb565_to_rgb888(uint16_t rgb565, uint8_t* r, uint8_t* g, uint8_t* b) {
    // LovyanGFX returns byte-swapped RGB565 data - swap bytes first
    uint16_t swapped = (rgb565 >> 8) | (rgb565 << 8);
    
    // Extract RGB components from RGB565 (5-6-5 bits)
    *r = ((swapped >> 11) & 0x1F) << 3;  // 5 bits red -> 8 bits
    *g = ((swapped >> 5) & 0x3F) << 2;   // 6 bits green -> 8 bits
    *b = (swapped & 0x1F) << 3;          // 5 bits blue -> 8 bits
    
    // Expand to full range by copying top bits to bottom bits
    *r |= (*r >> 5);
    *g |= (*g >> 6);
    *b |= (*b >> 5);
}

// Handle screenshot request
static void handleScreenshot() {
    Serial.println("Screenshot requested...");
    
    if (!display_driver_instance) {
        server.send(500, "text/plain", "Display driver not initialized");
        return;
    }
    
    // Flush LVGL to ensure display is up to date
    lv_refr_now(NULL);
    
    // Get screen dimensions
    const uint32_t width = SCREEN_WIDTH;
    const uint32_t height = SCREEN_HEIGHT;
    
    Serial.println("Taking screenshot using LovyanGFX readRect...");
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    
    // Allocate buffer directly in PSRAM if not already allocated (800*480*2 = 768,000 bytes)
    if (!screenshot_buffer) {
        screenshot_buffer = (uint16_t*)heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    }
    
    if (!screenshot_buffer) {
        Serial.println("Failed to allocate screenshot buffer in PSRAM");
        server.send(500, "text/plain", "Failed to allocate 768KB buffer in PSRAM");
        return;
    }
    
    Serial.println("Reading screen buffer...");
    
    // Read the entire screen directly from LovyanGFX frame buffer
    LGFX* lcd = display_driver_instance->getLCD();
    lcd->readRect(0, 0, width, height, screenshot_buffer);
    
    Serial.println("Converting to BMP...");
    
    // BMP file format
    // Header: 14 bytes + Info Header: 40 bytes = 54 bytes
    const uint32_t row_size = ((width * 3 + 3) / 4) * 4;  // BMP rows must be multiple of 4 bytes
    const uint32_t image_size = row_size * height;
    const uint32_t file_size = 54 + image_size;
    
    // Allocate buffer for BMP header + one row
    uint8_t* header = (uint8_t*)malloc(54);
    uint8_t* row_buffer = (uint8_t*)malloc(row_size);
    
    if (!header || !row_buffer) {
        free(header);
        free(row_buffer);
        server.send(500, "text/plain", "Memory allocation failed");
        return;
    }
    
    // BMP File Header (14 bytes)
    header[0] = 'B';
    header[1] = 'M';
    header[2] = file_size & 0xFF;
    header[3] = (file_size >> 8) & 0xFF;
    header[4] = (file_size >> 16) & 0xFF;
    header[5] = (file_size >> 24) & 0xFF;
    header[6] = 0;  // Reserved
    header[7] = 0;
    header[8] = 0;
    header[9] = 0;
    header[10] = 54;  // Offset to pixel data
    header[11] = 0;
    header[12] = 0;
    header[13] = 0;
    
    // BMP Info Header (40 bytes)
    header[14] = 40;  // Info header size
    header[15] = 0;
    header[16] = 0;
    header[17] = 0;
    header[18] = width & 0xFF;
    header[19] = (width >> 8) & 0xFF;
    header[20] = (width >> 16) & 0xFF;
    header[21] = (width >> 24) & 0xFF;
    header[22] = height & 0xFF;
    header[23] = (height >> 8) & 0xFF;
    header[24] = (height >> 16) & 0xFF;
    header[25] = (height >> 24) & 0xFF;
    header[26] = 1;  // Planes
    header[27] = 0;
    header[28] = 24;  // Bits per pixel (RGB888)
    header[29] = 0;
    header[30] = 0;  // Compression (0 = none)
    header[31] = 0;
    header[32] = 0;
    header[33] = 0;
    header[34] = image_size & 0xFF;
    header[35] = (image_size >> 8) & 0xFF;
    header[36] = (image_size >> 16) & 0xFF;
    header[37] = (image_size >> 24) & 0xFF;
    // Remaining bytes 38-53: resolution and palette (all zeros)
    for (int i = 38; i < 54; i++) header[i] = 0;
    
    // Send headers
    server.setContentLength(file_size);
    server.send(200, "image/bmp", "");
    
    // Send BMP header
    server.sendContent((const char*)header, 54);
    
    // Convert screenshot to BMP format and send
    // BMP is stored bottom-to-top, so start from last row
    for (int y = height - 1; y >= 0; y--) {
        memset(row_buffer, 0, row_size);
        
        for (uint32_t x = 0; x < width; x++) {
            uint16_t rgb565 = screenshot_buffer[y * width + x];
            uint8_t r, g, b;
            rgb565_to_rgb888(rgb565, &r, &g, &b);
            
            // BMP format is BGR
            row_buffer[x * 3 + 0] = b;
            row_buffer[x * 3 + 1] = g;
            row_buffer[x * 3 + 2] = r;
        }
        
        server.sendContent((const char*)row_buffer, row_size);
        
        // Yield to prevent watchdog timeout
        if (y % 10 == 0) {
            yield();
        }
    }
    
    free(header);
    free(row_buffer);
    
    Serial.println("Screenshot BMP sent!");
}

// Handle root page
static void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>FluidTouch Screenshot</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; text-align: center; margin: 20px; background: #1a1a1a; color: #fff; }";
    html += "h1 { color: #00AA88; }";
    html += "img { max-width: 100%; border: 2px solid #00AA88; margin: 20px 0; }";
    html += "button { background: #00AA88; color: white; border: none; padding: 15px 30px; ";
    html += "font-size: 18px; cursor: pointer; border-radius: 5px; margin: 10px; }";
    html += "button:hover { background: #008866; }";
    html += ".info { background: #2a2a2a; padding: 15px; border-radius: 5px; margin: 20px auto; max-width: 600px; }";
    html += "</style></head><body>";
    html += "<h1>FluidTouch Display</h1>";
    html += "<div class='info'>";
    html += "<p><strong>Display:</strong> " + String(SCREEN_WIDTH) + "x" + String(SCREEN_HEIGHT) + "</p>";
    html += "<p><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "</div>";
    html += "<button onclick='captureScreenshot()'>Capture Screenshot</button>";
    html += "<button onclick='location.reload()'>Refresh Page</button>";
    html += "<div id='imgContainer'></div>";
    html += "<script>";
    html += "function captureScreenshot() {";
    html += "  document.getElementById('imgContainer').innerHTML = '<p>Capturing screenshot...</p>';";
    html += "  var img = new Image();";
    html += "  img.onload = function() {";
    html += "    var link = document.createElement('a');";
    html += "    link.href = img.src;";
    html += "    link.download = 'fluidtouch_' + Date.now() + '.bmp';";
    html += "    document.getElementById('imgContainer').innerHTML = '';";
    html += "    document.getElementById('imgContainer').appendChild(img);";
    html += "    document.getElementById('imgContainer').appendChild(document.createElement('br'));";
    html += "    document.getElementById('imgContainer').appendChild(link);";
    html += "    link.textContent = 'Download Screenshot';";
    html += "    link.style.cssText = 'color: #00AA88; font-size: 18px; text-decoration: none;';";
    html += "  };";
    html += "  img.src = '/screenshot.bmp?t=' + Date.now();";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void ScreenshotServer::init(DisplayDriver* display_driver) {
    #if !ENABLE_SCREENSHOT_SERVER
    return;
    #endif
    
    // Store the display driver instance
    display_driver_instance = display_driver;
    
    Serial.println("\n=== Screenshot Server ===");
    
    // Check if WiFi is already connected (should be connected via machine config)
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Screenshot server disabled.");
        wifi_connected = false;
        return;
    }
    
    wifi_connected = true;
    Serial.println("WiFi already connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Access the screenshot server at: http://" + WiFi.localIP().toString());
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/screenshot.bmp", handleScreenshot);
    
    server.begin();
    Serial.println("Web server started");
}

void ScreenshotServer::handleClient() {
    #if !ENABLE_SCREENSHOT_SERVER
    return;
    #endif
    
    if (wifi_connected) {
        server.handleClient();
    }
}

bool ScreenshotServer::isConnected() {
    return wifi_connected && (WiFi.status() == WL_CONNECTED);
}

String ScreenshotServer::getIPAddress() {
    if (wifi_connected) {
        return WiFi.localIP().toString();
    }
    return "Not connected";
}

#else
// Stub implementations when server is disabled
void ScreenshotServer::init(DisplayDriver* display_driver) {}
void ScreenshotServer::handleClient() {}
bool ScreenshotServer::isConnected() { return false; }
String ScreenshotServer::getIPAddress() { return "Disabled"; }
#endif
