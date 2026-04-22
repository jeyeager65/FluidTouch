#include "network/xmodem_sender.h"

// XModem control characters
#define XMODEM_SOH  0x01   // Start of 128-byte block
#define XMODEM_EOT  0x04   // End of Transmission
#define XMODEM_ACK  0x06   // Acknowledge
#define XMODEM_NAK  0x15   // Negative Acknowledge
#define XMODEM_CAN  0x18   // Cancel
#define XMODEM_CRC  'C'    // Receiver ready for CRC mode
#define XMODEM_SUB  0x1A   // Pad byte for last block

#define XMODEM_BLOCK_SIZE   128
#define XMODEM_MAX_RETRIES  10

// CRC-16-CCITT (polynomial 0x1021, init 0x0000)
uint16_t XModemSender::calcCrc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

int XModemSender::readByte(Stream& serial, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (serial.available()) {
            return serial.read();
        }
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
    return -1; // timeout
}

void XModemSender::flushInput(Stream& serial, uint32_t drainMs) {
    uint32_t start = millis();
    while (millis() - start < drainMs) {
        if (serial.available()) {
            serial.read();
            start = millis(); // reset timer on each byte so we truly drain
        } else {
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }
}

bool XModemSender::sendFile(Stream& serial,
                             const char* remotePath,
                             Stream& fileStream,
                             size_t fileSize,
                             ProgressCallback onProgress) {
    // -----------------------------------------------------------------------
    // Step 1: Send FluidNC command to put it into XModem-receive mode
    // -----------------------------------------------------------------------
    char cmd[280];
    snprintf(cmd, sizeof(cmd), "$Xmodem/Receive=%s\n", remotePath);
    serial.print(cmd);
    serial.flush();
    Serial.printf("[XModem] Sent: %s", cmd);

    // -----------------------------------------------------------------------
    // Step 2: Drain any "ok" / MSG lines FluidNC echoes back before it starts
    //         sending 'C' characters (allow up to 1 second of echo traffic)
    // -----------------------------------------------------------------------
    flushInput(serial, 600);

    // -----------------------------------------------------------------------
    // Step 3: Wait for 'C' – the receiver's CRC-mode handshake.
    //         FluidNC sends 'C' every second until the sender responds.
    // -----------------------------------------------------------------------
    Serial.println("[XModem] Waiting for CRC handshake ('C')...");
    bool receiverReady = false;
    uint32_t waitStart = millis();
    while (millis() - waitStart < 30000) { // up to 30 s
        int b = readByte(serial, 1200);
        if (b == XMODEM_CRC) {
            Serial.println("[XModem] CRC handshake received");
            receiverReady = true;
            break;
        } else if (b == XMODEM_NAK) {
            // Receiver supports only checksum mode; we will still use CRC
            // packets – most implementations accept them regardless.
            Serial.println("[XModem] NAK handshake received (proceeding with CRC packets)");
            receiverReady = true;
            break;
        } else if (b >= 0) {
            // Ignore stray bytes (leftover echo text, etc.)
            Serial.printf("[XModem] Skipping byte: 0x%02X\n", (uint8_t)b);
        }
    }

    if (!receiverReady) {
        Serial.println("[XModem] ERROR: Timeout waiting for handshake");
        return false;
    }

    // -----------------------------------------------------------------------
    // Step 4: Send file in 128-byte blocks
    // -----------------------------------------------------------------------
    size_t totalBlocks = (fileSize + XMODEM_BLOCK_SIZE - 1) / XMODEM_BLOCK_SIZE;
    if (totalBlocks == 0) totalBlocks = 1;
    Serial.printf("[XModem] Sending %u blocks (%u bytes)\n", (unsigned)totalBlocks, (unsigned)fileSize);

    uint8_t seqNum = 1;
    size_t bytesSent = 0;
    uint8_t blockBuf[XMODEM_BLOCK_SIZE];
    // Packet layout: SOH(1) + seq(1) + ~seq(1) + data(128) + CRC_H(1) + CRC_L(1) = 133 bytes
    uint8_t packet[XMODEM_BLOCK_SIZE + 5];

    for (size_t blkIdx = 0; blkIdx < totalBlocks; blkIdx++) {
        // Read up to 128 bytes from the file stream
        size_t bytesRead = 0;
        while (bytesRead < XMODEM_BLOCK_SIZE) {
            int b = fileStream.read();
            if (b < 0) break; // EOF
            blockBuf[bytesRead++] = (uint8_t)b;
        }
        // Pad any remaining bytes with SUB (0x1A)
        memset(blockBuf + bytesRead, XMODEM_SUB, XMODEM_BLOCK_SIZE - bytesRead);

        uint16_t crc = calcCrc16(blockBuf, XMODEM_BLOCK_SIZE);
        packet[0] = XMODEM_SOH;
        packet[1] = seqNum;
        packet[2] = (uint8_t)(255u - seqNum);
        memcpy(packet + 3, blockBuf, XMODEM_BLOCK_SIZE);
        packet[3 + XMODEM_BLOCK_SIZE] = (uint8_t)(crc >> 8);
        packet[4 + XMODEM_BLOCK_SIZE] = (uint8_t)(crc & 0xFF);

        bool blockAcked = false;
        for (int retry = 0; retry < XMODEM_MAX_RETRIES && !blockAcked; retry++) {
            if (retry > 0) {
                Serial.printf("[XModem] Retry %d for block %u\n", retry, (unsigned)seqNum);
                flushInput(serial, 150); // drain garbage before resend
            }

            serial.write(packet, sizeof(packet));
            serial.flush();

            int response = readByte(serial, 5000);
            if (response == XMODEM_ACK) {
                blockAcked = true;
            } else if (response == XMODEM_NAK) {
                // Will retry
            } else if (response == XMODEM_CAN) {
                Serial.println("[XModem] CAN received – transfer cancelled by receiver");
                return false;
            } else if (response < 0) {
                Serial.println("[XModem] Timeout waiting for ACK");
            } else {
                Serial.printf("[XModem] Unexpected response 0x%02X\n", (uint8_t)response);
            }
        }

        if (!blockAcked) {
            Serial.printf("[XModem] Block %u failed after max retries – aborting\n", (unsigned)seqNum);
            // Send two CAN bytes to abort the receiver
            serial.write((uint8_t)XMODEM_CAN);
            serial.write((uint8_t)XMODEM_CAN);
            serial.flush();
            return false;
        }

        bytesSent += bytesRead;
        if (onProgress) {
            onProgress(bytesSent, fileSize);
        }

        seqNum++; // uint8_t wraps 255 → 0 automatically
    }

    // -----------------------------------------------------------------------
    // Step 5: End of Transmission
    // -----------------------------------------------------------------------
    Serial.println("[XModem] Sending EOT...");
    for (int eotTry = 0; eotTry < 5; eotTry++) {
        serial.write((uint8_t)XMODEM_EOT);
        serial.flush();
        int response = readByte(serial, 5000);
        if (response == XMODEM_ACK) {
            Serial.println("[XModem] Transfer complete");
            return true;
        }
        Serial.printf("[XModem] EOT response 0x%02X, retry %d\n",
                      response >= 0 ? (uint8_t)response : 0xFF, eotTry + 1);
    }

    Serial.println("[XModem] ERROR: EOT not acknowledged");
    return false;
}
