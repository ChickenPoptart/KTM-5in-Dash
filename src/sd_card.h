#pragma once

// The MicroSD card's filesystem: the ESP32-S3's SD controller in 1-bit mode
// (SD_MMC). The card slot is wired for SPI with chip select on the CH422G
// expander, which the SPI library can't drive; SD mode needs no chip
// select, only that line held high (D3).
#include <SD_MMC.h>
#define CARD SD_MMC
