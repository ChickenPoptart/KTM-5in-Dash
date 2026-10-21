#include "style.h"

#include "brightness.h"

// Every palette colour at full brightness, and the variable it's drawn from
struct PaletteEntry { uint16_t* colour; uint16_t full; };

uint16_t COL_BG, COL_PANEL, COL_PANEL_EDGE, COL_RING, COL_RING_GLOW, COL_TRACK, COL_TICK,
    COL_TICK_LABEL, COL_TEXT, COL_LABEL, COL_ACCENT, COL_NEUTRAL, COL_IDLE, COL_GREEN, COL_AMBER,
    COL_RED, COL_SPEED;

static const PaletteEntry PALETTE[] = {
    {&COL_BG, rgb(0x06, 0x0A, 0x14)},
    {&COL_PANEL, rgb(0x0E, 0x14, 0x24)},
    {&COL_PANEL_EDGE, rgb(0x22, 0x2C, 0x48)},
    {&COL_RING, rgb(0x4A, 0x6C, 0xF7)},
    {&COL_RING_GLOW, rgb(0x14, 0x1E, 0x4A)},
    {&COL_TRACK, rgb(0x1A, 0x20, 0x30)},
    {&COL_TICK, rgb(0x8A, 0x94, 0xA8)},
    {&COL_TICK_LABEL, rgb(0xA8, 0xB0, 0xC0)},
    {&COL_TEXT, rgb(0xFF, 0xFF, 0xFF)},
    {&COL_LABEL, rgb(0x70, 0x7C, 0x94)},
    {&COL_ACCENT, rgb(0x4A, 0x9C, 0xFF)},
    {&COL_NEUTRAL, rgb(0x3C, 0xE0, 0x6C)},
    {&COL_IDLE, rgb(0x00, 0xD8, 0xFF)},
    {&COL_GREEN, rgb(0x3C, 0xE0, 0x6C)},
    {&COL_AMBER, rgb(0xFF, 0xB0, 0x00)},
    {&COL_RED, rgb(0xFF, 0x2A, 0x2A)},
    {&COL_SPEED, rgb(0x4A, 0x9C, 0xFF)},  // same as the accent
};

static int current = 100;

void setBrightness(int percent) {
    current = percent;
    for (const PaletteEntry& e : PALETTE) *e.colour = dimmed565(e.full, percent);
}

int brightness() { return current; }

// Full brightness until the saved setting is loaded
static struct InitPalette { InitPalette() { setBrightness(100); } } initPalette;
