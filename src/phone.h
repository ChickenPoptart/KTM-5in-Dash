#pragma once

#include <stdint.h>

// What the phone sends over Bluetooth, copied out of the link so the
// screen code never touches it directly. The link itself is the Chronos
// app (phone_chronos.cpp); another phone app would only replace that file.

static const int PHONE_ICON_SIZE = 48;  // the phone's turn arrow, square

struct NavView {
    bool active = false;
    int turn = 0;              // demo arrow when there's no icon: -1 left, 0 on, 1 right
    bool hasIcon = false;
    uint32_t iconId = 0;       // changes whenever the icon does
    // 1 bit per pixel, rows top to bottom, high bit leftmost
    uint8_t icon[PHONE_ICON_SIZE * PHONE_ICON_SIZE / 8] = {};
    char distance[16] = "";    // to the next turn: "0.3 mi"
    char street[64] = "";      // the street, or what to do there
    char eta[24] = "";         // "12:52 PM"
    char remaining[40] = "";   // "14.2 mi   23 min"
};

struct MusicView {
    bool playing = false;
    char title[64] = "";
    char artist[64] = "";
};

struct WeatherView {
    bool valid = false;
    int temp = 0, high = 0, low = 0;  // °F
    int icon = 0;                     // Chronos weather code (see weatherName)
};

struct PhoneState {
    bool connected = false;
    int battery = -1;          // percent; -1 = not known
    bool charging = false;
    NavView nav;
    MusicView music;
    WeatherView weather;
};

// Things that happen once: calls, messages, the clock being set
struct PhoneEvent {
    enum Type { CALL_RINGING, CALL_ENDED, MESSAGE, CLOCK_SET } type;
    char app[32];
    char title[64];   // caller, or who a message is from
    char text[128];
};

enum MusicCommand { MUSIC_CMD_PREVIOUS, MUSIC_CMD_TOGGLE, MUSIC_CMD_NEXT };

// Starts advertising under this Bluetooth name. The clock it sets is local
// time, so set TZ first.
void phoneBegin(const char* name);
void phonePoll();                        // call every loop
void phoneSnapshot(PhoneState& out);     // the latest of everything
bool phoneNextEvent(PhoneEvent& out);    // oldest first; false when none
void phoneMusic(MusicCommand command);

// "Sunny", "Rain"… for a Chronos weather code
const char* weatherName(int icon);
