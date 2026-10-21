#pragma once

#include <stdint.h>

// Look of the 5" dash: every colour, size, zone and shift point in one
// place, so the design can be tuned without touching the drawing code.

// RGB565 from 8-bit RGB, so colours can be written as normal hex values
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ---- Palette ----
constexpr uint16_t COL_BG = rgb(0x06, 0x0A, 0x14);          // near-black navy
constexpr uint16_t COL_PANEL = rgb(0x0E, 0x14, 0x24);       // centre/bottom panels
constexpr uint16_t COL_PANEL_EDGE = rgb(0x22, 0x2C, 0x48);
constexpr uint16_t COL_RING = rgb(0x4A, 0x6C, 0xF7);        // outer ring blue
constexpr uint16_t COL_RING_GLOW = rgb(0x14, 0x1E, 0x4A);   // soft halo behind it
constexpr uint16_t COL_TRACK = rgb(0x1A, 0x20, 0x30);       // unlit part of an arc
constexpr uint16_t COL_TICK = rgb(0x8A, 0x94, 0xA8);
constexpr uint16_t COL_TICK_LABEL = rgb(0xA8, 0xB0, 0xC0);
constexpr uint16_t COL_TEXT = rgb(0xFF, 0xFF, 0xFF);
constexpr uint16_t COL_LABEL = rgb(0x70, 0x7C, 0x94);
constexpr uint16_t COL_ACCENT = rgb(0x4A, 0x9C, 0xFF);      // icons, speed arc
constexpr uint16_t COL_NEUTRAL = rgb(0x3C, 0xE0, 0x6C);     // "N" in green

// RPM zone colours: lit, and shown at quarter brightness on the unlit track
constexpr uint16_t COL_IDLE = rgb(0x00, 0xD8, 0xFF);
constexpr uint16_t COL_GREEN = rgb(0x3C, 0xE0, 0x6C);
constexpr uint16_t COL_AMBER = rgb(0xFF, 0xB0, 0x00);
constexpr uint16_t COL_RED = rgb(0xFF, 0x2A, 0x2A);
constexpr uint16_t COL_SPEED = COL_ACCENT;

// ---- RPM gauge ----
constexpr int RPM_MAX = 10000;          // top of the scale
constexpr int RPM_GREEN_FROM = 3000;    // below this: idle (cyan)
constexpr int RPM_AMBER_FROM = 7000;
constexpr int RPM_RED_FROM = 8200;      // redline zone up to the limiter

// ---- Two-stage shift light (to be tuned from ride logs) ----
constexpr int SHIFT_SOON_RPM = 7200;    // green: about time to shift
constexpr int SHIFT_NOW_RPM = 7800;     // red, flashing: shift now
constexpr int SHIFT_FLASH_MS = 90;      // red flash half-period

// ---- Speed gauge ----
constexpr int SPEED_MAX_MPH = 100;
constexpr int SPEED_TICK_STEP = 20;     // labelled ticks every 20 mph

// ---- Layout (800 x 480) ----
constexpr int TOP_STRIP_Y = 34;         // centre line of the top strip
constexpr int GAUGE_SIZE = 300;         // each gauge's square, in pixels
constexpr int GAUGE_Y = 70;             // top edge of both gauges
constexpr int RPM_GAUGE_X = 16;
constexpr int SPEED_GAUGE_X = 800 - 16 - GAUGE_SIZE;
constexpr int CENTER_PANEL_X = RPM_GAUGE_X + GAUGE_SIZE + 6;
constexpr int CENTER_PANEL_W = SPEED_GAUGE_X - 6 - CENTER_PANEL_X;
constexpr int BOTTOM_STRIP_Y = 388;     // top of the music / ETA strip

// Gauge geometry, from each gauge's centre
constexpr int ARC_START_DEG = 135;      // 0 = 3 o'clock, clockwise
constexpr int ARC_SWEEP_DEG = 270;
constexpr int ARC_OUTER = 112;          // the coloured band
constexpr int ARC_WIDTH = 16;
constexpr int TICK_LABEL_RADIUS = 133;  // numbers sit between ticks and ring
constexpr int RING_RADIUS = 146;        // the thin outer ring
constexpr int CENTRE_RADIUS = 93;       // the big numbers stay inside this
