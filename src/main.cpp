// 5" dash: two round gauges (RPM and speed, with miles since gas fill
// under it) and, between them, turn-by-turn directions or a compass when
// there's no route. Status icons along the top; music along the bottom
// with the phone connected, ride stats without it. Shows the bike's live
// data; with no bike sending, it plays a simulated ride (and a sample route
// and song; tap the screen to step through phone states) so the look can be
// tuned on the bench.

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <string.h>
#include <sys/time.h>
#include "board_w5.h"
#include "can_bus.h"
#include "can_tracker.h"
#include "ch422g.h"
#include "dash_fonts.h"
#include "dash_logic.h"
#include "demo_ride.h"
#include "fuel_trip.h"
#include "gps_view.h"
#include "gauge.h"
#include "gps.h"
#include "gt911.h"
#include "icons.h"
#include "ktm_decode.h"
#include "lgfx_w5.h"
#include "press_detector.h"
#include "ride_clock.h"
#include "sd_log.h"
#include "style.h"
#include "trip_stats.h"

static const unsigned long GAUGE_FRAME_MS = 33;   // ~30 fps
static const unsigned long STRIP_FRAME_MS = 500;
static const unsigned long CLOCK_SYNC_MS = 60000;
static const unsigned long TOUCH_POLL_MS = 15;

static LGFX_W5 lcd;
static Gauges gauges(lcd);
static CanTracker canTracker;
static KtmState ktm;
static GpsView gpsView;
static DemoRide demo;
static bool clockSet = false;
static FuelTrip fuel;
static TripStats trip;      // this ride, from the bike's own data
static TripStats demoTrip;  // the simulated ride, kept apart from the real one
static bool inDemo = true;
// Tapping the demo screen steps through what the phone might be doing
enum DemoPhone { DEMO_ROUTE_AND_MUSIC, DEMO_MUSIC_ONLY, DEMO_NO_PHONE, DEMO_PHONE_STATES };
static int demoPhone = DEMO_ROUTE_AND_MUSIC;
static Preferences prefs;  // flash that keeps its contents with the key off

// The smooth fonts unpack each glyph on the stack, and a 150 px gear digit
// takes about 9 KB: more than the default 8 KB loop stack
SET_LOOP_TASK_STACK_SIZE(24 * 1024);

// ---- What the phone sends (sample data until the Bluetooth link is in) ----

struct NavView {
    bool active = false;
    int turn = 0;              // -1 left, 0 straight on, 1 right
    char distance[16] = "";    // to the next turn: "0.3 mi"
    char street[40] = "";
    char eta[16] = "";         // "12:52 PM"
    char remaining[32] = "";   // "14.2 mi  23 min"
};

struct MusicView {
    bool playing = false;
    char title[64] = "";
    char artist[64] = "";
};

static NavView nav;
static MusicView music;
static bool phoneConnected = false;
static int phoneBattery = -1;  // percent; -1 = no phone

// Nothing from the phone (until the Bluetooth link is in, the only state
// outside the demo)
static void clearPhone() {
    phoneConnected = false;
    nav = NavView();
    music = MusicView();
    phoneBattery = -1;
}

// A sample route: counts down to each turn, then moves on to the next
static void updateDemoPhone(unsigned long nowMs) {
    if (demoPhone == DEMO_NO_PHONE) {
        clearPhone();
        return;
    }
    phoneConnected = true;
    nav.active = demoPhone == DEMO_ROUTE_AND_MUSIC;
    struct Leg { int turn; const char* street; };
    static const Leg LEGS[] = {
        {1, "Main St"}, {-1, "Canyon Rd"}, {0, "Ridge Loop"}, {1, "Forest Rd 157"},
    };
    const unsigned long LEG_MS = 9000;
    int leg = (nowMs / LEG_MS) % 4;
    float tenths = 8 - (nowMs % LEG_MS) * 8 / LEG_MS;  // 0.8 mi down to 0.1
    nav.turn = LEGS[leg].turn;
    snprintf(nav.distance, sizeof(nav.distance), "0.%d mi", (int)tenths);
    snprintf(nav.street, sizeof(nav.street), "%s", LEGS[leg].street);
    snprintf(nav.eta, sizeof(nav.eta), "12:52 PM");
    int miles10 = 142 - (int)(nowMs / 3000 % 100);
    snprintf(nav.remaining, sizeof(nav.remaining), "%d.%d mi   23 min", miles10 / 10, miles10 % 10);

    music.playing = true;
    snprintf(music.title, sizeof(music.title), "Highway Tune");
    snprintf(music.artist, sizeof(music.artist), "Greta Van Fleet");
    phoneBattery = 82;
}

// ---- Text that only repaints when it changes ----

struct Field {
    int x, y, w, h;           // box cleared before drawing; text sits in it
    textdatum_t datum;        // middle_left, middle_center or middle_right
    DashFont font;
    uint16_t bg;
    char shown[64];
    uint16_t shownColor;

    void draw(const char* text, uint16_t color) {
        if (strcmp(text, shown) == 0 && color == shownColor) return;
        snprintf(shown, sizeof(shown), "%s", text);
        shownColor = color;
        lcd.fillRect(x, y - h / 2, w, h, bg);
        useFont(lcd, font);
        lcd.setTextDatum(datum);
        lcd.setTextColor(color, bg);
        // Shorten anything too long for its box
        char fit[64];
        snprintf(fit, sizeof(fit), "%s", text);
        int len = strlen(fit);
        while (len > 3 && lcd.textWidth(fit) > w) {
            fit[--len] = '\0';
            memcpy(fit + len - 3, "...", 3);
        }
        int tx = datum == textdatum_t::middle_left ? x
               : datum == textdatum_t::middle_right ? x + w : x + w / 2;
        // The font paints its background a full line high: keep it in the box
        lcd.setClipRect(x, y - h / 2, w, h);
        lcd.drawString(fit, tx, y);
        lcd.clearClipRect();
    }
};

static Field fCoolant{48, TOP_STRIP_Y, 100, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static Field fElevation{206, TOP_STRIP_Y, 120, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static Field fTime{300, TOP_STRIP_Y, 200, 50, textdatum_t::middle_center, FONT_L, COL_BG, "", 0};
static Field fBattery{620, TOP_STRIP_Y, 60, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static Field fSats{722, TOP_STRIP_Y, 60, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};

static const int CP_X = CENTER_PANEL_X, CP_W = CENTER_PANEL_W;
static const int CP_MID = CP_X + CP_W / 2;
static Field fNavDistance{CP_X + 4, 208, CP_W - 8, 56, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0};
static Field fNavStreet{CP_X + 4, 254, CP_W - 8, 36, textdatum_t::middle_center, FONT_M, COL_PANEL, "", 0};
static Field fEta{CP_X + 4, 306, CP_W - 8, 34, textdatum_t::middle_center, FONT_M, COL_PANEL, "", 0};
static Field fRemaining{CP_X + 4, 336, CP_W - 8, 26, textdatum_t::middle_center, FONT_S, COL_PANEL, "", 0};

// Icons that change colour or shape, as last drawn; -1 = draw again
static const int NO_ARROW = 99;  // (-1 is a left turn)
static int phoneShown = -1, satShown = -1, arrowShown = NO_ARROW, playShown = -1;

// Compass mode of the centre panel (no route)
static const int COMPASS_SIZE = 140, COMPASS_R = 60;
static const int COMPASS_Y = 84;  // top of the compass on screen
static LGFX_Sprite compass(&lcd);  // internal RAM, like the gauge centres
static Field fHeading{CP_X + 4, 252, CP_W - 8, 48, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0};

static const int BS_Y = BOTTOM_STRIP_Y, BS_H = 76, BS_MID = BS_Y + BS_H / 2;
static const int BTN_PREV_X = 584, BTN_PLAY_X = 664, BTN_NEXT_X = 744;  // spaced for gloves
static Field fTitle{40, BS_MID - 13, 500, 32, textdatum_t::middle_left, FONT_M, COL_PANEL, "", 0};
static Field fArtist{40, BS_MID + 17, 500, 26, textdatum_t::middle_left, FONT_S, COL_PANEL, "", 0};

// Ride stats in the bottom bar when there's no phone: four columns
static const int STAT_W = 192;
static const char* const STAT_LABELS[4] = {"TRIP", "MOVING", "AVG", "TOP"};
static Field fStat[4] = {
    {16 + 0 * STAT_W + 6, BS_Y + 50, STAT_W - 12, 44, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0},
    {16 + 1 * STAT_W + 6, BS_Y + 50, STAT_W - 12, 44, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0},
    {16 + 2 * STAT_W + 6, BS_Y + 50, STAT_W - 12, 44, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0},
    {16 + 3 * STAT_W + 6, BS_Y + 50, STAT_W - 12, 44, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0},
};

// What the centre panel and bottom bar are laid out for; -1 = lay out again
static int centreMode = -1;  // 1 = route, 0 = compass
static int barMode = -1;     // 1 = music, 0 = ride stats

// ---- Screen layout ----

static void drawPanels() {
    lcd.fillSmoothRoundRect(CP_X, GAUGE_Y + 6, CP_W, GAUGE_SIZE - 12, 14, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(CP_X + 1, GAUGE_Y + 7, CP_W - 2, GAUGE_SIZE - 14, 13, COL_PANEL);
    lcd.fillSmoothRoundRect(16, BS_Y, 768, BS_H, 16, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(17, BS_Y + 1, 766, BS_H - 2, 15, COL_PANEL);

    iconCoolant(lcd, 26, TOP_STRIP_Y, COL_IDLE);
    iconMountain(lcd, 184, TOP_STRIP_Y, COL_LABEL);
}

// Screen re-laid out from scratch: forget what every field showed
static void drawScreen() {
    lcd.fillScreen(COL_BG);
    gauges.drawDials();
    drawPanels();
    for (Field* f : {&fCoolant, &fElevation, &fTime, &fBattery, &fSats, &fNavDistance, &fNavStreet,
                     &fTitle, &fArtist, &fEta, &fRemaining}) {
        f->shown[0] = '\0';
    }
    phoneShown = satShown = playShown = -1;
    arrowShown = NO_ARROW;
    centreMode = barMode = -1;
}

static void bootHardware() {
    Serial.begin(115200);
    delay(1500);  // let the USB serial port come up
    Serial.println("# 5\" dash");
    setenv("TZ", MOUNTAIN_TZ, 1);
    tzset();

    Serial.printf("# CH422G %s\n", ch422gBegin() ? "ok" : "NOT RESPONDING");
    ch422gWrite(EXIO_LCD_RST, false);
    delay(10);
    ch422gWrite(EXIO_LCD_RST, true);
    delay(100);
    Serial.printf("# display %s\n", lcd.init() ? "ok" : "FAILED");
    lcd.fillScreen(COL_BG);
    ch422gWrite(EXIO_BACKLIGHT, true);
    Serial.printf("# gauges %s\n", gauges.begin() ? "ok" : "OUT OF RAM");
    compass.setPsram(false);
    compass.setColorDepth(16);
    if (!compass.createSprite(COMPASS_SIZE, COMPASS_SIZE)) Serial.println("# compass: OUT OF RAM");
    drawScreen();
    Serial.printf("# touch %s\n", gt911Begin() ? "ok" : "NOT RESPONDING");

    ch422gWrite(EXIO_SD_CS, true);  // SD mode: the card's D3 line must be high
    if (sdLogBegin()) Serial.printf("# SD recording to %s\n", sdLogStatus().fileName);
    else Serial.println("# SD: no card");
    Serial.printf("# CAN %s\n", canBegin() ? "listen-only 500k" : "FAILED TO START");
    gpsBegin();
    prefs.begin("dash");
    if (prefs.isKey("fuelMi")) fuel.restore(prefs.getDouble("fuelMi"));
    Serial.printf("# %.1f mi since gas fill\n", fuel.miles());
    Serial.printf("# free internal RAM %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void setup() {
    bootHardware();
}

// Clock from GPS time; also gives SD log files real dates
static void syncClock(unsigned long nowMs) {
    static unsigned long lastSyncMs = 0;
    if (clockSet && nowMs - lastSyncMs < CLOCK_SYNC_MS) return;
    time_t utc;
    if (!gpsUtcTime(utc)) return;
    struct timeval tv = {utc, 0};
    settimeofday(&tv, nullptr);
    clockSet = true;
    lastSyncMs = nowMs;
}

static void drawTopStrip(bool demoMode, unsigned long nowMs) {
    char buf[32];
    if (demoMode) snprintf(buf, sizeof(buf), "212 F");
    else if (ktmCoolantLive(ktm, nowMs)) snprintf(buf, sizeof(buf), "%d F", ktm.coolantF);
    else snprintf(buf, sizeof(buf), "-- F");
    fCoolant.draw(buf, COL_TEXT);

    if (gpsView.fix) snprintf(buf, sizeof(buf), "%d,%03d ft", gpsView.elevationFt / 1000,
                              gpsView.elevationFt % 1000);
    else snprintf(buf, sizeof(buf), demoMode ? "4,602 ft" : "-- ft");
    fElevation.draw(buf, COL_TEXT);

    if (clockSet) formatClock(buf, sizeof(buf), time(nullptr));
    else snprintf(buf, sizeof(buf), demoMode ? "12:25 PM" : "--:--");
    fTime.draw(buf, COL_TEXT);

    bool phone = phoneBattery >= 0;
    if (phone != phoneShown) {
        phoneShown = phone;
        iconPhone(lcd, 604, TOP_STRIP_Y, phone ? COL_ACCENT : COL_LABEL, COL_BG);
    }
    if (phone) snprintf(buf, sizeof(buf), "%d%%", phoneBattery);
    else snprintf(buf, sizeof(buf), "--");
    fBattery.draw(buf, phone ? COL_TEXT : COL_LABEL);

    uint16_t sc = gpsView.fix ? COL_GREEN : COL_LABEL;
    if (sc != satShown) {
        satShown = sc;
        lcd.fillRect(684, TOP_STRIP_Y - 20, 36, 40, COL_BG);
        iconSatellite(lcd, 702, TOP_STRIP_Y, sc);
    }
    snprintf(buf, sizeof(buf), "%d", gpsView.satellites);
    fSats.draw(buf, gpsView.fix ? COL_TEXT : COL_LABEL);
}

// Which way the bike is going. GPS only knows while moving; stopped, the
// compass holds the last heading, greyed out.
struct Heading {
    int deg;
    bool live;   // moving: deg is current
    bool known;  // there's been a heading since power-up
};

static Heading currentHeading(unsigned long nowMs) {
    static bool seen = false;
    if (inDemo) return {(int)(nowMs / 150 % 360), true, true};  // slowly turning
    bool live = gpsView.fix && gpsView.speedMph >= GPS_HEADING_MIN_MPH;
    if (live) seen = true;
    return {gpsView.headingDeg, live, seen};
}

// A compass card that turns under a fixed pointer at the top
static void drawCompass(const Heading& h) {
    static int shownDeg = -1, shownLive = -1;
    if (shownDeg == h.deg && shownLive == h.live && centreMode == 0) return;
    shownDeg = h.deg;
    shownLive = h.live;

    LGFX_Sprite& s = compass;
    const int cx = COMPASS_SIZE / 2, cy = COMPASS_SIZE / 2 + 6;
    uint16_t ink = h.live ? COL_TEXT : COL_LABEL;
    s.fillScreen(COL_PANEL);
    s.fillSmoothCircle(cx, cy, COMPASS_R, COL_PANEL_EDGE);
    s.fillSmoothCircle(cx, cy, COMPASS_R - 3, COL_PANEL);
    s.fillTriangle(cx - 9, 0, cx + 9, 0, cx, 16, h.live ? COL_ACCENT : COL_LABEL);
    useFont(s, FONT_S);
    s.setTextDatum(textdatum_t::middle_center);
    static const char* const POINTS[4] = {"N", "E", "S", "W"};
    for (int a = 0; a < 360; a += 30) {
        // the bike's heading sits at the top (screen angle -90)
        float r = (a - (h.known ? h.deg : 0) - 90) * (float)M_PI / 180.0f;
        float ca = cosf(r), sa = sinf(r);
        bool point = a % 90 == 0;
        int r0 = COMPASS_R - 6, r1 = COMPASS_R - (point ? 13 : 10);
        s.drawWideLine(cx + ca * r0, cy + sa * r0, cx + ca * r1, cy + sa * r1, point ? 1.5f : 1.0f, ink);
        if (point) {
            s.setTextColor(a == 0 && h.live ? COL_RED : ink);
            s.drawString(POINTS[a / 90], cx + ca * (COMPASS_R - 26), cy + sa * (COMPASS_R - 26));
        }
    }
    useFont(s, FONT_M);
    s.setTextColor(ink);
    s.drawString(h.known ? headingCardinal(h.deg) : "--", cx, cy);
    s.pushSprite(&lcd, CP_MID - COMPASS_SIZE / 2, COMPASS_Y);
}

static void drawCentrePanel(unsigned long nowMs) {
    int mode = nav.active ? 1 : 0;
    if (mode != centreMode) {
        lcd.fillRect(CP_X + 6, GAUGE_Y + 10, CP_W - 12, GAUGE_SIZE - 20, COL_PANEL);
        lcd.drawFastHLine(CP_X + 20, 282, CP_W - 40, COL_PANEL_EDGE);
        for (Field* f : {&fNavDistance, &fNavStreet, &fEta, &fRemaining, &fHeading}) f->shown[0] = '\0';
        arrowShown = NO_ARROW;
    }

    if (nav.active) {
        centreMode = 1;
        if (nav.turn != arrowShown) {
            arrowShown = nav.turn;
            lcd.fillRect(CP_X + 8, 90, CP_W - 16, 90, COL_PANEL);
            iconTurn(lcd, CP_MID, 134, nav.turn, COL_ACCENT);
        }
        fNavDistance.draw(nav.distance, COL_TEXT);
        fNavStreet.draw(nav.street, COL_LABEL);
        char buf[32];
        snprintf(buf, sizeof(buf), "ETA %s", nav.eta);
        fEta.draw(buf, COL_TEXT);
        fRemaining.draw(nav.remaining, COL_LABEL);
        return;
    }

    Heading h = currentHeading(nowMs);
    drawCompass(h);
    centreMode = 0;
    char buf[16];
    if (h.known) snprintf(buf, sizeof(buf), "%d°", h.deg);
    else snprintf(buf, sizeof(buf), "--");
    fHeading.draw(buf, h.live ? COL_TEXT : COL_LABEL);

    // Under the line: the ride so far, or (when the bar below already
    // shows that) why there's no route
    if (phoneConnected) {
        const TripStats& t = inDemo ? demoTrip : trip;
        snprintf(buf, sizeof(buf), "%.1f mi", t.miles());
        fEta.draw(buf, COL_TEXT);
        fRemaining.draw("No route", COL_LABEL);
    } else {
        fEta.draw("No phone", COL_LABEL);
        fRemaining.draw("", COL_LABEL);
    }
}

static void drawMusicBar() {
    if (barMode != 1) {
        barMode = 1;
        lcd.fillRect(24, BS_Y + 4, 752, BS_H - 8, COL_PANEL);
        iconPrevious(lcd, BTN_PREV_X, BS_MID, COL_TEXT);
        iconNext(lcd, BTN_NEXT_X, BS_MID, COL_TEXT);
        fTitle.shown[0] = fArtist.shown[0] = '\0';
        playShown = -1;
    }
    if (music.playing != playShown) {
        playShown = music.playing;
        lcd.fillSmoothCircle(BTN_PLAY_X, BS_MID, 26, COL_ACCENT);
        if (music.playing) iconPause(lcd, BTN_PLAY_X, BS_MID, COL_PANEL);
        else iconPlay(lcd, BTN_PLAY_X + 2, BS_MID, COL_PANEL);
    }
    fTitle.draw(music.title[0] ? music.title : "Nothing playing", music.title[0] ? COL_TEXT : COL_LABEL);
    fArtist.draw(music.artist, COL_LABEL);
}

static void drawStatsBar() {
    if (barMode != 0) {
        barMode = 0;
        lcd.fillRect(24, BS_Y + 4, 752, BS_H - 8, COL_PANEL);
        useFont(lcd, FONT_S);
        lcd.setTextDatum(textdatum_t::middle_center);
        lcd.setTextColor(COL_LABEL, COL_PANEL);
        for (int i = 0; i < 4; i++) {
            lcd.drawString(STAT_LABELS[i], 16 + i * STAT_W + STAT_W / 2, BS_Y + 15);
            if (i > 0) lcd.drawFastVLine(16 + i * STAT_W, BS_Y + 14, BS_H - 28, COL_PANEL_EDGE);
            fStat[i].shown[0] = '\0';
        }
    }
    const TripStats& t = inDemo ? demoTrip : trip;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f mi", t.miles());
    fStat[0].draw(buf, COL_TEXT);
    formatDuration(buf, sizeof(buf), t.movingMs());
    fStat[1].draw(buf, COL_TEXT);
    double hours = t.movingMs() / 3600000.0;
    if (hours > 0) snprintf(buf, sizeof(buf), "%d mph", (int)(t.miles() / hours + 0.5));
    else snprintf(buf, sizeof(buf), "--");
    fStat[2].draw(buf, COL_TEXT);
    snprintf(buf, sizeof(buf), "%d mph", t.maxSpeedMph());
    fStat[3].draw(buf, COL_TEXT);
}

// ---- Miles since gas fill: tap it to reset ----

struct Box {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// The bottom of the speed dial, where the count is shown
static const Box FUEL_TAP{SPEED_GAUGE_X + 50, GAUGE_Y + GAUGE_SIZE / 2 + 60, GAUGE_SIZE - 100,
                          GAUGE_SIZE / 2 - 60};
static const Box POPUP{180, 96, 440, 280};
static const Box BTN_YES{204, 282, 188, 74};   // big and far apart for gloves
static const Box BTN_NO{408, 282, 188, 74};
static const unsigned long POPUP_TIMEOUT_MS = 10000;  // closes itself as "No"

static bool popupOpen = false;
static unsigned long popupMs = 0;

static void saveFuel() {
    prefs.putDouble("fuelMi", fuel.miles());
    fuel.markSaved();
}

static void button(const Box& b, const char* label, uint16_t color) {
    lcd.fillSmoothRoundRect(b.x, b.y, b.w, b.h, 14, color);
    lcd.setTextColor(COL_TEXT, color);
    lcd.drawString(label, b.x + b.w / 2, b.y + b.h / 2);
}

static void openFuelPopup(unsigned long nowMs) {
    popupOpen = true;
    popupMs = nowMs;
    lcd.fillSmoothRoundRect(POPUP.x - 2, POPUP.y - 2, POPUP.w + 4, POPUP.h + 4, 20, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(POPUP.x, POPUP.y, POPUP.w, POPUP.h, 18, COL_PANEL);
    iconFuel(lcd, 400, POPUP.y + 30, COL_ACCENT, COL_PANEL);
    useFont(lcd, FONT_M);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(COL_TEXT, COL_PANEL);
    lcd.drawString("Do you want to reset", 400, POPUP.y + 72);
    lcd.drawString("miles since gas fill?", 400, POPUP.y + 104);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f mi", fuel.miles());
    useFont(lcd, FONT_L);
    lcd.setTextColor(COL_ACCENT, COL_PANEL);
    lcd.drawString(buf, 400, POPUP.y + 150);
    useFont(lcd, FONT_M);
    button(BTN_YES, "Yes, reset", COL_RED);
    button(BTN_NO, "No", COL_PANEL_EDGE);
}

static void closeFuelPopup() {
    popupOpen = false;
    drawScreen();
}

static void onTap(int x, int y, unsigned long nowMs) {
    if (popupOpen) {
        if (BTN_YES.contains(x, y)) {
            fuel.reset();
            saveFuel();
            closeFuelPopup();
        } else if (BTN_NO.contains(x, y)) {
            closeFuelPopup();
        }
        return;
    }
    if (FUEL_TAP.contains(x, y)) openFuelPopup(nowMs);
    else if (inDemo) demoPhone = (demoPhone + 1) % DEMO_PHONE_STATES;
}

// Taps, at the spot the finger first came down
static void pollTouch(unsigned long nowMs) {
    static PressDetector press;
    static bool wasTouching = false;
    static int downX = 0, downY = 0;
    int x, y;
    bool touching = gt911Read(x, y);
    if (touching && !wasTouching) {
        downX = x;
        downY = y;
    }
    wasTouching = touching;
    if (press.update(touching, nowMs) == PRESS_TAP) onTap(downX, downY, nowMs);
}

void loop() {
    unsigned long now = millis();
    canPoll(canTracker, ktm, now);
    canTracker.updateRates(now);
    gpsPoll();

    static unsigned long lastGpsMs = 0;
    if (now - lastGpsMs >= 100) {
        lastGpsMs = now;
        gpsUpdateView(gpsView, gpsReading());
        syncClock(now);

        // Only real wheel speed counts toward the tank, never the demo
        static unsigned long lastFuelMs = now;
        uint32_t dt = now - lastFuelMs;
        lastFuelMs = now;
        fuel.ride(dt, ktmSpeedLive(ktm, now), ktm.speedMph);
        if (fuel.needsSave()) saveFuel();

        TripSample sample;
        sample.speedLive = ktmSpeedLive(ktm, now);
        sample.speedMph = ktm.speedMph;
        sample.rpmLive = ktmRpmLive(ktm, now);
        sample.rpm = ktm.rpm;
        sample.coolantLive = ktmCoolantLive(ktm, now);
        sample.coolantF = ktm.coolantF;
        trip.update(dt, sample);
        if (!sample.rpmLive) {  // the simulated ride
            TripSample d;
            d.speedLive = d.rpmLive = true;
            d.speedMph = demo.mph;
            d.rpm = demo.rpm;
            demoTrip.update(dt, d);
        }
    }

    static unsigned long lastTouchMs = 0;
    if (now - lastTouchMs >= TOUCH_POLL_MS) {
        lastTouchMs = now;
        pollTouch(now);
    }
    if (popupOpen) {  // the dash waits behind the pop-up
        if (now - popupMs >= POPUP_TIMEOUT_MS) closeFuelPopup();
        return;
    }

    // Live data when the bike is talking, the simulated ride otherwise
    bool demoMode = !ktmRpmLive(ktm, now);
    inDemo = demoMode;
    if (demoMode) {
        demo.update(now);
        updateDemoPhone(now);
    } else {
        clearPhone();  // until the Bluetooth link is in
    }

    static unsigned long lastGaugeMs = 0;
    if (now - lastGaugeMs >= GAUGE_FRAME_MS) {
        lastGaugeMs = now;
        int rpm = demoMode ? demo.rpm : ktm.rpm;
        ShiftStage shift = shiftStage(rpm, SHIFT_SOON_RPM, SHIFT_NOW_RPM);
        bool flashOn = (now / SHIFT_FLASH_MS) % 2 == 0;
        int gear = demoMode ? demo.gear : (ktmGearLive(ktm, now) ? ktm.gear : -1);
        gauges.drawRpm(RpmView{rpm, gear, true, shift, flashOn});

        SpeedView sp;
        sp.live = demoMode || ktmSpeedLive(ktm, now);
        sp.mph = demoMode ? demo.mph : ktm.speedMph;
        sp.fuelTenths = (int)(fuel.miles() * 10 + 1e-6);  // shown like an odometer, never rounded up
        gauges.drawSpeed(sp);
    }

    static unsigned long lastCentreMs = 0;  // often enough for a smooth compass
    if (now - lastCentreMs >= 100) {
        lastCentreMs = now;
        drawCentrePanel(now);
    }

    static unsigned long lastStripMs = 0;
    if (now - lastStripMs >= STRIP_FRAME_MS) {
        lastStripMs = now;
        drawTopStrip(demoMode, now);
        if (phoneConnected) drawMusicBar();
        else drawStatsBar();
    }
}
