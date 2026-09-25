// Desktop SD card: the display's SD card is the folder <data dir>/sd.
// Delete or rename that folder to simulate "no card inserted".
#pragma once

#include "FS.h"
#include "SPI.h"

typedef enum { CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC, CARD_UNKNOWN } sdcard_type_t;

namespace fs {
class SDFS : public FS {
public:
    SDFS() : FS("sd") {}
    bool begin(uint8_t ssPin = 0, SPIClass &spi = SPI, uint32_t frequency = 4000000,
               const char *mountpoint = "/sd", uint8_t max_files = 5, bool format_if_empty = false);
    void end() {}
    sdcard_type_t cardType();
    uint64_t cardSize();
    uint64_t totalBytes();
    uint64_t usedBytes();
};
} // namespace fs

extern fs::SDFS SD;
