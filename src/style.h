#pragma once

#include <stdint.h>

// Look of the 5" dash: every colour, size, zone and shift point in one
// place, so the design can be tuned without touching the drawing code.

// RGB565 from 8-bit RGB, so colours can be written as normal hex values
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ---- Palette ----
// The colours everything is drawn in, at the brightness picked on the
// brightness pop-up (see setBrightness). Full-brightness values are in
// style.cpp.
extern uint16_t COL_BG;          // near-black navy
extern uint16_t COL_PANEL;       // centre/bottom panels
extern uint16_t COL_PANEL_EDGE;
extern uint16_t COL_RING;        // outer ring blue
extern uint16_t COL_RING_GLOW;   // soft halo behind it
extern uint16_t COL_TRACK;       // unlit part of an arc
extern uint16_t COL_TICK;
extern uint16_t COL_TICK_LABEL;
extern uint16_t COL_TEXT;
extern uint16_t COL_LABEL;
extern uint16_t COL_ACCENT;      // icons, speed arc
extern uint16_t COL_NEUTRAL;     // "N" in green

// RPM zone colours: lit, and shown at quarter brightness on the unlit track
extern uint16_t COL_IDLE;
extern uint16_t COL_GREEN;
extern uint16_t COL_AMBER;
extern uint16_t COL_RED;
extern uint16_t COL_SPEED;

// The look: modern (blue, smooth arcs) or retro (KTM orange, segmented
// bars and seven-segment digits, like an 80s VFD/LCD dash). Redraw after.
enum Theme { THEME_MODERN, THEME_RETRO };
void setTheme(Theme t);
Theme theme();

// Screen brightness for night riding, 0-100 %. The backlight can only be
// on or off, so this redraws in darker colours. Redraw the screen after.
static const int BRIGHTNESS_LEVELS[] = {100, 70, 45, 25};
void setBrightness(int percent);
int brightness();

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
