#include "gauge.h"

#include <math.h>
#include "dash_fonts.h"
#include "icons.h"
#include "style.h"

static const int C = GAUGE_SIZE / 2;  // centre within each gauge's square
static const int ARC_INNER = ARC_OUTER - ARC_WIDTH;

// The centre buffer covers the circle inside the arc plus the gap at the
// bottom of the dial. Everything else in it is a key colour that is left
// out when it's copied to the screen, so the arcs and ticks stay intact.
static const int CENTRE_W = 2 * CENTRE_RADIUS + 2;
static const int CENTRE_TOP = 88;     // gauge centre, down from the buffer's top
static const int CENTRE_H = CENTRE_TOP + 116;
static const int GAP_RADIUS = 116;    // how far down into the gap it reaches
static const uint16_t KEY = rgb(0xFF, 0x00, 0xFF);

// The RPM number's box, below the gear: inside the circle and the gap, so
// it can be pushed on its own without touching the arc
static const int RPM_TEXT_W = 150, RPM_TEXT_H = 48;
static const int RPM_TEXT_Y = 58;  // centre, below the gauge centre

// A quarter-brightness version of a colour, for the unlit track
static uint16_t dim(uint16_t c) {
    uint16_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
    return ((r / 4) << 11) | ((g / 4) << 5) | (b / 4);
}

static float angleFor(int value, int max) {
    if (value < 0) value = 0;
    if (value > max) value = max;
    return ARC_START_DEG + (float)ARC_SWEEP_DEG * value / max;
}

// fillArc on the screen around a gauge's centre, with angles that may run
// past 360
static void arc(LGFX_W5& g, int cx, int cy, int r0, int r1, float a0, float a1, uint16_t color) {
    if (a1 <= a0) return;
    if (a0 >= 360) {
        g.fillArc(cx, cy, r0, r1, a0 - 360, a1 - 360, color);
    } else if (a1 > 360) {
        g.fillArc(cx, cy, r0, r1, a0, 360, color);
        g.fillArc(cx, cy, r0, r1, 0, a1 - 360, color);
    } else {
        g.fillArc(cx, cy, r0, r1, a0, a1, color);
    }
}

struct Zone { int from, to; const uint16_t& color; };  // (palette changes with brightness)
static const Zone RPM_ZONES[] = {
    {0, RPM_GREEN_FROM, COL_IDLE},
    {RPM_GREEN_FROM, RPM_AMBER_FROM, COL_GREEN},
    {RPM_AMBER_FROM, RPM_RED_FROM, COL_AMBER},
    {RPM_RED_FROM, RPM_MAX, COL_RED},
};

Gauges::Gauges(LGFX_W5& lcd) : lcd(lcd), centre(&lcd), rpmText(&lcd) {
    rpmArc.ox = RPM_GAUGE_X;
    rpmArc.oy = GAUGE_Y;
    rpmArc.max = RPM_MAX;
    rpmArc.zoned = true;
    speedArc.ox = SPEED_GAUGE_X;
    speedArc.oy = GAUGE_Y;
    speedArc.max = SPEED_MAX_MPH;
    speedArc.zoned = false;
}

bool Gauges::begin() {
    centre.setPsram(false);  // internal RAM: keeps drawing off the PSRAM bus
    centre.setColorDepth(16);
    rpmText.setPsram(false);
    rpmText.setColorDepth(16);
    return centre.createSprite(CENTRE_W, CENTRE_H) != nullptr &&
           rpmText.createSprite(RPM_TEXT_W, RPM_TEXT_H) != nullptr;
}

void Gauges::end() {
    centre.deleteSprite();
    rpmText.deleteSprite();
}

// Colours [from, to) of an arc either lit or unlit
static void paintArc(LGFX_W5& g, int cx, int cy, int max, bool zoned, int from, int to, bool lit) {
    if (to <= from) return;
    if (!zoned) {
        arc(g, cx, cy, ARC_OUTER, ARC_INNER, angleFor(from, max), angleFor(to, max),
            lit ? COL_SPEED : COL_TRACK);
        return;
    }
    for (const Zone& z : RPM_ZONES) {
        int a = from > z.from ? from : z.from;
        int b = to < z.to ? to : z.to;
        if (b > a) arc(g, cx, cy, ARC_OUTER, ARC_INNER, angleFor(a, max), angleFor(b, max),
                       lit ? z.color : dim(z.color));
    }
}

// ---- Retro: the arc as separate blocks, lit one at a time ----

static const int SEGMENTS = 50;          // 200 rpm or 2 mph each
static const float SEGMENT_GAP_DEG = 1.8f;

static bool retro() { return theme() == THEME_RETRO; }

// The colour of block i: its RPM zone (or the speed colour), dim if unlit
static uint16_t segmentColour(int max, bool zoned, int i, bool lit) {
    uint16_t c = COL_SPEED;
    if (zoned) {
        int v = (long)i * max / SEGMENTS;  // where the block starts
        for (const Zone& z : RPM_ZONES) {
            if (v >= z.from && v < z.to) c = z.color;
        }
    }
    return lit ? c : dim(c);
}

static void paintSegment(LGFX_W5& g, int cx, int cy, int max, bool zoned, int i, bool lit) {
    float step = (float)ARC_SWEEP_DEG / SEGMENTS;
    float a0 = ARC_START_DEG + i * step + SEGMENT_GAP_DEG / 2;
    arc(g, cx, cy, ARC_OUTER, ARC_INNER, a0, a0 + step - SEGMENT_GAP_DEG, segmentColour(max, zoned, i, lit));
}

// Seven-segment text over its unlit "8"s (the ghost segments of a real
// LCD). DSEG digits are all one width, and "!" is a blank digit, so text
// padded to the ghost's length lines up with it.
static void segText(lgfx::LGFXBase& s, DashFont font, const char* ghost, const char* text, int x, int y,
                    uint16_t color) {
    useFont(s, font);
    s.setTextDatum(textdatum_t::middle_center);
    s.setTextColor(COL_TRACK);
    s.drawString(ghost, x, y);
    s.setTextColor(color);
    s.drawString(text, x, y);
}

// value right-aligned in `digits` places, blanks as "!"; "--" when unknown
static void segDigits(char* out, int size, int value, int digits, bool known) {
    if (known) snprintf(out, size, "%*d", digits, value);
    else snprintf(out, size, "%*s", digits, "--");
    for (char* p = out; *p; p++) {
        if (*p == ' ') *p = '!';
    }
}

void Gauges::drawRing(int ox, int oy, uint16_t color) {
    int cx = ox + C, cy = oy + C;
    lcd.fillArc(cx, cy, RING_RADIUS + 3, RING_RADIUS - 3, 0, 360, dim(color));
    lcd.fillArc(cx, cy, RING_RADIUS + 1, RING_RADIUS - 1, 0, 360, color);
}

void Gauges::drawDials() {
    for (ArcState* a : {&rpmArc, &speedArc}) {
        int cx = a->ox + C, cy = a->oy + C;
        lcd.fillRect(a->ox, a->oy, GAUGE_SIZE, GAUGE_SIZE, COL_BG);
        if (retro()) {
            for (int i = 0; i < SEGMENTS; i++) paintSegment(lcd, cx, cy, a->max, a->zoned, i, false);
        } else {
            paintArc(lcd, cx, cy, a->max, a->zoned, 0, a->max, false);
        }
        a->shown = 0;

        // Ticks just outside the band, numbers between them and the ring
        int step = a->zoned ? 1000 : SPEED_TICK_STEP;
        int divisor = a->zoned ? 1000 : 1;
        useFont(lcd, FONT_S);
        lcd.setTextDatum(textdatum_t::middle_center);
        lcd.setTextColor(COL_TICK_LABEL, COL_BG);
        for (int v = 0; v <= a->max; v += step / 2) {
            bool major = v % step == 0;
            float ang = angleFor(v, a->max) * (float)M_PI / 180.0f;
            float ca = cosf(ang), sa = sinf(ang);
            int r0 = ARC_OUTER + 3, r1 = ARC_OUTER + (major ? 11 : 7);
            lcd.drawWideLine(cx + ca * r0, cy + sa * r0, cx + ca * r1, cy + sa * r1,
                             major ? 1.5f : 1.0f, COL_TICK);
            if (major) {
                char label[8];
                snprintf(label, sizeof(label), "%d", v / divisor);
                lcd.drawString(label, cx + ca * TICK_LABEL_RADIUS, cy + sa * TICK_LABEL_RADIUS);
            }
        }
        drawRing(a->ox, a->oy, COL_RING);
    }
    ringColor = COL_RING;
    lastRpm = RpmView{-1, -2, false, SHIFT_NONE, false};
    lastSpeed = SpeedView{-1, false, -1};
}

// Repaints only the slice of arc between what it shows and the new value
void Gauges::moveArc(ArcState& a, int value) {
    if (value < 0) value = 0;
    if (value > a.max) value = a.max;
    if (value == a.shown) return;
    int cx = a.ox + C, cy = a.oy + C;
    if (retro()) {  // repaint only the blocks that changed
        int was = segmentsLit(a.shown, a.max, SEGMENTS), now = segmentsLit(value, a.max, SEGMENTS);
        for (int i = was < now ? was : now; i < (was < now ? now : was); i++) {
            paintSegment(lcd, cx, cy, a.max, a.zoned, i, i < now);
        }
        a.shown = value;
        return;
    }
    if (value > a.shown) paintArc(lcd, cx, cy, a.max, a.zoned, a.shown, value, true);
    else paintArc(lcd, cx, cy, a.max, a.zoned, value, a.shown, false);
    a.shown = value;
}

void Gauges::clearCentre() {
    centre.fillScreen(KEY);
    centre.fillCircle(CENTRE_W / 2, CENTRE_TOP, CENTRE_RADIUS, COL_BG);
    centre.fillArc(CENTRE_W / 2, CENTRE_TOP, GAP_RADIUS, 0, 53, 127, COL_BG);  // the gap
    centre.setTextDatum(textdatum_t::middle_center);
}

void Gauges::pushCentre(const ArcState& a) {
    centre.pushSprite(&lcd, a.ox + C - CENTRE_W / 2, a.oy + C - CENTRE_TOP, KEY);
}

static uint16_t ringColorFor(ShiftStage shift, bool flashOn) {
    if (shift == SHIFT_NOW) return flashOn ? COL_RED : dim(COL_RED);
    if (shift == SHIFT_SOON) return COL_GREEN;
    return COL_RING;
}

void Gauges::drawRpm(const RpmView& v) {
    moveArc(rpmArc, v.live ? v.rpm : 0);

    uint16_t ring = ringColorFor(v.shift, v.flashOn);
    if (ring != ringColor) {
        ringColor = ring;
        drawRing(rpmArc.ox, rpmArc.oy, ring);
    }

    // The centre only changes when what it shows does; RPM is rounded to
    // 10 so idle wobble doesn't repaint it constantly
    int shownRpm = v.rpm / 10 * 10;
    bool sameLook = v.gear == lastRpm.gear && v.live == lastRpm.live && v.shift == lastRpm.shift;
    if (shownRpm == lastRpm.rpm && sameLook) return;
    lastRpm = v;
    lastRpm.rpm = shownRpm;

    char buf[16];
    if (v.live) snprintf(buf, sizeof(buf), "%d,%03d", shownRpm / 1000, shownRpm % 1000);
    else snprintf(buf, sizeof(buf), "--");
    uint16_t rpmColor = !v.live ? COL_LABEL : v.shift == SHIFT_NOW ? COL_RED
                      : v.shift == SHIFT_SOON ? COL_GREEN : COL_TEXT;

    // Most frames only the number moves: send just its box (a small push
    // keeps the panel's frame buffer reads from being starved)
    char seg[8];
    segDigits(seg, sizeof(seg), shownRpm, 4, v.live);
    if (sameLook) {
        rpmText.fillScreen(COL_BG);
        if (retro()) {
            segText(rpmText, FONT_SEG_SMALL, "8888", seg, RPM_TEXT_W / 2, RPM_TEXT_H / 2, rpmColor);
        } else {
            useFont(rpmText, FONT_L);
            rpmText.setTextDatum(textdatum_t::middle_center);
            rpmText.setTextColor(rpmColor);
            rpmText.drawString(buf, RPM_TEXT_W / 2, RPM_TEXT_H / 2);
        }
        rpmText.pushSprite(&lcd, rpmArc.ox + C - RPM_TEXT_W / 2, rpmArc.oy + C + RPM_TEXT_Y - RPM_TEXT_H / 2);
        return;
    }

    LGFX_Sprite& s = centre;
    const int cx = CENTRE_W / 2, cy = CENTRE_TOP;
    clearCentre();

    bool haveGear = v.live && v.gear >= 0;
    uint16_t gearColor = !haveGear ? COL_LABEL : v.gear == 0 ? COL_NEUTRAL : COL_TEXT;
    if (retro()) {
        const char* g = !haveGear ? "-" : v.gear == 0 ? "n" : gearLabel(v.gear);  // a 7-segment "n"
        segText(s, FONT_SEG_GEAR, "8", g, cx, cy - 14, gearColor);
        segText(s, FONT_SEG_SMALL, "8888", seg, cx, cy + RPM_TEXT_Y, rpmColor);
    } else {
        useFont(s, FONT_HUGE);
        s.setTextColor(gearColor);
        s.drawString(haveGear ? gearLabel(v.gear) : "-", cx, cy - 12);
        useFont(s, FONT_L);
        s.setTextColor(rpmColor);
        s.drawString(buf, cx, cy + RPM_TEXT_Y);
    }

    useFont(s, FONT_S);
    s.setTextColor(COL_LABEL);
    s.drawString("RPM", cx, cy + 96);

    pushCentre(rpmArc);
}

void Gauges::drawSpeed(const SpeedView& v) {
    moveArc(speedArc, v.live ? v.mph : 0);

    if (v.mph == lastSpeed.mph && v.live == lastSpeed.live && v.fuelTenths == lastSpeed.fuelTenths) return;
    lastSpeed = v;

    LGFX_Sprite& s = centre;
    const int cx = CENTRE_W / 2, cy = CENTRE_TOP;
    clearCentre();

    char buf[16];
    if (retro()) {
        segDigits(buf, sizeof(buf), v.mph, 3, v.live);
        segText(s, FONT_SEG_SPEED, "888", buf, cx, cy - 4, v.live ? COL_TEXT : COL_LABEL);
    } else {
        if (v.live) snprintf(buf, sizeof(buf), "%d", v.mph);
        else snprintf(buf, sizeof(buf), "--");
        useFont(s, FONT_XL);
        s.setTextColor(v.live ? COL_TEXT : COL_LABEL);
        s.drawString(buf, cx, cy - 4);
    }

    useFont(s, FONT_S);
    s.setTextColor(COL_LABEL);
    s.drawString("MPH", cx, cy + 56);
    // Miles since gas fill, in the gap at the bottom like a trip meter
    snprintf(buf, sizeof(buf), "%d.%d mi", v.fuelTenths / 10, v.fuelTenths % 10);
    useFont(s, FONT_M);
    int w = 26 + s.textWidth(buf);  // pump icon, then the number
    iconFuel(s, cx - w / 2 + 9, cy + 94, COL_ACCENT, COL_BG);
    s.setTextColor(COL_TEXT);
    s.setTextDatum(textdatum_t::middle_left);
    s.drawString(buf, cx - w / 2 + 26, cy + 94);

    pushCentre(speedArc);
}
