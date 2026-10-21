#include "nav_icons.h"

#include <math.h>
#include "dash_fonts.h"

// Every arrow is drawn for the right-hand version; left ones are mirrored.
// Coordinates are from the icon's centre, y down, within about +-48.

static const float ROAD = 7;        // half the width of a road
static const float HEAD_LEN = 26;   // arrowhead length
static const float HEAD_HALF = 17;  // arrowhead half-width at its base

struct Pen {
    lgfx::LGFXBase& g;
    int cx, cy, side;  // side -1 mirrors left to right
    float X(float x) const { return cx + side * x; }
    float Y(float y) const { return cy + y; }

    // A road through these points; the round ends make smooth joints
    void road(const float (*p)[2], int n, uint16_t color) const {
        for (int i = 0; i + 1 < n; i++) {
            g.drawWideLine(X(p[i][0]), Y(p[i][1]), X(p[i + 1][0]), Y(p[i + 1][1]), ROAD, color);
        }
    }

    // An arrowhead with its tip at (x, y), pointing along (dx, dy). There's
    // no smooth triangle in LovyanGFX, so it's filled, then its edges are
    // traced with thin anti-aliased lines.
    void head(float x, float y, float dx, float dy, uint16_t color) const {
        float len = sqrtf(dx * dx + dy * dy);
        dx /= len;
        dy /= len;
        float bx = x - dx * HEAD_LEN, by = y - dy * HEAD_LEN;
        float nx = -dy * HEAD_HALF, ny = dx * HEAD_HALF;
        float px[3] = {X(x), X(bx + nx), X(bx - nx)}, py[3] = {Y(y), Y(by + ny), Y(by - ny)};
        g.fillTriangle(px[0], py[0], px[1], py[1], px[2], py[2], color);
        for (int i = 0; i < 3; i++) {
            g.drawWideLine(px[i], py[i], px[(i + 1) % 3], py[(i + 1) % 3], 0.75f, color);
        }
    }

    // A road following a circle from angle a0 to a1 (degrees, 0 = right,
    // clockwise as y runs down)
    void bend(float ox, float oy, float r, float a0, float a1, uint16_t color) const {
        const int STEPS = 8;
        float px = ox + r * cosf(a0 * (float)M_PI / 180), py = oy + r * sinf(a0 * (float)M_PI / 180);
        for (int i = 1; i <= STEPS; i++) {
            float a = (a0 + (a1 - a0) * i / STEPS) * (float)M_PI / 180;
            float x = ox + r * cosf(a), y = oy + r * sinf(a);
            g.drawWideLine(X(px), Y(py), X(x), Y(y), ROAD, color);
            px = x;
            py = y;
        }
    }
};

static void straight(const Pen& p, uint16_t c) {
    const float r[][2] = {{0, 42}, {0, -18}};
    p.road(r, 2, c);
    p.head(0, -44, 0, -1, c);
}

static void turn(const Pen& p, uint16_t c) {
    const float up[][2] = {{-18, 42}, {-18, -4}};
    p.road(up, 2, c);
    p.bend(2, -4, 20, 180, 270, c);  // round the corner
    const float across[][2] = {{2, -24}, {18, -24}};
    p.road(across, 2, c);
    p.head(44, -24, 1, 0, c);
}

static void slight(const Pen& p, uint16_t c) {
    const float r[][2] = {{-12, 42}, {-12, 8}, {8, -12}};
    p.road(r, 3, c);
    p.head(28, -32, 1, -1, c);
}

static void sharp(const Pen& p, uint16_t c) {
    const float r[][2] = {{-16, 42}, {-16, -26}, {4, -6}};
    p.road(r, 3, c);
    p.head(24, 14, 1, 1, c);
}

static void uturn(const Pen& p, uint16_t c) {
    const float up[][2] = {{-16, 42}, {-16, -12}};
    p.road(up, 2, c);
    p.bend(2, -12, 18, 180, 360, c);  // over the top
    const float down[][2] = {{20, -12}, {20, 8}};
    p.road(down, 2, c);
    p.head(20, 36, 0, 1, c);
}

static void keep(const Pen& p, uint16_t c, uint16_t dim) {
    const float other[][2] = {{0, 10}, {-18, -12}, {-18, -40}};
    p.road(other, 3, dim);
    const float r[][2] = {{0, 42}, {0, 10}, {18, -12}, {18, -18}};
    p.road(r, 4, c);
    p.head(18, -44, 0, -1, c);
}

static void ramp(const Pen& p, uint16_t c, uint16_t dim) {
    const float main[][2] = {{-14, 44}, {-14, -44}};
    p.road(main, 2, dim);
    const float r[][2] = {{-14, 42}, {-14, 16}, {8, -6}};
    p.road(r, 3, c);
    p.head(28, -26, 1, -1, c);
}

static void merge(const Pen& p, uint16_t c, uint16_t dim) {
    const float other[][2] = {{-24, 42}, {-24, 24}, {0, 0}};
    p.road(other, 3, dim);
    const float r[][2] = {{0, 42}, {0, -18}};
    p.road(r, 2, c);
    p.head(0, -44, 0, -1, c);
}

// Traffic goes round anticlockwise (riding on the right): the 1st exit is
// to the right, the 2nd straight on, the 3rd to the left
static void roundabout(const Pen& p, const Maneuver& m, uint16_t c, uint16_t bg) {
    const float ox = 0, oy = 8, ring = 19;
    p.g.fillSmoothCircle(p.X(ox), p.Y(oy), ring + 4, c);
    p.g.fillSmoothCircle(p.X(ox), p.Y(oy), ring - 4, bg);
    const float in[][2] = {{0, 44}, {0, oy + ring}};
    p.road(in, 2, c);
    static const float EXIT_DEG[] = {-90, 0, -90, 180};  // by exit number; unknown: straight on
    float deg = m.exit >= 1 && m.exit <= 3 ? EXIT_DEG[m.exit] : -90;
    float dx = cosf(deg * (float)M_PI / 180), dy = sinf(deg * (float)M_PI / 180);
    const float out[][2] = {{ox + dx * ring, oy + dy * ring}, {ox + dx * 28, oy + dy * 28}};
    p.road(out, 2, c);
    p.head(ox + dx * 50, oy + dy * 50, dx, dy, c);
    if (m.exit > 0) {
        char n[4];
        snprintf(n, sizeof(n), "%d", m.exit);
        useFont(p.g, FONT_S);
        p.g.setTextDatum(textdatum_t::middle_center);
        p.g.setTextColor(c, bg);
        p.g.drawString(n, p.X(ox), p.Y(oy) + 1);
    }
}

// A map pin
static void arrive(const Pen& p, uint16_t c, uint16_t bg) {
    p.g.drawWedgeLine(p.X(0), p.Y(-6), p.X(0), p.Y(40), 20, 1.5f, c);
    p.g.fillSmoothCircle(p.X(0), p.Y(-14), 24, c);
    p.g.fillSmoothCircle(p.X(0), p.Y(-14), 9, bg);
}

void drawManeuver(lgfx::LGFXBase& g, int cx, int cy, const Maneuver& m, uint16_t color, uint16_t dim,
                  uint16_t bg) {
    Pen p{g, cx, cy, m.side < 0 ? -1 : 1};
    switch (m.kind) {
    case MANEUVER_TURN: turn(p, color); break;
    case MANEUVER_SLIGHT: slight(p, color); break;
    case MANEUVER_SHARP: sharp(p, color); break;
    case MANEUVER_UTURN: uturn(p, color); break;
    case MANEUVER_KEEP: keep(p, color, dim); break;
    case MANEUVER_RAMP: ramp(p, color, dim); break;
    case MANEUVER_MERGE: merge(p, color, dim); break;
    case MANEUVER_ROUNDABOUT: roundabout(p, m, color, bg); break;
    case MANEUVER_ARRIVE: arrive(p, color, bg); break;
    case MANEUVER_STRAIGHT:
    case MANEUVER_UNKNOWN: straight(p, color); break;
    }
}
