#include "ui/upload_manager.h"
#include "config.h"
#include "network/fluidnc_client.h"
#include <SD.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

bool UploadManager::_uploading = false;

bool UploadManager::init() {
    Serial.println("[UploadManager] Initializing SD card...");
    
#ifdef HARDWARE_ADVANCE
    Serial.println("[UploadManager] Advance: SPI mode (MOSI=6, MISO=4, CLK=5, CS=0/GND)");
    // CS is GPIO 0 per Elecrow example, though physically tied to GND
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false)) {
#else
    Serial.println("[UploadManager] Basic: SPI mode (MOSI=11, MISO=13, CLK=12, CS=10)");
    SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 4000000, "/sd", 5, false)) {  // Use configured CS pin
#endif
        Serial.println("[UploadManager] SD Card mount failed");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[UploadManager] No SD card attached");
        return false;
    }
    
    Serial.printf("[UploadManager] SD Card Type: %s\n",
                 cardType == CARD_MMC ? "MMC" :
                 cardType == CARD_SD ? "SDSC" :
                 cardType == CARD_SDHC ? "SDHC" : "UNKNOWN");
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[UploadManager] SD Card Size: %lluMB\n", cardSize);
    
    return true;
}

bool UploadManager::uploadFile(const char* localPath, 
                               const char* filename,
                               ProgressCallback onProgress,
                               CompleteCallback onComplete) {
    if (_uploading) {
        Serial.println("[UploadManager] Upload already in progress");
        if (onComplete) onComplete(false, "Upload already in progress");
        return false;
    }
    
    _uploading = true;
    
    Serial.printf("[UploadManager] Starting upload of %s\n", localPath);
    
    // Open local file
    File file = SD.open(localPath);
    if (!file) {
        Serial.println("[UploadManager] Failed to open local file");
        _uploading = false;
        if (onComplete) onComplete(false, "Failed to open local file");
        return false;
    }
    
    size_t fileSize = file.size();
    Serial.printf("[UploadManager] File size: %d bytes\n", fileSize);
    
    // Support files up to 10MB
    if (fileSize > 10485760) {  // 10MB limit
        Serial.println("[UploadManager] File too large for upload");
        file.close();
        _uploading = false;
        if (onComplete) onComplete(false, "File too large (max 10MB)");
        return false;
    }
    
    // Allocate streaming buffer in PSRAM (16KB chunks)
    const size_t CHUNK_SIZE = 16384;
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(CHUNK_SIZE, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        Serial.println("[UploadManager] Failed to allocate buffer");
        file.close();
        _uploading = false;
        if (onComplete) onComplete(false, "Out of memory");
        return false;
    }
    
    // Get FluidNC IP and construct URL
    String machineIP = FluidNCClient::getMachineIP();
    if (machineIP.isEmpty()) {
        Serial.println("[UploadManager] FluidNC not connected");
        heap_caps_free(buffer);
        file.close();
        _uploading = false;
        if (onComplete) onComplete(false, "FluidNC not connected");
        return false;
    }
    
    // Resolve hostname if needed
    IPAddress serverIP;
    if (machineIP.indexOf('.') == -1) {
        // It's a hostname, try to resolve it
        if (!WiFi.hostByName(machineIP.c_str(), serverIP)) {
            Serial.printf("[UploadManager] Failed to resolve hostname: %s\n", machineIP.c_str());
            heap_caps_free(buffer);
            file.close();
            _uploading = false;
            if (onComplete) onComplete(false, "Failed to resolve hostname");
            return false;
        }
        machineIP = serverIP.toString();
        Serial.printf("[UploadManager] Resolved to IP: %s\n", machineIP.c_str());
    }
    
    // Build remote path - remove leading slash from filename if present
    String filenameStr = String(filename);
    if (filenameStr.startsWith("/")) {
        filenameStr = filenameStr.substring(1);
    }
    
    // FluidNC expects path as directory only, filename goes in form field name
    String destDir = String(FLUIDNC_UPLOAD_PATH);
    String fullPath = destDir + filenameStr;
    
    // Get current timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    
    String url = "http://" + machineIP + "/upload";
    Serial.printf("[UploadManager] Uploading to %s (path: %s)\n", url.c_str(), fullPath.c_str());
    
    // Build multipart boundary
    String boundary = "----WebKitFormBoundary" + String(random(0xffff), HEX);
    String contentType = "multipart/form-data; boundary=" + boundary;
    
    // Build multipart body parts
    // Part 1: path field (destination directory)
    String part1 = "--" + boundary + "\r\n";
    part1 += "Content-Disposition: form-data; name=\"path\"\r\n\r\n";
    part1 += destDir + "\r\n";
    
    // Part 2: file size field
    String sizeFieldName = fullPath + "S";
    String part2 = "--" + boundary + "\r\n";
    part2 += "Content-Disposition: form-data; name=\"" + sizeFieldName + "\"\r\n\r\n";
    part2 += String(fileSize) + "\r\n";
    
    // Part 3: timestamp field
    String timeFieldName = fullPath + "T";
    String part3 = "--" + boundary + "\r\n";
    part3 += "Content-Disposition: form-data; name=\"" + timeFieldName + "\"\r\n\r\n";
    part3 += String(timestamp) + "\r\n";
    
    // Part 4: file data header
    String part4 = "--" + boundary + "\r\n";
    part4 += "Content-Disposition: form-data; name=\"myfiles\"; filename=\"" + fullPath + "\"\r\n";
    part4 += "Content-Type: application/octet-stream\r\n\r\n";
    
    String footer = "\r\n--" + boundary + "--\r\n";
    
    size_t totalSize = part1.length() + part2.length() + part3.length() + part4.length() + fileSize + footer.length();
    Serial.printf("[UploadManager] Total multipart size: %d bytes\n", totalSize);
    
    // Report initial progress
    if (onProgress) {
        onProgress(0, fileSize);
    }
    
    // Create WiFi client and connect directly
    WiFiClient client;
    if (!client.connect(machineIP.c_str(), 80)) {
        Serial.println("[UploadManager] Connection failed");
        heap_caps_free(buffer);
        file.close();
        _uploading = false;
        if (onComplete) onComplete(false, "Connection failed");
        return false;
    }
    
    Serial.println("[UploadManager] Connected, sending request...");
    
    // Send HTTP headers
    client.print("POST /upload HTTP/1.1\r\n");
    client.print("Host: " + machineIP + "\r\n");
    client.print("Content-Type: " + contentType + "\r\n");
    client.print("Content-Length: " + String(totalSize) + "\r\n");
    client.print("Connection: close\r\n\r\n");
    
    // Send multipart parts
    client.print(part1);
    client.print(part2);
    client.print(part3);
    client.print(part4);
    
    Serial.println("[UploadManager] Headers sent, streaming file...");
    
    // Stream file in chunks
    size_t bytesUploaded = 0;
    size_t lastReportedPercent = 0;
    bool success = true;
    
    while (file.available()) {
        size_t bytesRead = file.read(buffer, CHUNK_SIZE);
        size_t bytesWritten = client.write(buffer, bytesRead);
        
        if (bytesWritten != bytesRead) {
            Serial.println("[UploadManager] Write error");
            success = false;
            break;
        }
        
        bytesUploaded += bytesWritten;
        
        // Report progress every 1% to provide smooth updates
        size_t currentPercent = (bytesUploaded * 100) / fileSize;
        if (onProgress && (currentPercent > lastReportedPercent || bytesUploaded == fileSize)) {
            onProgress(bytesUploaded, fileSize);
            lastReportedPercent = currentPercent;
            Serial.printf("[UploadManager] Progress: %d%% (%d/%d bytes)\n", currentPercent, bytesUploaded, fileSize);
        }
        
        // Small delay for network and watchdog
        delay(1);
    }
    
    file.close();
    
    // Send footer
    if (success) {
        client.print(footer);
        Serial.println("[UploadManager] All data sent, waiting for response...");
    }
    
    // Get response
    int httpCode = -1;
    if (success) {
        unsigned long timeout = millis();
        while (client.available() == 0) {
            if (millis() - timeout > 10000) {
                Serial.println("[UploadManager] Response timeout");
                success = false;
                break;
            }
            delay(10);
        }
        
        if (success) {
            String response = client.readStringUntil('\n');
            Serial.printf("[UploadManager] Response: %s\n", response.c_str());
            
            if (response.indexOf("200") >= 0) httpCode = 200;
            else if (response.indexOf("201") >= 0) httpCode = 201;
            else {
                Serial.println("[UploadManager] Upload failed - bad response");
                success = false;
                httpCode = 500;
            }
        }
    }
    
    client.stop();
    
    Serial.printf("[UploadManager] HTTP response code: %d\n", httpCode);
    
    if (success && (httpCode == 200 || httpCode == 201)) {
        Serial.println("[UploadManager] Upload successful");
    } else if (httpCode < 0) {
        Serial.printf("[UploadManager] Upload failed - connection error (code %d)\n", httpCode);
    } else {
        Serial.printf("[UploadManager] Upload failed - server error (code %d)\n", httpCode);
    }
    
    // Cleanup
    heap_caps_free(buffer);
    _uploading = false;
    
    if (onComplete) {
        if (success && (httpCode == 200 || httpCode == 201)) {
            onComplete(true, "Upload successful");
        } else if (httpCode < 0) {
            onComplete(false, "Connection error");
        } else {
            onComplete(false, "Server error");
        }
    }
    
    Serial.printf("[UploadManager] Upload %s\n", success ? "completed" : "failed");
    return success;
}



bool UploadManager::isUploading() {
    return _uploading;
}
