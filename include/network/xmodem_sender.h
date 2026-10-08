#ifndef XMODEM_SENDER_H
#define XMODEM_SENDER_H

#include <Arduino.h>
#include <functional>

// XModem-CRC sender.
// Implements the XModem protocol (CRC-16-CCITT variant) used by FluidNC's
// $Xmodem/Receive=<path> command.  Must be called from a dedicated FreeRTOS
// task so it can block waiting for each ACK without stalling the LVGL loop.
class XModemSender {
public:
    using ProgressCallback = std::function<void(size_t bytesSent, size_t totalBytes)>;

    // Sends the $Xmodem/Receive=<remotePath> command then transfers the open
    // file using XModem-CRC.  The file must be opened for reading and
    // positioned at byte 0.  fileSize must match the actual file size.
    // Returns true on success.
    static bool sendFile(Stream& serial,
                         const char* remotePath,
                         Stream& fileStream,
                         size_t fileSize,
                         ProgressCallback onProgress = nullptr);

private:
    static uint16_t calcCrc16(const uint8_t* data, size_t len);
    // Reads one byte with a timeout.  Returns -1 on timeout.
    static int readByte(Stream& serial, uint32_t timeoutMs);
    // Drains all incoming bytes for drainMs milliseconds.
    static void flushInput(Stream& serial, uint32_t drainMs);
};

#endif // XMODEM_SENDER_H
