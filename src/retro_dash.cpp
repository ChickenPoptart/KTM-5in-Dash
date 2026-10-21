#include "retro_dash.h"

#include <math.h>
#include <string.h>
#include "dash_fonts.h"
#include "icon_scale.h"
#include "icons.h"
#include "marquee.h"
#include "nav_icons.h"
#include "style.h"

// ---- Layout (800 x 480): five "LCD windows" ----

struct Rect { int x, y, w, h; };
// Tach on the left, speedo on the right
static const Rect TOP{10, 8, 780, 52}, TACH{10, 70, 290, 332}, CENTRE{312, 70, 176, 332},
    SPEED{500, 70, 290, 332}, BOTTOM{10, 410, 780, 58};
static const int TOP_MID = TOP.y + TOP.h / 2, BOTTOM_MID = BOTTOM.y + BOTTOM.h / 2;

// ---- The curved bar graphs ----

struct Pt { float x, y; };
struct Quad { Pt p[4]; };

// Speedo: a quadratic curve sweeping up to the right, blocks growing longer
static const Pt SPEED_CURVE[3] = {{530, 388}, {534, 122}, {772, 114}};
static const int SPEED_SEGS = 40, SPEED_MAX = 100;
// Tach: rises and hooks over at the redline, like the C4's
static const Pt TACH_CURVE[4] = {{40, 388}, {58, 80}, {170, 34}, {262, 206}};  // peaks level with the speedo
static const int TACH_SEGS = 50;

static Quad speedQuads[SPEED_SEGS], tachQuads[TACH_SEGS];

static Pt bezier(const Pt* p, int n, float t) {  // n = 3 or 4 points
    float u = 1 - t;
    if (n == 3) return {u * u * p[0].x + 2 * u * t * p[1].x + t * t * p[2].x,
                        u * u * p[0].y + 2 * u * t * p[1].y + t * t * p[2].y};
    return {u * u * u * p[0].x + 3 * u * u * t * p[1].x + 3 * u * t * t * p[2].x + t * t * t * p[3].x,
            u * u * u * p[0].y + 3 * u * u * t * p[1].y + 3 * u * t * t * p[2].y + t * t * t * p[3].y};
}

// Points evenly spaced along the curve (by length, not by t), with the
// curve's direction at each
static const int STEPS = 200;
struct CurveTable { Pt pt[STEPS + 1]; float len[STEPS + 1]; };

static void tableFor(const Pt* p, int n, CurveTable& c) {
    c.len[0] = 0;
    for (int i = 0; i <= STEPS; i++) {
        c.pt[i] = bezier(p, n, (float)i / STEPS);
        if (i) c.len[i] = c.len[i - 1] + hypotf(c.pt[i].x - c.pt[i - 1].x, c.pt[i].y - c.pt[i - 1].y);
    }
}

static void along(const CurveTable& c, float s, Pt& at, Pt& dir) {
    int i = 1;
    while (i < STEPS && c.len[i] < s) i++;
    float f = (s - c.len[i - 1]) / (c.len[i] - c.len[i - 1]);
    at = {c.pt[i - 1].x + (c.pt[i].x - c.pt[i - 1].x) * f, c.pt[i - 1].y + (c.pt[i].y - c.pt[i - 1].y) * f};
    float dx = c.pt[i].x - c.pt[i - 1].x, dy = c.pt[i].y - c.pt[i - 1].y, m = hypotf(dx, dy);
    dir = {dx / m, dy / m};
}

// Blocks standing out from the curve (to its outer side), gaps between
static void makeBars(const Pt* p, int n, Quad* quads, int count, float len0, float len1) {
    static CurveTable c;
    tableFor(p, n, c);
    float step = c.len[STEPS] / count, gap = 3;
    for (int i = 0; i < count; i++) {
        Pt a, da, b, db;
        along(c, i * step + gap / 2, a, da);
        along(c, (i + 1) * step - gap / 2, b, db);
        float l = len0 + (len1 - len0) * i / (count - 1);
        // outward normal: the curve's direction turned a quarter left
        quads[i] = Quad{{a, b, {b.x + db.y * l, b.y - db.x * l}, {a.x + da.y * l, a.y - da.x * l}}};
    }
}

// Scale numbers on the inner side of a curve, evenly along it
static void scaleLabels(LGFX_W5& g, const Pt* p, int n, const char* const* labels, int count, int first) {
    static CurveTable c;
    tableFor(p, n, c);
    useFont(g, FONT_S);
    g.setTextDatum(textdatum_t::middle_center);
    g.setTextColor(COL_LABEL, COL_PANEL);
    for (int k = first; k < count; k++) {
        float s = c.len[STEPS] * k / (count - 1);
        if (s < 1) s = 1;
        if (s > c.len[STEPS] - 1) s = c.len[STEPS] - 1;
        Pt at, dir;
        along(c, s, at, dir);
        g.drawString(labels[k], at.x - dir.y * 22, at.y + dir.x * 22);
    }
}

static void paintQuad(LGFX_W5& g, const Quad& q, uint16_t color) {
    g.fillTriangle(q.p[0].x, q.p[0].y, q.p[1].x, q.p[1].y, q.p[2].x, q.p[2].y, color);
    g.fillTriangle(q.p[0].x, q.p[0].y, q.p[2].x, q.p[2].y, q.p[3].x, q.p[3].y, color);
}

static uint16_t quarter(uint16_t c) {  // an unlit segment: the lit colour, faint
    return (((c >> 11) / 4) << 11) | ((((c >> 5) & 0x3F) / 4) << 5) | ((c & 0x1F) / 4);
}

static uint16_t tachColour(int i) {
    int rpm = (long)i * RPM_MAX / TACH_SEGS;
    return rpm >= RPM_RED_FROM ? COL_RED : rpm >= RPM_AMBER_FROM ? COL_AMBER : COL_ACCENT;
}

static void paintTach(LGFX_W5& g, int i, bool lit) {
    uint16_t c = tachColour(i);
    paintQuad(g, tachQuads[i], lit ? c : quarter(c));
}

static void paintSpeed(LGFX_W5& g, int i, bool lit) {
    paintQuad(g, speedQuads[i], lit ? COL_SPEED : quarter(COL_SPEED));
}

// ---- Segment text ----

// Seven-segment text over its unlit "8"s, into a sprite (no flicker)
static void ghostText(lgfx::LGFXBase& s, DashFont font, const char* ghost, const char* text, int x, int y,
                      uint16_t color, uint16_t bg) {
    useFont(s, font);
    s.setTextDatum(textdatum_t::middle_center);
    s.setTextColor(COL_TRACK, bg);
    s.drawString(ghost, x, y);
    s.setTextColor(color);
    s.drawString(text, x, y);
}

// value right-aligned in `digits` places, blanks as "!"; "--" when unknown
static void digits(char* out, int size, int value, int places, bool known) {
    if (known) snprintf(out, size, "%*d", places, value);
    else snprintf(out, size, "%*s", places, "--");
    for (char* p = out; *p; p++) {
        if (*p == ' ') *p = '!';
    }
}

// Fourteen-segment text straight onto the screen, one character cell at a
// time and only where it changed: each cell is cleared to its unlit "~",
// then the character lit over it
struct SegLine {
    int x, y;        // left edge, middle
    int width;       // characters
    DashFont font;
    char shown[40];

    void draw(LGFX_W5& g, const char* text, uint16_t color) {
        useFont(g, font);
        int cw = g.textWidth("!");
        g.setTextDatum(textdatum_t::middle_left);
        for (int i = 0; i < width; i++) {
            char c = text[i] ? text[i] : '!';
            if (shown[i] == c) continue;
            shown[i] = c;
            char one[2] = {c, 0};
            g.setTextColor(COL_TRACK, COL_PANEL);
            g.drawString("~", x + i * cw, y);
            g.setTextColor(color);
            g.drawString(one, x + i * cw, y);
        }
    }
    void forget() { memset(shown, 0, sizeof(shown)); }
};

static SegLine satLine{540, TOP_MID, 5, FONT_SEG14, {}};
static SegLine phoneLine{650, TOP_MID, 5, FONT_SEG14, {}};
static SegLine bottomLine{26, BOTTOM_MID, 28, FONT_SEG14, {}};
static SegLine distLine{328, 352, 8, FONT_SEG14, {}};
static SegLine streetLine{325, 384, 10, FONT_SEG14_S, {}};
static SegLine cardinalLine{382, 310, 2, FONT_SEG14, {}};
static SegLine routeLine{325, 384, 10, FONT_SEG14_S, {}};

// Seven-segment readouts drawn straight on the screen (rarely changing)
struct SegReadout {
    int x, y;
    DashFont font;
    const char* ghost;
    char shown[16];

    void draw(LGFX_W5& g, const char* text, uint16_t color) {
        if (strcmp(text, shown) == 0) return;
        snprintf(shown, sizeof(shown), "%s", text);
        ghostText(g, font, ghost, text, x, y, color, COL_PANEL);
    }
    void forget() { shown[0] = '\0'; }
};

static SegReadout tempReadout{262, TOP_MID, FONT_SEG_TINY, "888", ""};
static SegReadout clockReadout{400, TOP_MID, FONT_SEG_SMALL, "88:88", ""};
static SegReadout fillReadout{618, 380, FONT_SEG_TINY, "888.8", ""};
static SegReadout headingReadout{400, 274, FONT_SEG_SMALL, "888", ""};
static SegReadout altReadout{424, 346, FONT_SEG_TINY, "8888", ""};

// ---- Digit buffers (internal RAM) for the big numbers ----

static const Rect SPEED_DIGITS{586, 258, 200, 84}, RPM_DIGITS{102, 276, 136, 84}, GEAR_DIGIT{350, 112, 100, 116};
static LGFX_Sprite* speedSprite = nullptr;
static LGFX_Sprite* rpmSprite = nullptr;
static LGFX_Sprite* gearSprite = nullptr;

static LGFX_Sprite* makeSprite(LGFX_W5& g, const Rect& r) {
    LGFX_Sprite* s = new LGFX_Sprite(&g);
    s->setPsram(false);
    s->setColorDepth(16);
    if (!s->createSprite(r.w, r.h)) {
        delete s;
        return nullptr;
    }
    return s;
}

static void pushDigits(LGFX_Sprite* s, const Rect& r, DashFont font, const char* ghost, const char* text,
                       uint16_t color) {
    s->fillScreen(COL_PANEL);
    ghostText(*s, font, ghost, text, r.w / 2, r.h / 2, color, COL_PANEL);
    s->pushSprite(r.x, r.y);
}

// ---- What's on screen, so only changes are drawn ----

static int speedLit, tachLit, shownSpeed, shownRpm100, shownGear, tachLook, coolLit, centreMode,
    bottomMode, playShown;
static uint32_t arrowShown;

static void forgetAll() {
    speedLit = tachLit = shownSpeed = shownRpm100 = shownGear = tachLook = coolLit = centreMode = -99;
    bottomMode = playShown = -99;
    arrowShown = 0xFFFFFFFF;
    for (SegLine* l : {&satLine, &phoneLine, &bottomLine, &distLine, &streetLine, &cardinalLine, &routeLine}) {
        l->forget();
    }
    for (SegReadout* r : {&tempReadout, &clockReadout, &fillReadout, &headingReadout, &altReadout}) r->forget();
}

RetroDash::RetroDash(LGFX_W5& lcd) : lcd(lcd) {}

bool RetroDash::begin() {
    if (ready) return true;
    makeBars(SPEED_CURVE, 3, speedQuads, SPEED_SEGS, 12, 34);
    makeBars(TACH_CURVE, 4, tachQuads, TACH_SEGS, 14, 30);
    speedSprite = makeSprite(lcd, SPEED_DIGITS);
    rpmSprite = makeSprite(lcd, RPM_DIGITS);
    gearSprite = makeSprite(lcd, GEAR_DIGIT);
    ready = speedSprite && rpmSprite && gearSprite;
    if (!ready) end();
    return ready;
}

void RetroDash::end() {
    for (LGFX_Sprite** s : {&speedSprite, &rpmSprite, &gearSprite}) {
        if (*s) {
            (*s)->deleteSprite();
            delete *s;
            *s = nullptr;
        }
    }
    ready = false;
}

static void window(LGFX_W5& g, const Rect& r) {
    g.fillRoundRect(r.x, r.y, r.w, r.h, 10, COL_PANEL_EDGE);
    g.fillRoundRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 8, COL_PANEL);
}

static void label(LGFX_W5& g, const char* text, int x, int y, textdatum_t datum = textdatum_t::middle_center) {
    useFont(g, FONT_S);
    g.setTextDatum(datum);
    g.setTextColor(COL_LABEL, COL_PANEL);
    g.drawString(text, x, y);
}

static const int COOL_SEGS = 12, COOL_X = 70;
static const int COOL_LOW_F = 100, COOL_HIGH_F = 250, COOL_RED_FROM = 10;  // last two: hot

static void paintCool(LGFX_W5& g, int i, bool lit) {
    uint16_t c = i >= COOL_RED_FROM ? COL_RED : COL_ACCENT;
    g.fillRect(COOL_X + i * 11, TOP_MID - 10, 7, 20, lit ? c : quarter(c));
}

void RetroDash::drawStatic() {
    forgetAll();
    lcd.fillScreen(COL_BG);
    for (const Rect& r : {TOP, SPEED, CENTRE, TACH, BOTTOM}) window(lcd, r);

    // Top: coolant bar, temperature, clock, satellites, phone, the sun
    iconCoolant(lcd, 32, TOP_MID - 2, COL_LABEL);  // coolant: cold to hot
    label(lcd, "C", COOL_X - 8, TOP_MID);
    label(lcd, "H", COOL_X + COOL_SEGS * 11 + 7, TOP_MID);
    for (int i = 0; i < COOL_SEGS; i++) paintCool(lcd, i, false);
    iconBrightness(lcd, 764, TOP_MID, COL_TEXT);

    // Speedo
    for (int i = 0; i < SPEED_SEGS; i++) paintSpeed(lcd, i, false);
    static const char* const SPEED_LABELS[] = {"0", "20", "40", "60", "80", "100"};
    scaleLabels(lcd, SPEED_CURVE, 3, SPEED_LABELS, 6, 1);
    label(lcd, "MPH", 766, 352);
    label(lcd, "FILL", 530, 380, textdatum_t::middle_left);

    // Tach
    for (int i = 0; i < TACH_SEGS; i++) paintTach(lcd, i, false);
    static const char* const TACH_LABELS[] = {"0", "20", "40", "60", "80", "100"};
    scaleLabels(lcd, TACH_CURVE, 4, TACH_LABELS, 6, 0);
    label(lcd, "RPM/100", 230, 368);

    // Centre
    label(lcd, "GEAR", 400, 92);
}

// The tach window's frame is the shift light
static void drawTachFrame(LGFX_W5& g, uint16_t color) {
    for (int k = 0; k < 3; k++) g.drawRoundRect(TACH.x + k, TACH.y + k, TACH.w - 2 * k, TACH.h - 2 * k, 10 - k, color);
}

void RetroDash::selfTest() {
    if (!ready) return;
    // Every segment on, like an 80s dash at key-on
    for (int i = 0; i < SPEED_SEGS; i++) paintSpeed(lcd, i, true);
    for (int i = 0; i < TACH_SEGS; i++) paintTach(lcd, i, true);
    for (int i = 0; i < COOL_SEGS; i++) paintCool(lcd, i, true);
    pushDigits(speedSprite, SPEED_DIGITS, FONT_SEG_BIG, "888", "888", COL_TEXT);
    pushDigits(rpmSprite, RPM_DIGITS, FONT_SEG_BIG, "88", "88", COL_TEXT);
    pushDigits(gearSprite, GEAR_DIGIT, FONT_SEG_GEAR, "8", "8", COL_TEXT);
    drawTachFrame(lcd, COL_ACCENT);
    delay(700);
    // ...then the bars sweep back down
    const int FRAMES = 30;
    for (int f = FRAMES - 1; f >= 0; f--) {
        int sp = SPEED_SEGS * f / FRAMES, tc = TACH_SEGS * f / FRAMES;
        for (int i = sp; i < SPEED_SEGS; i++) paintSpeed(lcd, i, false);
        for (int i = tc; i < TACH_SEGS; i++) paintTach(lcd, i, false);
        delay(20);
    }
    for (int i = 0; i < COOL_SEGS; i++) paintCool(lcd, i, false);
    drawTachFrame(lcd, COL_PANEL_EDGE);
    forgetAll();
    speedLit = tachLit = coolLit = 0;
}

// A bar's lit count moved: repaint just the blocks in between
template <typename Paint>
static void moveBar(int& shown, int now, Paint paint) {
    if (now == shown) return;
    if (shown < 0) shown = 0;
    int lo = shown < now ? shown : now, hi = shown < now ? now : shown;
    for (int i = lo; i < hi; i++) paint(i, i < now);
    shown = now;
}

// Dot-matrix arrows, drawn by hand for a 14 x 14 grid (right-hand versions;
// lefts are mirrored): '#' lit, 'o' the road not taken (dim), '.' unlit
static const int DOTS = 14;
struct DotArrow { ManeuverKind kind; const char* rows[DOTS]; };
static const DotArrow DOT_ARROWS[] = {
    {MANEUVER_STRAIGHT, {
        "......##......",
        ".....####.....",
        "....######....",
        "...########...",
        "..##########..",
        "......##......",
        "......##......",
        "......##......",
        "......##......",
        "......##......",
        "......##......",
        "......##......",
        "......##......",
        "......##......",
    }},
    {MANEUVER_TURN, {
        "..............",
        "........#.....",
        "........##....",
        "........###...",
        "...#########..",
        "..###########.",
        "..##########..",
        "..##.....##...",
        "..##.....#....",
        "..##..........",
        "..##..........",
        "..##..........",
        "..##..........",
        "..##..........",
    }},
    {MANEUVER_SLIGHT, {
        "..............",
        "......#######.",
        "........#####.",
        ".........####.",
        "........#####.",
        ".......###.##.",
        "......###...#.",
        ".....###......",
        "....###.......",
        "...###........",
        "...##.........",
        "...##.........",
        "...##.........",
        "...##.........",
    }},
    {MANEUVER_SHARP, {
        "..##..........",
        "..##..........",
        "..###.........",
        "...###........",
        "....###.......",
        ".....###......",
        "......###..#..",
        ".......###.##.",
        "........#####.",
        ".........####.",
        "........#####.",
        "......#######.",
        "..............",
        "..............",
    }},
    {MANEUVER_UTURN, {
        "..............",
        "....######....",
        "...########...",
        "..###....###..",
        "..##......##..",
        "..##......##..",
        "..##......##..",
        "..##......##..",
        "..##....######",
        "..##.....####.",
        "..##......##..",
        "..##..........",
        "..##..........",
        "..##..........",
    }},
    {MANEUVER_KEEP, {
        "..o......####.",
        "..o.......###.",
        "..o......####.",
        "..o.....###.#.",
        "..oo...###....",
        "...o..###.....",
        "...o.###......",
        "....###.......",
        "....##........",
        "....##........",
        "....##........",
        "....##........",
        "....##........",
        "....##........",
    }},
    {MANEUVER_RAMP, {
        "..o.......###.",
        "..o........##.",
        "..o.......###.",
        "..o......##.#.",
        "..o.....##....",
        "..o....##.....",
        "..o...##......",
        "..o..##.......",
        "..##.#........",
        "..###.........",
        "..##..........",
        "..##..........",
        "..##..........",
        "..##..........",
    }},
    {MANEUVER_MERGE, {
        "......##......",
        ".....####.....",
        "....######....",
        "...##.##.##...",
        "......##......",
        "......##......",
        "......##......",
        ".....###......",
        "....o.##......",
        "...o..##......",
        "..o...##......",
        ".o....##......",
        "o.....##......",
        "......##......",
    }},
    {MANEUVER_ROUNDABOUT, {
        "......##......",
        ".....####.....",
        "....######....",
        "......##......",
        "....######....",
        "...##....##...",
        "..##......##..",
        "..##......##..",
        "..##......##..",
        "...##....##...",
        "....######....",
        "......##......",
        "......##......",
        "......##......",
    }},
    {MANEUVER_ARRIVE, {
        "....######....",
        "...########...",
        "..####..####..",
        "..###....###..",
        "..###....###..",
        "..####..####..",
        "...########...",
        "...########...",
        "....######....",
        ".....####.....",
        ".....####.....",
        "......##......",
        "......##......",
        "..............",
    }},
};

// The turn as a dot-matrix: our hand-drawn arrow, or the phone's own icon
// sampled onto the grid when the directions aren't one we know
static void drawDotArrow(LGFX_W5& g, const RetroView& v) {
    const int CELL = 7, X0 = 400 - DOTS * CELL / 2, Y0 = 234;
    const DotArrow* art = nullptr;
    for (const DotArrow& a : DOT_ARROWS) {
        if (a.kind == v.maneuver.kind) art = &a;
    }
    if (!art && !v.phoneIcon) art = &DOT_ARROWS[0];  // nothing to go on: straight on
    bool mirror = v.maneuver.side < 0;
    for (int r = 0; r < DOTS; r++) {
        for (int c = 0; c < DOTS; c++) {
            char ch = '.';
            if (art) ch = art->rows[r][mirror ? DOTS - 1 - c : c];
            else if (iconBit(v.phoneIcon, 48, c * 48 / DOTS + 1, r * 48 / DOTS + 1)) ch = '#';
            uint16_t col = ch == '#' ? COL_TEXT : ch == 'o' ? COL_LABEL : COL_TRACK;
            g.fillRect(X0 + c * CELL, Y0 + r * CELL, CELL - 1, CELL - 1, col);
        }
    }
}

// Below the gear digit (which ends at y 228)
static void clearCentreLower(LGFX_W5& g) { g.fillRect(CENTRE.x + 4, 230, CENTRE.w - 8, 166, COL_PANEL); }

static void drawCentre(LGFX_W5& g, const RetroView& v, unsigned long nowMs) {
    int mode = v.route ? 1 : 0;
    if (mode != centreMode) {
        centreMode = mode;
        clearCentreLower(g);
        arrowShown = 0xFFFFFFFF;
        for (SegLine* l : {&distLine, &streetLine, &cardinalLine, &routeLine}) l->forget();
        headingReadout.forget();
        altReadout.forget();
        if (!v.route) {
            label(g, "HDG", 400, 244);
            label(g, "ALT", 336, 346, textdatum_t::middle_left);
        }
    }
    char buf[96], win[16];
    if (v.route) {
        uint32_t id = v.maneuver.kind != MANEUVER_UNKNOWN
                          ? 0x80000000u | v.maneuver.kind << 16 | (v.maneuver.side + 1) << 8 | v.maneuver.exit
                          : v.phoneIconId;
        if (id != arrowShown) {
            arrowShown = id;
            drawDotArrow(g, v);
        }
        marqueeWindow(v.distance, distLine.width, 0, win, sizeof(win));
        distLine.draw(g, win, COL_TEXT);
        // The street, then the ETA, scrolling past
        snprintf(buf, sizeof(buf), "%s   ETA %s   %s", v.street, v.eta, v.left);
        marqueeWindow(buf, streetLine.width, nowMs / 300, win, sizeof(win));
        streetLine.draw(g, win, COL_TEXT);
        return;
    }
    if (v.headingKnown) snprintf(buf, sizeof(buf), "%03d", v.headingDeg);
    else snprintf(buf, sizeof(buf), "!--");
    headingReadout.draw(g, buf, v.headingLive ? COL_TEXT : COL_LABEL);
    cardinalLine.draw(g, v.headingKnown ? headingCardinal(v.headingDeg) : "--", v.headingLive ? COL_TEXT : COL_LABEL);
    digits(buf, sizeof(buf), v.elevationFt, 4, v.altitudeKnown);
    altReadout.draw(g, buf, COL_TEXT);
    marqueeWindow("No route", routeLine.width, 0, win, sizeof(win));
    routeLine.draw(g, win, COL_LABEL);
}

static const int BTN_X0 = 560, BTN_STEP = 76, BTN_W = 64;

static void drawButtons(LGFX_W5& g, bool show, bool playing) {
    for (int k = 0; k < 3; k++) {
        int x = BTN_X0 + k * BTN_STEP;
        g.fillRect(x, BOTTOM.y + 10, BTN_W + 1, BOTTOM.h - 20, COL_PANEL);
        if (!show) continue;
        g.drawRoundRect(x, BOTTOM.y + 12, BTN_W, BOTTOM.h - 24, 6, COL_ACCENT);
        g.drawRoundRect(x + 1, BOTTOM.y + 13, BTN_W - 2, BOTTOM.h - 26, 5, COL_ACCENT);
        int cx = x + BTN_W / 2;
        if (k == 0) iconPrevious(g, cx, BOTTOM_MID, COL_TEXT);
        else if (k == 2) iconNext(g, cx, BOTTOM_MID, COL_TEXT);
        else if (playing) iconPause(g, cx, BOTTOM_MID, COL_TEXT);
        else iconPlay(g, cx + 2, BOTTOM_MID, COL_TEXT);
    }
}

static void drawBottom(LGFX_W5& g, const RetroView& v, unsigned long nowMs) {
    int mode = v.alert ? 2 : v.musicBar ? 1 : 0;
    bool buttons = mode == 1;
    if (mode != bottomMode || (buttons && v.playing != playShown)) {
        bottomMode = mode;
        playShown = v.playing;
        drawButtons(g, buttons, v.playing);
    }
    char buf[200], win[40];
    uint16_t color = COL_TEXT;
    int offset = nowMs / 300;  // scrolling speed
    if (v.alert && v.alertIsCall) {
        bool blink = (nowMs / 500) % 2;  // CALL flashes
        snprintf(buf, sizeof(buf), "%s %s", blink ? "CALL" : "    ", v.alertFrom[0] ? v.alertFrom : "UNKNOWN");
        offset = 0;
        color = COL_GREEN;
    } else if (v.alert) {
        snprintf(buf, sizeof(buf), "MSG %s: %s", v.alertFrom, v.alertText);
    } else if (v.musicBar) {
        if (v.title[0]) snprintf(buf, sizeof(buf), "%s - %s", v.title, v.artist);
        else snprintf(buf, sizeof(buf), "NOTHING PLAYING");
        if (!v.playing) color = COL_LABEL;
    } else {
        snprintf(buf, sizeof(buf), "TRIP %.1f MI   TOP %d MPH", v.tripMiles, v.tripTopMph);
        offset = 0;
    }
    marqueeWindow(buf, bottomLine.width, offset, win, sizeof(win));
    bottomLine.draw(g, win, color);
}

static void drawTop(LGFX_W5& g, const RetroView& v, unsigned long nowMs) {
    int lit = v.coolantLive ? segmentsLit(v.coolantF - COOL_LOW_F, COOL_HIGH_F - COOL_LOW_F, COOL_SEGS) : 0;
    moveBar(coolLit, lit, [&](int i, bool on) { paintCool(g, i, on); });
    char buf[16];
    digits(buf, sizeof(buf), v.coolantF, 3, v.coolantLive);
    tempReadout.draw(g, buf, v.coolantF >= 230 ? COL_RED : COL_TEXT);

    // The clock, its colon blinking each second
    snprintf(buf, sizeof(buf), "%5s", v.clock);
    for (char* p = buf; *p; p++) {
        if (*p == ' ') *p = '!';
    }
    char* colon = strchr(buf, ':');
    if (colon && (nowMs / 1000) % 2) *colon = ' ';
    clockReadout.draw(g, buf, COL_TEXT);

    char line[16];
    snprintf(line, sizeof(line), "SAT%2d", v.satellites);
    marqueeWindow(line, 5, 0, buf, sizeof(buf));
    satLine.draw(g, buf, v.gpsFix ? COL_TEXT : COL_LABEL);
    if (v.phone && v.phoneBattery >= 0) snprintf(line, sizeof(line), "PH%3d", v.phoneBattery);
    else snprintf(line, sizeof(line), "PH --");
    marqueeWindow(line, 5, 0, buf, sizeof(buf));
    phoneLine.draw(g, buf, v.phone ? COL_TEXT : COL_LABEL);
}

void RetroDash::update(const RetroView& v, unsigned long nowMs) {
    if (!ready) return;
    // Bars: every frame, only the blocks that changed
    moveBar(speedLit, v.speedLive ? segmentsLit(v.mph, SPEED_MAX, SPEED_SEGS) : 0,
            [&](int i, bool on) { paintSpeed(lcd, i, on); });
    moveBar(tachLit, v.rpmLive ? segmentsLit(v.rpm, RPM_MAX, TACH_SEGS) : 0,
            [&](int i, bool on) { paintTach(lcd, i, on); });

    // Shift light: the tach window's frame, yellow then flashing red
    int look = v.shift == SHIFT_NOW ? (v.flashOn ? 2 : 3) : v.shift == SHIFT_SOON ? 1 : 0;
    if (look != tachLook) {
        tachLook = look;
        drawTachFrame(lcd, look == 1 ? COL_GREEN : look == 2 ? COL_RED : COL_PANEL_EDGE);
    }

    // Big digits, when they change
    char buf[8];
    int speed = v.speedLive ? v.mph : -1;
    if (speed != shownSpeed) {
        shownSpeed = speed;
        digits(buf, sizeof(buf), v.mph, 3, v.speedLive);
        pushDigits(speedSprite, SPEED_DIGITS, FONT_SEG_BIG, "888", buf, v.speedLive ? COL_TEXT : COL_LABEL);
    }
    int rpm100 = v.rpmLive ? (v.rpm / 100 > 99 ? 99 : v.rpm / 100) : -1;
    int rpmKey = rpm100 * 4 + v.shift;  // the digits change colour with the shift light too
    if (rpmKey != shownRpm100) {
        shownRpm100 = rpmKey;
        digits(buf, sizeof(buf), rpm100, 2, v.rpmLive);
        uint16_t c = !v.rpmLive ? COL_LABEL : v.shift == SHIFT_NOW ? COL_RED : v.shift == SHIFT_SOON ? COL_GREEN : COL_TEXT;
        pushDigits(rpmSprite, RPM_DIGITS, FONT_SEG_BIG, "88", buf, c);
    }
    int gear = v.rpmLive ? v.gear : -2;
    if (gear != shownGear) {
        shownGear = gear;
        const char* g = gear < 0 ? "-" : gear == 0 ? "n" : gearLabel(gear);
        pushDigits(gearSprite, GEAR_DIGIT, FONT_SEG_GEAR, "8", g, gear == 0 ? COL_NEUTRAL : gear < 0 ? COL_LABEL : COL_TEXT);
    }

    // The rest a few times a second
    static unsigned long lastSlowMs = 0;
    if (nowMs - lastSlowMs < 150) return;
    lastSlowMs = nowMs;
    char fill[16];
    snprintf(fill, sizeof(fill), "%3d.%d", v.fuelTenths / 10 % 1000, v.fuelTenths % 10);
    for (char* p = fill; *p; p++) {
        if (*p == ' ') *p = '!';
    }
    fillReadout.draw(lcd, fill, COL_TEXT);
    drawTop(lcd, v, nowMs);
    drawCentre(lcd, v, nowMs);
    drawBottom(lcd, v, nowMs);
}

RetroTap RetroDash::tapAt(int x, int y, const RetroView& v) const {
    if (x >= 730 && y < TOP.y + TOP.h + 8) return RETRO_TAP_BRIGHTNESS;
    if (x >= SPEED.x && x < SPEED.x + 200 && y >= 350 && y < SPEED.y + SPEED.h) return RETRO_TAP_FUEL;  // FILL
    if (y >= BOTTOM.y - 6) {
        if (!v.alert && v.musicBar && x >= BTN_X0 - 6) {
            int k = (x - BTN_X0 + 6) / BTN_STEP;
            return k <= 0 ? RETRO_TAP_PREVIOUS : k == 1 ? RETRO_TAP_PLAY : RETRO_TAP_NEXT;
        }
        return RETRO_TAP_BOTTOM;
    }
    return RETRO_TAP_NONE;
}
