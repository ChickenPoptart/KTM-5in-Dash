// The phone link through the Chronos app (fbiego/chronos-esp32): the dash
// shows up to the app as a watch.
//
// Chronos parses what arrives on the Bluetooth task and calls back from
// there, so everything it hands over is copied into this file's own state
// under a lock, and the main loop takes copies of that.

#include "phone.h"

#include <ChronosESP32.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static ChronosESP32* watch = nullptr;
static SemaphoreHandle_t lock = nullptr;
static PhoneState state;
// The app's temperature setting, from its user settings (CF_USER). Until
// it says otherwise, weather is taken to be in Celsius, the app's default.
static bool appSendsFahrenheit = false;

static int toFahrenheit(int t) {
    if (appSendsFahrenheit) return t;
    return (t * 9 + (t >= 0 ? 2 : -2)) / 5 + 32;  // rounded
}

static const int EVENT_QUEUE = 6;
static PhoneEvent events[EVENT_QUEUE];
static int eventHead = 0, eventCount = 0;  // oldest, and how many waiting

static void copy(char* out, size_t cap, const String& s) {
    snprintf(out, cap, "%s", s.c_str());
}

// Only called with the lock held
static void queue(PhoneEvent::Type type, const String& app, const String& title, const String& text) {
    if (eventCount == EVENT_QUEUE) {  // full: drop the oldest
        eventHead = (eventHead + 1) % EVENT_QUEUE;
        eventCount--;
    }
    PhoneEvent& e = events[(eventHead + eventCount) % EVENT_QUEUE];
    e.type = type;
    copy(e.app, sizeof(e.app), app);
    copy(e.title, sizeof(e.title), title);
    copy(e.text, sizeof(e.text), text);
    eventCount++;
}

// A fresh start whenever the link comes up or drops. Checked from the
// link's own state each loop: Chronos's connect callback doesn't always
// arrive (seen after the dash restarts and the phone reconnects at once).
static void setConnected(bool connected) {
    xSemaphoreTake(lock, portMAX_DELAY);
    if (state.connected != connected) {
        state = PhoneState();
        state.connected = connected;
    }
    xSemaphoreGive(lock);
}

static void onNotification(Notification n) {
    xSemaphoreTake(lock, portMAX_DELAY);
    queue(PhoneEvent::MESSAGE, n.app, n.title, n.message);
    xSemaphoreGive(lock);
}

static void onRinger(String caller, bool ringing) {
    xSemaphoreTake(lock, portMAX_DELAY);
    queue(ringing ? PhoneEvent::CALL_RINGING : PhoneEvent::CALL_ENDED, "", caller, "");
    xSemaphoreGive(lock);
}

static void copyNavigation() {
    Navigation& n = watch->getNavigation();
    NavView& v = state.nav;
    v.active = n.active && n.isNavigation;
    copy(v.distance, sizeof(v.distance), n.title);
    copy(v.street, sizeof(v.street), n.directions);
    // Google Maps sends "Arrive 9:27 AM"; the dash labels it ETA itself
    String eta = n.eta;
    if (eta.startsWith("Arrive ")) eta = eta.substring(7);
    copy(v.eta, sizeof(v.eta), eta);
    // Distance and time left, either of which may be blank
    const char* gap = n.distance.length() && n.duration.length() ? "   " : "";
    snprintf(v.remaining, sizeof(v.remaining), "%s%s%s", n.distance.c_str(), gap, n.duration.c_str());
}

static void onConfiguration(Config config, uint32_t a, uint32_t b) {
    xSemaphoreTake(lock, portMAX_DELAY);
    switch (config) {
    case CF_NAV_DATA:
        copyNavigation();
        break;
    case CF_NAV_ICON:
        if (a == 2) {  // the whole icon has arrived
            Navigation& n = watch->getNavigation();
            memcpy(state.nav.icon, n.icon, sizeof(state.nav.icon));
            state.nav.hasIcon = n.hasIcon;
            state.nav.iconId = n.iconCRC;
        }
        break;
    case CF_MUSIC: {
        MusicInfo& m = watch->getMusicInfo();
        state.music.playing = m.state == 1;
        // With nothing playing, Chronos sends "No active media" by "Chronos"
        bool nothing = m.title == "No active media";
        copy(state.music.title, sizeof(state.music.title), nothing ? String() : m.title);
        copy(state.music.artist, sizeof(state.music.artist), nothing ? String() : m.artist);
        break;
    }
    case CF_PBAT:
        state.charging = a != 0;
        state.battery = b;
        break;
    case CF_WEATHER:
        if (watch->getWeatherCount() > 0) {
            Weather& w = watch->getWeatherAt(0);  // today
            state.weather.valid = true;
            state.weather.temp = toFahrenheit(w.temp);
            state.weather.high = toFahrenheit(w.high);
            state.weather.low = toFahrenheit(w.low);
            state.weather.icon = w.icon;
        }
        break;
    case CF_USER:
        appSendsFahrenheit = ((b >> 8) & 0xFF) != 0;  // temperature unit byte
        Serial.printf("# phone settings: temperature unit %u\n", (unsigned)((b >> 8) & 0xFF));
        break;
    case CF_TIME:
        if (a == 1) queue(PhoneEvent::CLOCK_SET, "", "", "");  // set, as local time
        break;
    default:
        break;
    }
    xSemaphoreGive(lock);
}

void phoneBegin(const char* name) {
    lock = xSemaphoreCreateMutex();
    watch = new ChronosESP32(name);
    watch->setNotificationCallback(onNotification);
    watch->setRingerCallback(onRinger);
    watch->setConfigurationCallback(onConfiguration);
    watch->begin();
    watch->setBattery(100);         // the dash runs off the bike
    watch->setNotifyBattery(true);  // ask the phone to send its battery level
}

void phonePoll() {
    if (!watch) return;
    watch->loop();
    setConnected(watch->isConnected());
    static bool subscribed = false;
    if (watch->isSubscribed() != subscribed) {
        subscribed = !subscribed;
        Serial.printf("# phone %s the dash's replies\n", subscribed ? "subscribed to" : "unsubscribed from");
    }
}

void phoneSnapshot(PhoneState& out) {
    xSemaphoreTake(lock, portMAX_DELAY);
    out = state;
    xSemaphoreGive(lock);
}

bool phoneNextEvent(PhoneEvent& out) {
    xSemaphoreTake(lock, portMAX_DELAY);
    bool any = eventCount > 0;
    if (any) {
        out = events[eventHead];
        eventHead = (eventHead + 1) % EVENT_QUEUE;
        eventCount--;
    }
    xSemaphoreGive(lock);
    return any;
}

void phoneMusic(MusicCommand command) {
    if (!watch || !watch->isConnected()) return;
    static const Control CONTROLS[] = {MUSIC_PREVIOUS, MUSIC_TOGGLE, MUSIC_NEXT};
    watch->musicControl(CONTROLS[command]);
}

const char* weatherName(int icon) {
    static const char* const NAMES[] = {"Pt. cloudy", "Sunny", "Snow", "Rain",
                                        "Cloudy", "Storm", "Windy", "Fog"};
    return icon >= 0 && icon < 8 ? NAMES[icon] : "";
}
