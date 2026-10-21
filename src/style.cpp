#include "style.h"

#include "brightness.h"

// Every palette colour in each theme at full brightness, and the variable
// it's drawn from
struct PaletteEntry { uint16_t* colour; uint16_t modern; uint16_t retro; };

uint16_t COL_BG, COL_PANEL, COL_PANEL_EDGE, COL_RING, COL_RING_GLOW, COL_TRACK, COL_TICK,
    COL_TICK_LABEL, COL_TEXT, COL_LABEL, COL_ACCENT, COL_NEUTRAL, COL_IDLE, COL_GREEN, COL_AMBER,
    COL_RED, COL_SPEED;

// Retro is KTM orange (#FF6600) on a warm black. Its shift light still
// steps up: brighter yellow when it's nearly time, then red.
static const PaletteEntry PALETTE[] = {
    //                 modern                      retro
    {&COL_BG,          rgb(0x06, 0x0A, 0x14), rgb(0x0A, 0x05, 0x02)},
    {&COL_PANEL,       rgb(0x0E, 0x14, 0x24), rgb(0x16, 0x0A, 0x03)},
    {&COL_PANEL_EDGE,  rgb(0x22, 0x2C, 0x48), rgb(0x48, 0x20, 0x06)},
    {&COL_RING,        rgb(0x4A, 0x6C, 0xF7), rgb(0xFF, 0x66, 0x00)},
    {&COL_RING_GLOW,   rgb(0x14, 0x1E, 0x4A), rgb(0x40, 0x18, 0x00)},
    {&COL_TRACK,       rgb(0x1A, 0x20, 0x30), rgb(0x30, 0x15, 0x04)},  // retro: unlit segments
    {&COL_TICK,        rgb(0x8A, 0x94, 0xA8), rgb(0xC0, 0x55, 0x10)},
    {&COL_TICK_LABEL,  rgb(0xA8, 0xB0, 0xC0), rgb(0xFF, 0x80, 0x20)},
    {&COL_TEXT,        rgb(0xFF, 0xFF, 0xFF), rgb(0xFF, 0x8C, 0x20)},
    {&COL_LABEL,       rgb(0x70, 0x7C, 0x94), rgb(0xA8, 0x4C, 0x0C)},
    {&COL_ACCENT,      rgb(0x4A, 0x9C, 0xFF), rgb(0xFF, 0x66, 0x00)},
    {&COL_NEUTRAL,     rgb(0x3C, 0xE0, 0x6C), rgb(0xFF, 0xC8, 0x30)},
    {&COL_IDLE,        rgb(0x00, 0xD8, 0xFF), rgb(0xFF, 0x66, 0x00)},
    {&COL_GREEN,       rgb(0x3C, 0xE0, 0x6C), rgb(0xFF, 0xC8, 0x30)},  // retro: "nearly time" yellow
    {&COL_AMBER,       rgb(0xFF, 0xB0, 0x00), rgb(0xFF, 0x99, 0x00)},
    {&COL_RED,         rgb(0xFF, 0x2A, 0x2A), rgb(0xFF, 0x22, 0x10)},
    {&COL_SPEED,       rgb(0x4A, 0x9C, 0xFF), rgb(0xFF, 0x66, 0x00)},  // same as the accent
};

static int current = 100;
static Theme currentTheme = THEME_MODERN;

static void applyPalette() {
    for (const PaletteEntry& e : PALETTE) {
        *e.colour = dimmed565(currentTheme == THEME_RETRO ? e.retro : e.modern, current);
    }
}

void setBrightness(int percent) {
    current = percent;
    applyPalette();
}

int brightness() { return current; }

void setTheme(Theme t) {
    currentTheme = t;
    applyPalette();
}

Theme theme() { return currentTheme; }

// Modern, full brightness until the saved settings are loaded
static struct InitPalette { InitPalette() { applyPalette(); } } initPalette;
