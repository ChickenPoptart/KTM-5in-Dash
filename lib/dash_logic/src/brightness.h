#pragma once

#include <stdint.h>

// An RGB565 colour at `percent` of its brightness (each channel scaled,
// rounded down). The board's backlight can only be on or off, so dimming
// the screen for night riding means drawing everything darker.
inline uint16_t dimmed565(uint16_t c, int percent) {
    uint16_t r = (c >> 11) * percent / 100;
    uint16_t g = ((c >> 5) & 0x3F) * percent / 100;
    uint16_t b = (c & 0x1F) * percent / 100;
    return (r << 11) | (g << 5) | b;
}
