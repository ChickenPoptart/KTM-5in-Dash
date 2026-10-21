#include "icons.h"

#include <math.h>

// A filled rectangle len x width centred on (cx, cy), turned by deg
static void rotatedRect(lgfx::LGFXBase& g, float cx, float cy, float len, float width, float deg,
                        uint16_t color) {
    float a = deg * (float)M_PI / 180.0f;
    float ux = cosf(a) * len / 2, uy = sinf(a) * len / 2;      // along
    float vx = -sinf(a) * width / 2, vy = cosf(a) * width / 2;  // across
    float x0 = cx - ux - vx, y0 = cy - uy - vy;
    float x1 = cx + ux - vx, y1 = cy + uy - vy;
    float x2 = cx + ux + vx, y2 = cy + uy + vy;
    float x3 = cx - ux + vx, y3 = cy - uy + vy;
    g.fillTriangle(x0, y0, x1, y1, x2, y2, color);
    g.fillTriangle(x0, y0, x2, y2, x3, y3, color);
}

// A short horizontal wave, like the water lines under a coolant symbol
static void wave(lgfx::LGFXBase& g, int x0, int x1, int y, uint16_t color) {
    float px = x0, py = y;
    for (int x = x0 + 2; x <= x1; x += 2) {
        float wy = y + 2.0f * sinf((x - x0) * (float)M_PI / 5.0f);
        g.drawWideLine(px, py, x, wy, 1.2f, color);
        px = x;
        py = wy;
    }
}

void iconCoolant(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    g.fillSmoothRoundRect(x - 3, y - 17, 7, 22, 3, color);  // stem
    g.fillSmoothCircle(x, y + 4, 6, color);                 // bulb
    for (int i = 0; i < 3; i++) {                           // scale marks
        int ty = y - 13 + i * 5;
        g.drawWideLine(x + 6, ty, x + 11, ty, 1.0f, color);
    }
    wave(g, x - 16, x - 6, y + 13, color);
    wave(g, x + 6, x + 16, y + 13, color);
    wave(g, x - 16, x + 16, y + 19, color);
}

void iconMountain(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    g.fillTriangle(x - 18, y + 12, x - 4, y - 12, x + 10, y + 12, color);
    g.fillTriangle(x - 2, y + 12, x + 8, y - 4, x + 18, y + 12, color);
}

void iconPhone(lgfx::LGFXBase& g, int x, int y, uint16_t color, uint16_t bg) {
    g.fillSmoothRoundRect(x - 9, y - 15, 18, 30, 4, color);
    g.fillRect(x - 6, y - 11, 12, 20, bg);  // screen
    g.fillCircle(x, y + 12, 1, bg);         // home dot
}

// A fuel pump with its hose, about 22 px tall
void iconFuel(lgfx::LGFXBase& g, int x, int y, uint16_t color, uint16_t bg) {
    g.fillSmoothRoundRect(x - 8, y - 11, 13, 22, 2, color);  // pump
    g.fillRect(x - 6, y - 8, 9, 6, bg);                      // dial window
    g.fillRect(x - 10, y + 9, 17, 3, color);                 // base
    g.drawWideLine(x + 5, y - 6, x + 9, y - 3, 1.2f, color); // hose
    g.drawWideLine(x + 9, y - 3, x + 9, y + 6, 1.2f, color);
    g.drawWideLine(x + 9, y + 6, x + 6, y + 6, 1.2f, color);
    g.fillRect(x + 7, y - 10, 4, 5, color);                  // nozzle
}

// Like the satellite in the Pixel's satellite-messaging icon: a body with a
// solar panel either side, tilted 45 degrees, sending signal waves
void iconSatellite(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    const float bx = x - 3, by = y - 3;  // body, up and left of centre
    rotatedRect(g, bx, by, 9, 9, 45, color);
    rotatedRect(g, bx - 9, by + 9, 11, 9, -45, color);   // panel, down-left
    rotatedRect(g, bx + 9, by - 9, 11, 9, -45, color);   // panel, up-right
    g.drawWideLine(bx - 4, by + 4, bx - 6, by + 6, 1.0f, color);
    g.drawWideLine(bx + 4, by - 4, bx + 6, by - 6, 1.0f, color);
    g.fillArc(bx, by, 12, 10, 20, 70, color);           // signal, down-right
    g.fillArc(bx, by, 18, 16, 20, 70, color);
}

void iconPrevious(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    g.fillRect(x - 12, y - 10, 4, 20, color);
    g.fillTriangle(x - 8, y, x + 2, y - 10, x + 2, y + 10, color);
    g.fillTriangle(x + 2, y, x + 12, y - 10, x + 12, y + 10, color);
}

void iconNext(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    g.fillTriangle(x - 12, y - 10, x - 2, y, x - 12, y + 10, color);
    g.fillTriangle(x - 2, y - 10, x + 8, y, x - 2, y + 10, color);
    g.fillRect(x + 8, y - 10, 4, 20, color);
}

void iconPlay(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    g.fillTriangle(x - 7, y - 11, x + 11, y, x - 7, y + 11, color);
}

void iconPause(lgfx::LGFXBase& g, int x, int y, uint16_t color) {
    g.fillRect(x - 8, y - 10, 6, 20, color);
    g.fillRect(x + 2, y - 10, 6, 20, color);
}

void iconTurn(lgfx::LGFXBase& g, int x, int y, int direction, uint16_t color) {
    if (direction == 0) {
        g.fillRect(x - 6, y - 14, 13, 56, color);
        g.fillTriangle(x - 22, y - 12, x, y - 40, x + 22, y - 12, color);
        return;
    }
    int s = direction > 0 ? 1 : -1;  // mirror for a left turn
    int ax = x + 4 * s;              // centre of the bend
    g.fillRect(x - 16 * s - 6, y, 13, 42, color);  // up the road...
    if (s > 0) g.fillArc(ax, y, 26, 14, 180, 270, color);  // ...round the bend...
    else g.fillArc(ax, y, 26, 14, 270, 360, color);
    int hx0 = s > 0 ? ax : ax - 18;
    g.fillRect(hx0, y - 26, 18, 13, color);  // ...and off to the side
    int tip = ax + 40 * s, base = ax + 18 * s;
    g.fillTriangle(base, y - 40, tip, y - 20, base, y, color);
}
