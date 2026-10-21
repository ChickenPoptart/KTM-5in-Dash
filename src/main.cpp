// 5" dash: two round gauges (RPM and speed, with miles since gas fill
// under it) and, between them, turn-by-turn directions or a compass when
// there's no route. Status icons along the top; music along the bottom
// with the phone connected, ride stats without it. Shows the bike's live
// data; with no bike sending, it plays a simulated ride (and a sample route
// and song; tap the screen to step through phone states) until a real phone
// connects through the Chronos app so the look can be
// tuned on the bench. The demo is off unless switched on (hold the screen
// for a second); then it runs only while the bike isn't sending.

#include <Arduino.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <Wire.h>
#include <string.h>
#include <sys/time.h>
#include "board_w5.h"
#include "can_bus.h"
#include "can_tracker.h"
#include "ch422g.h"
#include "dash_fonts.h"
#include "field.h"
#include "dash_logic.h"
#include "demo_ride.h"
#include "alert_banner.h"
#include "eta.h"
#include "fuel_trip.h"
#include "gps_view.h"
#include "icon_scale.h"
#include "gauge.h"
#include "gps.h"
#include "gt911.h"
#include "icons.h"
#include "ktm_decode.h"
#include "phone.h"
#include "lgfx_w5.h"
#include "maneuver.h"
#include "nav_icons.h"
#include "gesture.h"
#include "pages.h"
#include "retro_dash.h"
#include "ride_clock.h"
#include "sd_log.h"
#include "style.h"
#include "tap_sequencer.h"
#include "text_wrap.h"
#include "trip_stats.h"

static const unsigned long GAUGE_FRAME_MS = 33;   // ~30 fps
static const unsigned long STRIP_FRAME_MS = 500;
static const unsigned long CLOCK_SYNC_MS = 60000;
static const unsigned long TOUCH_POLL_MS = 15;

LGFX_W5 lcd;  // (field.h and pages.cpp draw on it too)
static Gauges gauges(lcd);
static RetroDash retro(lcd);  // the retro look's dash: its own layout and drawing

static bool retroShown() { return theme() == THEME_RETRO && retro.active(); }

// Only one dash's digit buffers fit in internal RAM at once
static void applyLook() {
    if (theme() == THEME_RETRO) {
        gauges.end();
        if (!retro.begin()) {
            Serial.println("# retro dash: OUT OF RAM, staying modern");
            gauges.begin();
        }
    } else {
        retro.end();
        gauges.begin();
    }
}
static CanTracker canTracker;
static KtmState ktm;
static GpsView gpsView;
static DemoRide demo;
static bool clockSet = false;
static FuelTrip fuel;
static TripStats trip;      // this ride, from the bike's own data
static TripStats demoTrip;  // the simulated ride, kept apart from the real one
static TripStats trip1, trip2;  // car-style trip meters: kept in flash until reset
static RideCharts charts;       // this ride, from the bike and the GPS
static RideCharts demoCharts;

// The pages, in swipe order: swipe left for the next, right for the one before
enum Page { PAGE_DASH, PAGE_TRIPS, PAGE_CHARTS, PAGE_COUNT };
static int page = PAGE_DASH;
static bool popupOpen = false;  // a pop-up is up: the page behind it waits
static void showPage(int p);
static bool inDemo = false;      // showing the simulated ride right now
static bool demoEnabled = false; // hold the screen for 1 s to switch the demo on or off

// The demo (a simulated ride, route, song and calls, for tuning the screen
// and recording videos on the bench) is only in the bench build
// (pio run -e waveshare5_demo). On the bike a gloved long press must never
// put fake speed on the dash.
#ifdef DASH_DEMO
static const bool DEMO_BUILD = true;
#else
static const bool DEMO_BUILD = false;
#endif

// A frozen dash restarts itself after this long without the loop running
static const int WATCHDOG_S = 8;
// Tapping the demo screen steps through what the phone might be doing
enum DemoPhone { DEMO_ROUTE_AND_MUSIC, DEMO_MUSIC_ONLY, DEMO_NO_PHONE, DEMO_PHONE_STATES };
static int demoPhone = DEMO_ROUTE_AND_MUSIC;
static Preferences prefs;  // flash that keeps its contents with the key off

// For tracking down screen jitter: the last heavy thing the dash did, and
// a log line for every flash write (writing flash briefly stalls the memory
// the screen is read from)
static const char* lastHeavy = "";
static void heavy(const char* what) {
    lastHeavy = what;
}
static void flashWrite(const char* what) {
    heavy(what);
    Serial.printf("# flash write: %s at %lu ms\n", what, millis());
}

// The smooth fonts unpack each glyph on the stack, and a 150 px gear digit
// takes about 9 KB: more than the default 8 KB loop stack
SET_LOOP_TASK_STACK_SIZE(24 * 1024);

// ---- What the phone sends: the real phone when it's connected, otherwise
// (on the bench) the demo's sample route and song ----

static PhoneState phone;
static bool realPhone = false;   // a phone is connected over Bluetooth
static AlertBanner banner;       // calls and messages
static bool demoPlaying = true;  // the demo's play/pause button works too

static void clearPhone() { phone = PhoneState(); }

// A sample route: counts down to each turn, then moves on to the next
static void updateDemoPhone(unsigned long nowMs) {
    if (demoPhone == DEMO_NO_PHONE) {
        clearPhone();
        return;
    }
    phone.connected = true;
    phone.nav.active = demoPhone == DEMO_ROUTE_AND_MUSIC;
    phone.nav.hasIcon = false;
    // Directions worded the way Google Maps words them, one of each arrow
    static const char* const LEGS[] = {
        "Turn right onto Main St",
        "Turn left onto Canyon Rd",
        "Slight right onto Ridge Loop",
        "Sharp left onto Forest Rd 157",
        "Keep right at the fork",
        "Take exit 271 toward Main St",
        "Merge onto I-15 N",
        "At the roundabout, take the 2nd exit onto Main St",
        "Make a U-turn",
        "Continue onto Canyon Rd",
        "Your destination will be on the right",
    };
    const int LEG_COUNT = sizeof(LEGS) / sizeof(LEGS[0]);
    const unsigned long LEG_MS = 6000;
    int leg = (nowMs / LEG_MS) % LEG_COUNT;
    float tenths = 8 - (nowMs % LEG_MS) * 8 / LEG_MS;  // 0.8 mi down to 0.1
    phone.nav.turn = 0;
    snprintf(phone.nav.distance, sizeof(phone.nav.distance), "0.%d mi", (int)tenths);
    snprintf(phone.nav.street, sizeof(phone.nav.street), "%s", LEGS[leg]);
    snprintf(phone.nav.eta, sizeof(phone.nav.eta), "12:52 PM");
    int miles10 = 142 - (int)(nowMs / 3000 % 100);
    snprintf(phone.nav.remaining, sizeof(phone.nav.remaining), "%d.%d mi   23 min", miles10 / 10, miles10 % 10);

    phone.music.playing = demoPlaying;
    snprintf(phone.music.title, sizeof(phone.music.title), "Highway Tune");
    snprintf(phone.music.artist, sizeof(phone.music.artist), "Greta Van Fleet");
    phone.battery = 82;
    phone.charging = true;
    phone.weather.valid = true;
    phone.weather.temp = 54;
    phone.weather.high = 61;
    phone.weather.low = 40;
    phone.weather.icon = 1;
}

// The demo ride's coolant and road: warming up, then a slow climb and descent
static int demoCoolantF(unsigned long nowMs) {
    int warm = 140 + (int)(nowMs / 1000);
    return warm < 200 ? warm : 200 + (int)(8 * sinf(nowMs / 40000.0f));
}

static int demoElevationFt(unsigned long nowMs) {
    return 4600 + (int)(900 * sinf(nowMs / 300000.0f));
}

// The demo's phone gets a text, and then a call, every 40 s
static void demoAlerts(unsigned long nowMs) {
    static int lastSecond = -1;
    int second = nowMs / 1000 % 40;
    if (second == lastSecond) return;
    lastSecond = second;
    if (second == 12) banner.message("Messages", "Sam", "On my way, about 10 minutes out", nowMs);
    if (second == 26) banner.callRinging("Mom", nowMs);
    if (second == 34) banner.callEnded();
}

static void handlePhoneEvents(unsigned long nowMs) {
    PhoneEvent e;
    while (phoneNextEvent(e)) {
        Serial.printf("# phone event %d: [%s] [%s] [%s]\n", e.type, e.app, e.title, e.text);
        switch (e.type) {
        case PhoneEvent::CALL_RINGING:
            banner.callRinging(e.title, nowMs);
            if (page != PAGE_DASH && !popupOpen) showPage(PAGE_DASH);  // to see who it is
            break;
        case PhoneEvent::CALL_ENDED: banner.callEnded(); break;
        case PhoneEvent::MESSAGE: banner.message(e.app, e.title, e.text, nowMs); break;
        case PhoneEvent::CLOCK_SET: clockSet = true; break;
        }
    }
}


static Field fCoolant{48, TOP_STRIP_Y, 80, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static Field fElevation{166, TOP_STRIP_Y, 120, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static Field fTime{300, TOP_STRIP_Y, 200, 50, textdatum_t::middle_center, FONT_L, COL_BG, "", 0};
static Field fBattery{582, TOP_STRIP_Y, 54, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static Field fSats{684, TOP_STRIP_Y, 44, 34, textdatum_t::middle_left, FONT_M, COL_BG, "", 0};
static const int BRIGHT_ICON_X = 764;  // the sun: tap for the brightness pop-up

static const int CP_X = CENTER_PANEL_X, CP_W = CENTER_PANEL_W;
static const int CP_MID = CP_X + CP_W / 2;
static Field fNavDistance{CP_X + 4, 208, CP_W - 8, 56, textdatum_t::middle_center, FONT_L, COL_PANEL, "", 0};
static Field fNavStreet{CP_X + 4, 254, CP_W - 8, 36, textdatum_t::middle_center, FONT_M, COL_PANEL, "", 0};
// Directions too long for one line: two smaller lines in the same space
static Field fNavLine1{CP_X + 4, 247, CP_W - 8, 22, textdatum_t::middle_center, FONT_S, COL_PANEL, "", 0};
static Field fNavLine2{CP_X + 4, 269, CP_W - 8, 22, textdatum_t::middle_center, FONT_S, COL_PANEL, "", 0};
static int streetLines = 0;  // as laid out: 1 or 2; 0 = lay out again
static Field fEta{CP_X + 4, 306, CP_W - 8, 34, textdatum_t::middle_center, FONT_M, COL_PANEL, "", 0};
static Field fRemaining{CP_X + 4, 336, CP_W - 8, 26, textdatum_t::middle_center, FONT_S, COL_PANEL, "", 0};

// Icons that change colour or shape, as last drawn; -1 = draw again
static const uint32_t NO_ARROW = 0xFFFFFFFE;
static int phoneShown = -1, satShown = -1, playShown = -1, bannerShown = -1;
static uint32_t arrowShown = NO_ARROW;  // the phone's icon id, or the demo's turn

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

// Which the bar shows with the phone connected. Auto: music while there's
// a track (playing or paused), ride stats with no music app active. A tap
// on the bar picks one; a new track starting goes back to auto.
enum BarChoice { BAR_AUTO, BAR_MUSIC, BAR_STATS };
static BarChoice barChoice = BAR_AUTO;

static bool showMusicBar() {
    if (!phone.connected) return false;
    if (barChoice == BAR_AUTO) return phone.music.title[0] != '\0';
    return barChoice == BAR_MUSIC;
}

// ---- Screen layout ----

static void drawPanels() {
    lcd.fillSmoothRoundRect(CP_X, GAUGE_Y + 6, CP_W, GAUGE_SIZE - 12, 14, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(CP_X + 1, GAUGE_Y + 7, CP_W - 2, GAUGE_SIZE - 14, 13, COL_PANEL);
    lcd.fillSmoothRoundRect(16, BS_Y, 768, BS_H, 16, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(17, BS_Y + 1, 766, BS_H - 2, 15, COL_PANEL);

    iconCoolant(lcd, 26, TOP_STRIP_Y, COL_IDLE);
    iconMountain(lcd, 146, TOP_STRIP_Y, COL_LABEL);
    iconBrightness(lcd, BRIGHT_ICON_X, TOP_STRIP_Y, COL_TEXT);
}

// Screen re-laid out from scratch: forget what every field showed
static void drawScreen() {
    if (retroShown()) {
        retro.drawStatic();
        drawPageDots(PAGE_DASH, PAGE_COUNT);
        bannerShown = -1;
        return;
    }
    lcd.fillScreen(COL_BG);
    gauges.drawDials();
    drawPanels();
    for (Field* f : {&fCoolant, &fElevation, &fTime, &fBattery, &fSats, &fNavDistance, &fNavStreet,
                     &fTitle, &fArtist, &fEta, &fRemaining}) {
        f->shown[0] = '\0';
    }
    phoneShown = satShown = playShown = bannerShown = -1;
    drawPageDots(PAGE_DASH, PAGE_COUNT);
    arrowShown = NO_ARROW;
    centreMode = barMode = -1;
}

static const TripStats* const* tripColumns() {
    static const TripStats* cols[TRIP_COLUMNS];
    cols[TRIP_RIDE] = inDemo ? &demoTrip : &trip;
    cols[TRIP_1] = &trip1;
    cols[TRIP_2] = &trip2;
    return cols;
}

// The current page, from scratch
static void drawPage() {
    heavy("full page draw");
    if (page == PAGE_DASH) {
        drawScreen();
        return;
    }
    lcd.fillScreen(COL_BG);
    if (page == PAGE_TRIPS) drawTripsPage(true, tripColumns());
    else drawChartsPage(true, inDemo ? demoCharts : charts);
    drawPageDots(page, PAGE_COUNT);
}

static void showPage(int p) {
    page = p;
    drawPage();
}

// Trip 1 and Trip 2 in flash, as they are in memory (sizes checked, so a
// firmware change to TripStats starts them afresh rather than misreading)
static void loadTrip(const char* key, TripStats& t) {
    if (prefs.isKey(key) && prefs.getBytesLength(key) == sizeof(TripStats)) prefs.getBytes(key, &t, sizeof(t));
}

static void saveTrips() {
    flashWrite("trip meters");
    prefs.putBytes("trip1", &trip1, sizeof(trip1));
    prefs.putBytes("trip2", &trip2, sizeof(trip2));
}

static void bootHardware() {
    Serial.begin(115200);
    delay(1500);  // let the USB serial port come up
    Serial.printf("# 5\" dash, built %s %s%s\n", __DATE__, __TIME__, DEMO_BUILD ? " (bench build, with demo)" : "");
    prefs.begin("dash");
    setBrightness(prefs.getInt("bright", 100));
    setTheme(prefs.getUChar("theme", THEME_MODERN) == THEME_RETRO ? THEME_RETRO : THEME_MODERN);
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
    applyLook();
    Serial.printf("# dash: %s\n", retroShown() ? "retro" : "modern");
    compass.setPsram(false);
    compass.setColorDepth(16);
    if (!compass.createSprite(COMPASS_SIZE, COMPASS_SIZE)) Serial.println("# compass: OUT OF RAM");
    drawScreen();
    if (retroShown()) retro.selfTest();  // all segments on, like an 80s dash at key-on
    Serial.printf("# touch %s\n", gt911Begin() ? "ok" : "NOT RESPONDING");

    ch422gWrite(EXIO_SD_CS, true);  // SD mode: the card's D3 line must be high
    if (sdLogBegin()) Serial.printf("# SD recording to %s\n", sdLogStatus().fileName);
    else Serial.println("# SD: no card");
    Serial.printf("# CAN %s\n", canBegin() ? "listen-only 500k" : "FAILED TO START");
    gpsBegin();
    phoneBegin("KTM Dash");
    Serial.println("# phone: advertising as KTM Dash");
    demoEnabled = DEMO_BUILD && prefs.getBool("demo", false);
    if (prefs.isKey("fuelMi")) fuel.restore(prefs.getDouble("fuelMi"));
    Serial.printf("# %.1f mi since gas fill\n", fuel.miles());
    loadTrip("trip1", trip1);
    loadTrip("trip2", trip2);
    Serial.printf("# trip 1 %.1f mi, trip 2 %.1f mi\n", trip1.miles(), trip2.miles());
    Serial.printf("# free internal RAM %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void setup() {
    bootHardware();
    esp_task_wdt_init(WATCHDOG_S, true);  // reboot if the loop stops
    esp_task_wdt_add(nullptr);
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
    fTime.font = theme() == THEME_RETRO ? FONT_SEG_SMALL : FONT_L;
    if (theme() == THEME_RETRO) {  // seven-segment: digits only, no AM/PM
        char* space = strchr(buf, ' ');
        if (space) *space = '\0';
    }
    fTime.draw(buf, COL_TEXT);

    bool linked = phone.connected;
    int look = linked + 2 * phone.charging;
    if (look != phoneShown) {
        phoneShown = look;
        iconPhone(lcd, 566, TOP_STRIP_Y, linked ? COL_ACCENT : COL_LABEL, COL_BG);
        if (phone.charging) iconCharging(lcd, 566, TOP_STRIP_Y - 1, COL_GREEN);
    }
    if (linked && phone.battery >= 0) snprintf(buf, sizeof(buf), "%d%%", phone.battery);
    else snprintf(buf, sizeof(buf), "--");
    fBattery.draw(buf, linked ? COL_TEXT : COL_LABEL);

    uint16_t sc = gpsView.fix ? COL_GREEN : COL_LABEL;
    if (sc != satShown) {
        satShown = sc;
        lcd.fillRect(646, TOP_STRIP_Y - 20, 36, 40, COL_BG);
        iconSatellite(lcd, 664, TOP_STRIP_Y, sc);
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
    // At most 4 times a second, and only for a real change: each redraw is
    // a 40 KB push into the frame buffer
    static int shownDeg = -1000, shownLive = -1;
    static unsigned long shownMs = 0;
    int turned = abs(((h.deg - shownDeg) % 360 + 540) % 360 - 180);
    bool relayout = centreMode != 0;
    if (!relayout && h.live == shownLive && (turned < 2 || millis() - shownMs < 250)) return;
    shownMs = millis();
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

// Under the ETA: distance and time left if the phone sends them. Google
// Maps through Chronos only sends the ETA, so the minutes left come from
// the ETA and the clock (no way to know the miles).
static const char* routeLeft(const NavView& n, char* buf, int size) {
    if (n.remaining[0] || !clockSet) return n.remaining;
    time_t t = time(nullptr);
    struct tm local;
    localtime_r(&t, &local);
    int left = minutesUntil(n.eta, local.tm_hour * 60 + local.tm_min);
    if (left < 0) return "";
    if (left > 23 * 60) snprintf(buf, size, "Arriving");  // just past the ETA
    else if (left < 60) snprintf(buf, size, "%d min left", left);
    else snprintf(buf, size, "%d h %d min left", left / 60, left % 60);
    return buf;
}

static int smallTextWidth(const char* s) {
    useFont(lcd, FONT_S);
    return lcd.textWidth(s);
}

// The street (or what to do there): one line if it fits, else two smaller
static void drawStreet(const char* street) {
    useFont(lcd, FONT_M);
    int lines = lcd.textWidth(street) <= fNavStreet.w ? 1 : 2;
    if (lines != streetLines) {
        streetLines = lines;
        lcd.fillRect(CP_X + 4, 236, CP_W - 8, 44, COL_PANEL);
        fNavStreet.shown[0] = fNavLine1.shown[0] = fNavLine2.shown[0] = '\0';
    }
    if (lines == 1) {
        fNavStreet.draw(street, COL_LABEL);
        return;
    }
    char a[64], b[64];
    wrapTwoLines(street, fNavLine1.w, smallTextWidth, a, b, sizeof(a));
    fNavLine1.draw(a, COL_LABEL);
    fNavLine2.draw(b, COL_LABEL);
}

// Between two RGB565 colours: a = 0 gives bg, 255 gives fg
static uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
    auto mix = [a](int b, int f) { return (b * (255 - a) + f * a + 127) / 255; };
    int r = mix(bg >> 11, fg >> 11), g = mix((bg >> 5) & 0x3F, (fg >> 5) & 0x3F), b = mix(bg & 0x1F, fg & 0x1F);
    return (r << 11) | (g << 5) | b;
}

// The phone's 48 px turn icon at double size, with smoothed edges (the
// panel behind it is already cleared, so blank pixels are skipped)
static void drawPhoneIcon(const NavView& n, int cx, int cy, uint16_t color) {
    const int S = 2, SIZE = PHONE_ICON_SIZE * S;
    int x0 = cx - SIZE / 2, y0 = cy - SIZE / 2;
    lcd.startWrite();
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            uint8_t a = iconCoverage(n.icon, PHONE_ICON_SIZE, S, x, y);
            if (a) lcd.writePixel(x0 + x, y0 + y, blend565(COL_PANEL, color, a));
        }
    }
    lcd.endWrite();
}

static void drawCentrePanel(unsigned long nowMs) {
    int mode = phone.nav.active ? 1 : 0;
    if (mode != centreMode) {
        lcd.fillRect(CP_X + 6, GAUGE_Y + 10, CP_W - 12, GAUGE_SIZE - 20, COL_PANEL);
        lcd.drawFastHLine(CP_X + 20, 282, CP_W - 40, COL_PANEL_EDGE);
        for (Field* f : {&fNavDistance, &fNavStreet, &fNavLine1, &fNavLine2, &fEta, &fRemaining, &fHeading}) {
            f->shown[0] = '\0';
        }
        streetLines = 0;
        arrowShown = NO_ARROW;
    }

    if (phone.nav.active) {
        centreMode = 1;
        const NavView& n = phone.nav;
        // Our own arrow when the directions say what to do; the phone's
        // icon only for anything we don't recognise
        Maneuver mv = maneuverFor(n.street);
        uint32_t arrow = mv.kind != MANEUVER_UNKNOWN ? 0x80000000u | mv.kind << 16 | (mv.side + 1) << 8 | mv.exit
                       : n.hasIcon ? n.iconId : (uint32_t)(n.turn + 2);
        if (arrow != arrowShown) {
            arrowShown = arrow;
            lcd.fillRect(CP_X + 8, 84, CP_W - 16, 100, COL_PANEL);
            if (mv.kind != MANEUVER_UNKNOWN) drawManeuver(lcd, CP_MID, 134, mv, COL_ACCENT, COL_PANEL_EDGE, COL_PANEL);
            else if (n.hasIcon) drawPhoneIcon(n, CP_MID, 134, COL_ACCENT);
            else iconTurn(lcd, CP_MID, 134, n.turn, COL_ACCENT);
        }
        fNavDistance.draw(phone.nav.distance, COL_TEXT);
        drawStreet(phone.nav.street);
        char buf[32];
        if (phone.nav.eta[0]) snprintf(buf, sizeof(buf), "ETA %s", phone.nav.eta);
        else buf[0] = '\0';
        fEta.draw(buf, COL_TEXT);
        fRemaining.draw(routeLeft(phone.nav, buf, sizeof(buf)), COL_LABEL);
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
    if (phone.connected && phone.weather.valid) {
        const WeatherView& w = phone.weather;
        char line[40];
        snprintf(line, sizeof(line), "%d°  %s", w.temp, weatherName(w.icon));
        fEta.draw(line, COL_TEXT);
        snprintf(line, sizeof(line), "High %d   Low %d", w.high, w.low);
        fRemaining.draw(line, COL_LABEL);
    } else if (phone.connected) {
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
    if (phone.music.playing != playShown) {
        playShown = phone.music.playing;
        lcd.fillSmoothCircle(BTN_PLAY_X, BS_MID, 26, COL_ACCENT);
        if (phone.music.playing) iconPause(lcd, BTN_PLAY_X, BS_MID, COL_PANEL);
        else iconPlay(lcd, BTN_PLAY_X + 2, BS_MID, COL_PANEL);
    }
    fTitle.draw(phone.music.title[0] ? phone.music.title : "Nothing playing", phone.music.title[0] ? COL_TEXT : COL_LABEL);
    fArtist.draw(phone.music.artist, COL_LABEL);
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

// ---- The retro dash: everything it shows, gathered in one place ----

static void fillRetroView(RetroView& v, unsigned long nowMs) {
    bool d = inDemo;
    v.rpm = d ? demo.rpm : ktm.rpm;
    v.rpmLive = d || ktmRpmLive(ktm, nowMs);
    v.gear = d ? demo.gear : (ktmGearLive(ktm, nowMs) ? ktm.gear : -1);
    v.shift = shiftStage(v.rpm, SHIFT_SOON_RPM, SHIFT_NOW_RPM);
    v.flashOn = (nowMs / SHIFT_FLASH_MS) % 2 == 0;
    v.mph = d ? demo.mph : ktm.speedMph;
    v.speedLive = d || ktmSpeedLive(ktm, nowMs);
    v.fuelTenths = (int)(fuel.miles() * 10 + 1e-6);
    v.coolantLive = d || ktmCoolantLive(ktm, nowMs);
    v.coolantF = d ? demoCoolantF(nowMs) : ktm.coolantF;
    char clk[16];
    if (clockSet) formatClock(clk, sizeof(clk), time(nullptr));
    else snprintf(clk, sizeof(clk), d ? "12:25 PM" : "--:--");
    char* space = strchr(clk, ' ');  // seven segments: no AM/PM
    if (space) *space = '\0';
    snprintf(v.clock, sizeof(v.clock), "%s", clk);
    v.satellites = gpsView.satellites;
    v.gpsFix = gpsView.fix;
    v.phone = phone.connected;
    v.phoneBattery = phone.battery;

    const NavView& n = phone.nav;
    v.route = n.active;
    v.maneuver = maneuverFor(n.street);
    v.phoneIcon = n.hasIcon ? n.icon : nullptr;
    v.phoneIconId = n.iconId;
    snprintf(v.distance, sizeof(v.distance), "%s", n.distance);
    snprintf(v.street, sizeof(v.street), "%s", n.street);
    snprintf(v.eta, sizeof(v.eta), "%s", n.eta);
    char left[32];
    snprintf(v.left, sizeof(v.left), "%s", routeLeft(n, left, sizeof(left)));

    Heading h = currentHeading(nowMs);
    v.headingKnown = h.known;
    v.headingLive = h.live;
    v.headingDeg = h.deg;
    v.altitudeKnown = d || (gpsView.fix && gpsView.altitudeValid);
    v.elevationFt = d ? demoElevationFt(nowMs) : gpsView.elevationFt;

    v.musicBar = showMusicBar();
    v.playing = phone.music.playing;
    snprintf(v.title, sizeof(v.title), "%s", phone.music.title);
    snprintf(v.artist, sizeof(v.artist), "%s", phone.music.artist);
    const TripStats& t = d ? demoTrip : trip;
    v.tripMiles = t.miles();
    v.tripTopMph = t.maxSpeedMph();

    v.alert = banner.visible(nowMs);
    v.alertIsCall = banner.kind() == ALERT_CALL;
    snprintf(v.alertFrom, sizeof(v.alertFrom), "%s", banner.title());
    snprintf(v.alertText, sizeof(v.alertText), "%s", banner.text());
}

// ---- Calls and messages: a banner over the bottom bar ----

static Field fBannerTitle{92, BS_MID - 13, 540, 32, textdatum_t::middle_left, FONT_M, COL_PANEL, "", 0};
static Field fBannerText{92, BS_MID + 17, 540, 26, textdatum_t::middle_left, FONT_S, COL_PANEL, "", 0};

static void drawBarPanel(uint16_t edge) {
    lcd.fillSmoothRoundRect(16, BS_Y, 768, BS_H, 16, edge);
    lcd.fillSmoothRoundRect(19, BS_Y + 3, 762, BS_H - 6, 13, COL_PANEL);
}

// True while the banner is up (and the bar underneath waits)
static bool drawBanner(unsigned long nowMs) {
    int show = banner.visible(nowMs) ? banner.kind() + 1 : 0;
    bool call = banner.kind() == ALERT_CALL;
    if (show != bannerShown) {
        bool wasShown = bannerShown > 0;
        bannerShown = show;
        if (!show) {
            if (wasShown) {  // put the bar back as it was
                drawBarPanel(COL_PANEL_EDGE);
                barMode = -1;
            }
            return false;
        }
        drawBarPanel(call ? COL_GREEN : COL_ACCENT);
        if (call) iconPhone(lcd, 56, BS_MID, COL_GREEN, COL_PANEL);
        else iconMessage(lcd, 56, BS_MID, COL_ACCENT, COL_PANEL);
        useFont(lcd, FONT_S);
        lcd.setTextDatum(textdatum_t::middle_right);
        lcd.setTextColor(COL_LABEL, COL_PANEL);
        lcd.drawString("TAP TO CLOSE", 764, BS_MID);
        fBannerTitle.shown[0] = fBannerText.shown[0] = '\0';
    }
    if (!show) return false;
    if (call) {
        fBannerTitle.draw(banner.title()[0] ? banner.title() : "Unknown caller", COL_TEXT);
        fBannerText.draw("Incoming call", COL_GREEN);
    } else {
        char line[100];
        snprintf(line, sizeof(line), "%s   %s", banner.title(), banner.app());
        fBannerTitle.draw(line, COL_TEXT);
        fBannerText.draw(banner.text(), COL_LABEL);
    }
    return true;
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
static const Box BAR_TAP{16, BS_Y, 768, BS_H};  // the bottom bar
static const int BTN_ZONE = 80;                 // each music button's tap width
static const Box BTN_YES{204, 282, 188, 74};   // big and far apart for gloves
static const Box BTN_NO{408, 282, 188, 74};
static const unsigned long POPUP_TIMEOUT_MS = 10000;  // closes itself, changing nothing

enum PopupKind { POPUP_FUEL, POPUP_TRIP, POPUP_BRIGHTNESS };
static PopupKind popupKind = POPUP_FUEL;
static unsigned long popupMs = 0;

static void saveFuel() {
    flashWrite("gas count");
    prefs.putDouble("fuelMi", fuel.miles());
    fuel.markSaved();
}

static void button(const Box& b, const char* label, uint16_t color) {
    lcd.fillSmoothRoundRect(b.x, b.y, b.w, b.h, 14, color);
    lcd.setTextColor(COL_TEXT, color);
    lcd.drawString(label, b.x + b.w / 2, b.y + b.h / 2);
}

// "Do you want to reset ...?" with the count it would clear, and Yes / No
static void openResetPopup(PopupKind kind, const char* line1, const char* line2, double miles,
                           unsigned long nowMs) {
    popupOpen = true;
    popupKind = kind;
    popupMs = nowMs;
    lcd.fillSmoothRoundRect(POPUP.x - 2, POPUP.y - 2, POPUP.w + 4, POPUP.h + 4, 20, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(POPUP.x, POPUP.y, POPUP.w, POPUP.h, 18, COL_PANEL);
    if (kind == POPUP_FUEL) iconFuel(lcd, 400, POPUP.y + 30, COL_ACCENT, COL_PANEL);
    useFont(lcd, FONT_M);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(COL_TEXT, COL_PANEL);
    lcd.drawString(line1, 400, POPUP.y + 72);
    lcd.drawString(line2, 400, POPUP.y + 104);
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f mi", miles);
    useFont(lcd, FONT_L);
    lcd.setTextColor(COL_ACCENT, COL_PANEL);
    lcd.drawString(buf, 400, POPUP.y + 150);
    useFont(lcd, FONT_M);
    button(BTN_YES, "Yes, reset", COL_RED);
    button(BTN_NO, "No", COL_PANEL_EDGE);
}

static void openFuelPopup(unsigned long nowMs) {
    openResetPopup(POPUP_FUEL, "Do you want to reset", "miles since gas fill?", fuel.miles(), nowMs);
}

static int resettingTrip = TRIP_1;  // which one the trip pop-up is asking about

static void openTripResetPopup(int which, unsigned long nowMs) {
    resettingTrip = which;
    const TripStats& t = which == TRIP_1 ? trip1 : trip2;
    openResetPopup(POPUP_TRIP, "Do you want to reset", which == TRIP_1 ? "Trip 1?" : "Trip 2?", t.miles(), nowMs);
}

static void closePopup() {
    popupOpen = false;
    drawPage();
}

// ---- Brightness: the sun at the top right ----

static const Box BRIGHT_TAP{BRIGHT_ICON_X - 34, 0, 800 - (BRIGHT_ICON_X - 34), GAUGE_Y};
static const int LEVELS = sizeof(BRIGHTNESS_LEVELS) / sizeof(BRIGHTNESS_LEVELS[0]);
static const int LEVEL_W = 92, LEVEL_GAP = 12, LEVEL_H = 100;

static const int THEME_W = 196, THEME_H = 54;
static Box themeButton(int t) {  // THEME_MODERN or THEME_RETRO
    int x0 = POPUP.x + (POPUP.w - 2 * THEME_W - LEVEL_GAP) / 2;
    return Box{x0 + t * (THEME_W + LEVEL_GAP), POPUP.y + 204, THEME_W, THEME_H};
}

static Box levelButton(int i) {
    int x0 = POPUP.x + (POPUP.w - LEVELS * LEVEL_W - (LEVELS - 1) * LEVEL_GAP) / 2;
    return Box{x0 + i * (LEVEL_W + LEVEL_GAP), POPUP.y + 90, LEVEL_W, LEVEL_H};
}

static void openBrightnessPopup(unsigned long nowMs) {
    popupOpen = true;
    popupKind = POPUP_BRIGHTNESS;
    popupMs = nowMs;
    lcd.fillSmoothRoundRect(POPUP.x - 2, POPUP.y - 2, POPUP.w + 4, POPUP.h + 4, 20, COL_PANEL_EDGE);
    lcd.fillSmoothRoundRect(POPUP.x, POPUP.y, POPUP.w, POPUP.h, 18, COL_PANEL);
    iconBrightness(lcd, 400, POPUP.y + 30, COL_TEXT);
    useFont(lcd, FONT_M);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(COL_TEXT, COL_PANEL);
    lcd.drawString("Screen brightness", 400, POPUP.y + 66);
    for (int i = 0; i < LEVELS; i++) {
        char label[8];
        snprintf(label, sizeof(label), "%d%%", BRIGHTNESS_LEVELS[i]);
        button(levelButton(i), label, BRIGHTNESS_LEVELS[i] == brightness() ? COL_ACCENT : COL_PANEL_EDGE);
    }
    button(themeButton(THEME_MODERN), "Modern", theme() == THEME_MODERN ? COL_ACCENT : COL_PANEL_EDGE);
    button(themeButton(THEME_RETRO), "Retro", theme() == THEME_RETRO ? COL_ACCENT : COL_PANEL_EDGE);
    useFont(lcd, FONT_S);
    lcd.setTextColor(COL_LABEL, COL_PANEL);
    lcd.drawString("Tap outside to close", 400, POPUP.y + POPUP.h - 10);
}

// A look was picked: the whole screen redraws in it
static void pickTheme(Theme t) {
    setTheme(t);
    applyLook();
    flashWrite("theme");
    prefs.putUChar("theme", t);
    Serial.printf("# theme %s\n", retroShown() ? "retro" : "modern");
    closePopup();
    if (retroShown() && page == PAGE_DASH) retro.selfTest();
}

// A level was picked: the whole screen redraws at it
static void pickBrightness(int percent) {
    setBrightness(percent);
    flashWrite("brightness");
    prefs.putInt("bright", percent);
    Serial.printf("# brightness %d%%\n", percent);
    closePopup();
}

static void onTap(int x, int y, unsigned long nowMs) {
    if (popupOpen && popupKind == POPUP_BRIGHTNESS) {
        for (int i = 0; i < LEVELS; i++) {
            if (levelButton(i).contains(x, y)) {
                pickBrightness(BRIGHTNESS_LEVELS[i]);
                return;
            }
        }
        for (Theme t : {THEME_MODERN, THEME_RETRO}) {
            if (themeButton(t).contains(x, y)) {
                pickTheme(t);
                return;
            }
        }
        if (!POPUP.contains(x, y)) closePopup();
        return;
    }
    if (popupOpen) {
        if (BTN_YES.contains(x, y)) {
            if (popupKind == POPUP_FUEL) {
                fuel.reset();
                saveFuel();
            } else {
                (resettingTrip == TRIP_1 ? trip1 : trip2) = TripStats();
                saveTrips();
            }
            closePopup();
        } else if (BTN_NO.contains(x, y)) {
            closePopup();
        }
        return;
    }
    if (retroShown()) {
        RetroView v;
        fillRetroView(v, nowMs);
        RetroTap tap = retro.tapAt(x, y, v);
        MusicCommand cmd = MUSIC_CMD_TOGGLE;
        switch (tap) {
        case RETRO_TAP_BRIGHTNESS: openBrightnessPopup(nowMs); return;
        case RETRO_TAP_FUEL: openFuelPopup(nowMs); return;
        case RETRO_TAP_PREVIOUS: cmd = MUSIC_CMD_PREVIOUS; break;
        case RETRO_TAP_NEXT: cmd = MUSIC_CMD_NEXT; break;
        case RETRO_TAP_PLAY: break;
        case RETRO_TAP_BOTTOM:
            if (v.alert) banner.dismiss();
            else if (phone.connected) barChoice = showMusicBar() ? BAR_STATS : BAR_MUSIC;
            else if (inDemo) demoPhone = (demoPhone + 1) % DEMO_PHONE_STATES;
            return;
        default:
            if (inDemo) demoPhone = (demoPhone + 1) % DEMO_PHONE_STATES;
            return;
        }
        if (realPhone && !inDemo) phoneMusic(cmd);
        else if (cmd == MUSIC_CMD_TOGGLE) demoPlaying = !demoPlaying;
        return;
    }
    if (BRIGHT_TAP.contains(x, y)) {
        openBrightnessPopup(nowMs);
        return;
    }
    if (bannerShown > 0 && BAR_TAP.contains(x, y)) {
        banner.dismiss();
        return;
    }
    if (FUEL_TAP.contains(x, y)) {
        openFuelPopup(nowMs);
        return;
    }
    if (barMode == 1 && BAR_TAP.contains(x, y)) {  // music buttons, a wide zone each
        MusicCommand cmd;
        if (abs(x - BTN_PREV_X) < BTN_ZONE / 2) cmd = MUSIC_CMD_PREVIOUS;
        else if (abs(x - BTN_PLAY_X) < BTN_ZONE / 2) cmd = MUSIC_CMD_TOGGLE;
        else if (abs(x - BTN_NEXT_X) < BTN_ZONE / 2) cmd = MUSIC_CMD_NEXT;
        else {  // the song name: switch to ride stats
            barChoice = BAR_STATS;
            return;
        }
        if (realPhone && !inDemo) phoneMusic(cmd);
        else if (cmd == MUSIC_CMD_TOGGLE) demoPlaying = !demoPlaying;
        return;
    }
    if (barMode == 0 && phone.connected && BAR_TAP.contains(x, y)) {  // stats: back to music
        barChoice = BAR_MUSIC;
        return;
    }
    if (inDemo) demoPhone = (demoPhone + 1) % DEMO_PHONE_STATES;
}

// Taps on the pages other than the dash (single taps there wait a moment,
// in case it's a double tap back to the dash)
static void onPageTap(int x, int y, unsigned long nowMs) {
    if (page == PAGE_TRIPS) {
        int which = tripResetAt(x, y);
        if (which >= 0) openTripResetPopup(which, nowMs);
    }
}

static void pollTouch(unsigned long nowMs) {
    static GestureTracker gestures;
    static TapSequencer taps;
    int x = 0, y = 0;
    bool touching = gt911Read(x, y);
    Gesture g = gestures.update(touching, x, y, nowMs);
    if (popupOpen) {
        if (g.kind == GESTURE_TAP) onTap(g.x, g.y, nowMs);
        return;
    }
    switch (g.kind) {
    case GESTURE_SWIPE_LEFT:
        if (page + 1 < PAGE_COUNT) showPage(page + 1);
        return;
    case GESTURE_SWIPE_RIGHT:
        if (page > 0) showPage(page - 1);
        return;
    case GESTURE_HOLD:
        if (DEMO_BUILD && page == PAGE_DASH) {  // demo on or off, remembered
            demoEnabled = !demoEnabled;
            flashWrite("demo setting");
            prefs.putBool("demo", demoEnabled);
            Serial.printf("# demo %s\n", demoEnabled ? "on" : "off");
            drawScreen();
            if (demoEnabled && retroShown()) retro.selfTest();
        }
        return;
    case GESTURE_TAP:
        if (page == PAGE_DASH) onTap(g.x, g.y, nowMs);  // straight away: buttons feel instant
        else taps.tap(nowMs, g.x, g.y);
        break;
    default:
        break;
    }
    TapResult t = taps.poll(nowMs);
    if (t.kind == TAP_DOUBLE) showPage(PAGE_DASH);
    else if (t.kind == TAP_SINGLE) onPageTap(t.x, t.y, nowMs);
}

void loop() {
    esp_task_wdt_reset();
    unsigned long now = millis();
    static unsigned long passStartMs = 0;
    if (passStartMs && now - passStartMs > 40) {
        Serial.printf("# slow loop %lu ms at %lu ms (last: %s)\n", now - passStartMs, now, lastHeavy);
    }
    passStartMs = now;
    lastHeavy = "";
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
        sample.elevationValid = gpsView.fix && gpsView.altitudeValid;
        sample.elevationFt = gpsView.elevationFt;
        trip.update(dt, sample);
        if (sample.rpmLive || sample.speedLive) {  // trip meters count only with the bike on
            trip1.update(dt, sample);
            trip2.update(dt, sample);
        }
        static unsigned long lastTripSaveMs = 0, savedMovingMs = 0;
        if (now - lastTripSaveMs >= 60000) {  // at most once a minute, and only after riding
            lastTripSaveMs = now;
            if (trip1.movingMs() != savedMovingMs) {
                savedMovingMs = trip1.movingMs();
                saveTrips();
            }
        }
        if (inDemo) {  // the simulated ride
            TripSample d;
            d.speedLive = d.rpmLive = true;
            d.speedMph = demo.mph;
            d.rpm = demo.rpm;
            d.coolantLive = true;
            d.coolantF = demoCoolantF(now);
            d.elevationValid = true;
            d.elevationFt = demoElevationFt(now);
            demoTrip.update(dt, d);
        }
    }

    // Chart samples: the bike and GPS, with gaps where there's no data
    static unsigned long lastChartMs = 0;
    if (now - lastChartMs >= CHART_SAMPLE_MS) {
        lastChartMs = now;
        charts.speed.add(ktmSpeedLive(ktm, now) ? ktm.speedMph : CHART_NO_DATA);
        charts.rpm.add(ktmRpmLive(ktm, now) ? ktm.rpm : CHART_NO_DATA);
        charts.coolant.add(ktmCoolantLive(ktm, now) ? ktm.coolantF : CHART_NO_DATA);
        charts.elevation.add(gpsView.fix && gpsView.altitudeValid ? gpsView.elevationFt : CHART_NO_DATA);
        if (inDemo) {
            demoCharts.speed.add(demo.mph);
            demoCharts.rpm.add(demo.rpm);
            demoCharts.coolant.add(demoCoolantF(now));
            demoCharts.elevation.add(demoElevationFt(now));
        }
        if (page == PAGE_CHARTS && !popupOpen) drawChartsPage(false, inDemo ? demoCharts : charts);
    }

    static unsigned long lastTouchMs = 0;
    if (now - lastTouchMs >= TOUCH_POLL_MS) {
        lastTouchMs = now;
        pollTouch(now);
    }
    if (popupOpen) {  // the dash waits behind the pop-up
        if (now - popupMs >= POPUP_TIMEOUT_MS) closePopup();
        return;
    }

    // Live data when the bike is talking, the simulated ride otherwise
    bool demoMode = demoEnabled && !ktmRpmLive(ktm, now);
    inDemo = demoMode;
    if (demoMode) demo.update(now);

    static unsigned long lastPhoneMs = 0;
    if (now - lastPhoneMs >= 100) {
        lastPhoneMs = now;
        PhoneState live;
        phoneSnapshot(live);
        if (live.connected != realPhone) {
            Serial.printf("# phone %s\n", live.connected ? "connected" : "disconnected");
        }
        realPhone = live.connected;
        if (realPhone && !demoMode) {  // (the demo takes over the phone too, for showing off)
            // What the phone sends, as it changes: for checking the link
            if (strcmp(live.nav.distance, phone.nav.distance) || strcmp(live.nav.street, phone.nav.street) ||
                live.nav.active != phone.nav.active) {
                Serial.printf("# nav %s: [%s] [%s] eta [%s] left [%s] icon %d\n",
                              live.nav.active ? "on" : "off", live.nav.distance, live.nav.street,
                              live.nav.eta, live.nav.remaining, live.nav.hasIcon);
            }
            if (live.music.playing && strcmp(live.music.title, phone.music.title)) {
                barChoice = BAR_AUTO;  // a new track: back to the music
            }
            if (strcmp(live.music.title, phone.music.title) || live.music.playing != phone.music.playing) {
                Serial.printf("# music %s: [%s] [%s]\n", live.music.playing ? "playing" : "paused",
                              live.music.title, live.music.artist);
            }
            phone = live;
        }
        else if (demoMode) updateDemoPhone(now);
        else clearPhone();
        handlePhoneEvents(now);
        if (demoMode && phone.connected) demoAlerts(now);
    }

    if (page != PAGE_DASH) {
        static unsigned long lastPageMs = 0;
        if (page == PAGE_TRIPS && now - lastPageMs >= 1000) {
            lastPageMs = now;
            drawTripsPage(false, tripColumns());
        }
        return;
    }

    if (retroShown()) {
        static unsigned long lastRetroMs = 0;
        if (now - lastRetroMs >= GAUGE_FRAME_MS) {
            lastRetroMs = now;
            RetroView v;
            fillRetroView(v, now);
            retro.update(v, now);
        }
        return;
    }

    static unsigned long lastGaugeMs = 0;
    if (now - lastGaugeMs >= GAUGE_FRAME_MS) {
        lastGaugeMs = now;
        int rpm = demoMode ? demo.rpm : ktm.rpm;
        ShiftStage shift = shiftStage(rpm, SHIFT_SOON_RPM, SHIFT_NOW_RPM);
        bool flashOn = (now / SHIFT_FLASH_MS) % 2 == 0;
        int gear = demoMode ? demo.gear : (ktmGearLive(ktm, now) ? ktm.gear : -1);
        gauges.drawRpm(RpmView{rpm, gear, demoMode || ktmRpmLive(ktm, now), shift, flashOn});

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
        if (!drawBanner(now)) {
            if (showMusicBar()) drawMusicBar();
            else drawStatsBar();
        }
    }
}
