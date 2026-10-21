#include "pages.h"

#include "dash_logic.h"
#include "field.h"
#include "style.h"

// "12,345": thousands separated, as the dash shows feet and rpm
static void thousands(char* out, int size, int v) {
    if (v >= 1000 || v <= -1000) snprintf(out, size, "%d,%03d", v / 1000, (v < 0 ? -v : v) % 1000);
    else snprintf(out, size, "%d", v);
}

// ---- Trips ----

static const int TRIP_W = 256, TRIP_GAP = 8, TRIP_Y = 12, TRIP_H = 412;
static int tripX(int col) { return TRIP_GAP + col * (TRIP_W + TRIP_GAP); }

static const char* const TRIP_TITLES[TRIP_COLUMNS] = {"THIS RIDE", "TRIP 1", "TRIP 2"};
static const int ROWS = 7;
static const char* const ROW_LABELS[ROWS] = {"Moving", "Average", "Top speed", "Top RPM",
                                             "Hottest", "Climb", "Descent"};
static const int ROW_Y0 = 150, ROW_STEP = 32;
static const int RESET_Y = 364, RESET_H = 50;

// The big distance, then a value at the right of each row
#define MILES_FIELD(c) Field{tripX(c) + 8, 98, TRIP_W - 16, 52, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0}
#define ROW_FIELD(c, r) \
    Field{tripX(c) + 110, ROW_Y0 + (r) * ROW_STEP, TRIP_W - 126, 30, textdatum_t::middle_right, FONT_M, COL_PANEL, "", 0}
#define ROW_FIELDS(c) \
    {ROW_FIELD(c, 0), ROW_FIELD(c, 1), ROW_FIELD(c, 2), ROW_FIELD(c, 3), ROW_FIELD(c, 4), ROW_FIELD(c, 5), ROW_FIELD(c, 6)}

static Field milesField[TRIP_COLUMNS] = {MILES_FIELD(0), MILES_FIELD(1), MILES_FIELD(2)};
static Field rowField[TRIP_COLUMNS][ROWS] = {ROW_FIELDS(0), ROW_FIELDS(1), ROW_FIELDS(2)};

static void layoutTrips() {
    useFont(lcd, FONT_M);
    for (int c = 0; c < TRIP_COLUMNS; c++) {
        int x = tripX(c);
        lcd.fillSmoothRoundRect(x, TRIP_Y, TRIP_W, TRIP_H, 14, COL_PANEL_EDGE);
        lcd.fillSmoothRoundRect(x + 1, TRIP_Y + 1, TRIP_W - 2, TRIP_H - 2, 13, COL_PANEL);
        useFont(lcd, FONT_M);
        lcd.setTextDatum(textdatum_t::middle_center);
        lcd.setTextColor(COL_ACCENT, COL_PANEL);
        lcd.drawString(TRIP_TITLES[c], x + TRIP_W / 2, 44);
        useFont(lcd, FONT_S);
        lcd.setTextDatum(textdatum_t::middle_left);
        lcd.setTextColor(COL_LABEL, COL_PANEL);
        for (int r = 0; r < ROWS; r++) lcd.drawString(ROW_LABELS[r], x + 16, ROW_Y0 + r * ROW_STEP);
        lcd.setTextDatum(textdatum_t::middle_center);
        if (c == TRIP_RIDE) {
            lcd.drawString("Since key-on", x + TRIP_W / 2, RESET_Y + RESET_H / 2);
        } else {
            lcd.fillSmoothRoundRect(x + 48, RESET_Y, TRIP_W - 96, RESET_H, 12, COL_PANEL_EDGE);
            useFont(lcd, FONT_M);
            lcd.setTextColor(COL_TEXT, COL_PANEL_EDGE);
            lcd.drawString("RESET", x + TRIP_W / 2, RESET_Y + RESET_H / 2);
        }
        milesField[c].shown[0] = '\0';
        for (Field& f : rowField[c]) f.shown[0] = '\0';
    }
}

void drawTripsPage(bool full, const TripStats* const trips[TRIP_COLUMNS]) {
    if (full) layoutTrips();
    char buf[24], n[16];
    for (int c = 0; c < TRIP_COLUMNS; c++) {
        const TripStats& t = *trips[c];
        snprintf(buf, sizeof(buf), "%.1f mi", t.miles());
        milesField[c].draw(buf, COL_TEXT);

        formatDuration(buf, sizeof(buf), t.movingMs());
        rowField[c][0].draw(buf, COL_TEXT);
        double hours = t.movingMs() / 3600000.0;
        if (hours > 0.01) snprintf(buf, sizeof(buf), "%d mph", (int)(t.miles() / hours + 0.5));
        else snprintf(buf, sizeof(buf), "--");
        rowField[c][1].draw(buf, COL_TEXT);
        snprintf(buf, sizeof(buf), "%d mph", t.maxSpeedMph());
        rowField[c][2].draw(buf, COL_TEXT);
        if (t.maxRpm() > 0) thousands(buf, sizeof(buf), t.maxRpm());
        else snprintf(buf, sizeof(buf), "--");
        rowField[c][3].draw(buf, COL_TEXT);
        if (t.maxCoolantF() > 0) snprintf(buf, sizeof(buf), "%d F", t.maxCoolantF());
        else snprintf(buf, sizeof(buf), "--");
        rowField[c][4].draw(buf, COL_TEXT);
        thousands(n, sizeof(n), t.gainFt());
        snprintf(buf, sizeof(buf), "+%s ft", n);
        rowField[c][5].draw(buf, COL_TEXT);
        thousands(n, sizeof(n), t.lossFt());
        snprintf(buf, sizeof(buf), "-%s ft", n);
        rowField[c][6].draw(buf, COL_TEXT);
    }
}

int tripResetAt(int x, int y) {
    for (int c = TRIP_1; c <= TRIP_2; c++) {
        int bx = tripX(c) + 48;
        if (x >= bx && x < bx + TRIP_W - 96 && y >= RESET_Y - 10 && y < RESET_Y + RESET_H + 10) return c;
    }
    return -1;
}

// ---- Charts ----

struct ChartSpec {
    const char* title;
    const char* unit;
    const uint16_t& color;
    bool fromZero;  // speed and rpm: the scale starts at 0
    int minSpan;    // smallest range shown, so a flat line isn't all noise
};

static const ChartSpec SPECS[4] = {
    {"SPEED", "mph", COL_SPEED, true, 20},
    {"RPM", "", COL_AMBER, true, 2000},
    {"COOLANT", "F", COL_IDLE, false, 20},
    {"ELEVATION", "ft", COL_GREEN, false, 200},
};

static const int CHART_W = 388, CHART_H = 202;
static int chartX(int i) { return 8 + (i % 2) * (CHART_W + 8); }
static int chartY(int i) { return 12 + (i / 2) * (CHART_H + 8); }

static void layoutChart(int i) {
    int x = chartX(i), y = chartY(i);
    lcd.fillSmoothRoundRect(x, y, CHART_W, CHART_H, 12, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(x + 1, y + 1, CHART_W - 2, CHART_H - 2, 11, COL_PANEL);
    useFont(lcd, FONT_S);
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.setTextColor(SPECS[i].color, COL_PANEL);
    lcd.drawString(SPECS[i].title, x + 14, y + 18);
}

static void drawChart(int i, const ChartSeries<CHART_POINTS>& s) {
    const ChartSpec& spec = SPECS[i];
    int x = chartX(i), y = chartY(i);
    int px0 = x + 58, px1 = x + CHART_W - 14, py0 = y + 38, py1 = y + CHART_H - 30;

    // Clear everything that changes: the plot, its scale, the latest value
    lcd.fillRect(x + 4, y + 30, CHART_W - 8, CHART_H - 34, COL_PANEL);
    lcd.fillRect(x + CHART_W / 2, y + 4, CHART_W / 2 - 10, 26, COL_PANEL);

    int16_t lo, hi;
    useFont(lcd, FONT_S);
    if (!s.range(lo, hi)) {
        lcd.setTextDatum(textdatum_t::middle_center);
        lcd.setTextColor(COL_LABEL, COL_PANEL);
        lcd.drawString("No data yet", x + CHART_W / 2, y + CHART_H / 2);
        return;
    }
    int16_t latest = CHART_NO_DATA, peak = hi;
    for (int k = s.size() - 1; k >= 0 && latest == CHART_NO_DATA; k--) latest = s.at(k);

    // Scale: from 0 for speed and rpm, else around the data; at least minSpan
    int low = spec.fromZero ? 0 : lo, high = hi;
    if (high - low < spec.minSpan) {
        int pad = spec.minSpan - (high - low);
        if (spec.fromZero) high = low + spec.minSpan;
        else {
            low -= pad / 2;
            high = low + spec.minSpan;
        }
    }
    auto yFor = [&](int v) { return py1 - (float)(v - low) * (py1 - py0) / (high - low); };

    // Gridlines with their values at the bottom, middle and top
    char buf[24], n[16];
    lcd.setTextDatum(textdatum_t::middle_right);
    lcd.setTextColor(COL_LABEL, COL_PANEL);
    for (int g = 0; g <= 2; g++) {
        int v = low + (high - low) * g / 2;
        int gy = yFor(v);
        lcd.drawFastHLine(px0, gy, px1 - px0, COL_PANEL_EDGE);
        thousands(n, sizeof(n), v);
        lcd.drawString(n, px0 - 8, gy);
    }

    // The line, with gaps where there was no data
    int count = s.size();
    for (int k = 1; k < count; k++) {
        int16_t a = s.at(k - 1), b = s.at(k);
        if (a == CHART_NO_DATA || b == CHART_NO_DATA) continue;
        float xa = px0 + (float)(k - 1) * (px1 - px0) / (count - 1);
        float xb = px0 + (float)k * (px1 - px0) / (count - 1);
        lcd.drawWideLine(xa, yFor(a), xb, yFor(b), 1.3f, spec.color);
    }

    // Time along the bottom: start and how long it's been recording
    unsigned long spanMs = (unsigned long)count * s.samplesPerPoint() * CHART_SAMPLE_MS;
    lcd.setTextDatum(textdatum_t::middle_left);
    lcd.drawString("0:00", px0, py1 + 16);
    formatDuration(buf, sizeof(buf), spanMs);
    lcd.setTextDatum(textdatum_t::middle_right);
    lcd.drawString(buf, px1, py1 + 16);

    // Latest and highest, top right
    if (latest != CHART_NO_DATA) {
        char a[16], b[16];
        thousands(a, sizeof(a), latest);
        thousands(b, sizeof(b), peak);
        snprintf(buf, sizeof(buf), "%s %s   max %s", a, spec.unit, b);
        lcd.setTextColor(COL_TEXT, COL_PANEL);
        lcd.drawString(buf, x + CHART_W - 14, y + 18);
    }
}

void drawChartsPage(bool full, const RideCharts& charts) {
    const ChartSeries<CHART_POINTS>* series[4] = {&charts.speed, &charts.rpm, &charts.coolant,
                                                  &charts.elevation};
    for (int i = 0; i < 4; i++) {
        if (full) layoutChart(i);
        drawChart(i, *series[i]);
    }
}

void drawPageDots(int page, int count) {
    const int y = 472, gap = 18;
    int x0 = 400 - (count - 1) * gap / 2;
    for (int i = 0; i < count; i++) {
        lcd.fillCircle(x0 + i * gap, y, 5, COL_BG);
        if (i == page) lcd.fillSmoothCircle(x0 + i * gap, y, 4, COL_TEXT);
        else lcd.fillSmoothCircle(x0 + i * gap, y, 3, COL_PANEL_EDGE);
    }
}
