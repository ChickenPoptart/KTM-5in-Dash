#pragma once

#include <stdint.h>
#include "dash_logic.h"
#include "lgfx_w5.h"
#include "maneuver.h"

// The retro dash: an 80s digital dash in KTM orange, after the C4 Corvette
// and Astra GTE. Curved bar-graph speedo and tach, seven-segment digits
// over their unlit "8"s, fourteen-segment scrolling text, a dot-matrix turn
// arrow, and a power-on self-test that lights everything at once.

// Everything it shows, filled in by the main loop each frame
struct RetroView {
    int rpm = 0;
    bool rpmLive = false;
    int gear = -1;                // -1 none, 0 neutral
    ShiftStage shift = SHIFT_NONE;
    bool flashOn = false;
    int mph = 0;
    bool speedLive = false;
    int fuelTenths = 0;           // miles since gas fill, tenths
    int coolantF = 0;
    bool coolantLive = false;
    char clock[8] = "--:--";      // "12:25"
    int satellites = 0;
    bool gpsFix = false;
    bool phone = false;
    int phoneBattery = -1;

    bool route = false;           // turn-by-turn
    Maneuver maneuver = {MANEUVER_UNKNOWN, 0, 0};
    const uint8_t* phoneIcon = nullptr;  // 48 px, 1 bit, for unknown manoeuvres
    uint32_t phoneIconId = 0;
    char distance[16] = "";       // "0.3 mi"
    char street[64] = "";
    char eta[24] = "";            // "12:52 PM"
    char left[32] = "";           // "12 min left"

    bool headingKnown = false, headingLive = false;
    int headingDeg = 0;
    bool altitudeKnown = false;
    int elevationFt = 0;

    bool musicBar = false;        // the bottom shows music (else ride stats)
    bool playing = false;
    char title[64] = "";
    char artist[64] = "";
    double tripMiles = 0;
    int tripTopMph = 0;

    bool alert = false;           // a call or message, over the bottom
    bool alertIsCall = false;
    char alertFrom[64] = "";
    char alertText[128] = "";
};

// What a tap on the retro dash asks for
enum RetroTap {
    RETRO_TAP_NONE,
    RETRO_TAP_FUEL,        // the gas count: reset pop-up
    RETRO_TAP_BRIGHTNESS,  // the sun: brightness and look
    RETRO_TAP_PREVIOUS,
    RETRO_TAP_PLAY,
    RETRO_TAP_NEXT,
    RETRO_TAP_BOTTOM,      // the bottom window: close an alert, or music <-> stats
};

class RetroDash {
public:
    explicit RetroDash(LGFX_W5& lcd);
    bool begin();         // digit buffers in internal RAM; false if out of RAM
    void end();
    bool active() const { return ready; }

    void drawStatic();    // the windows and scales; then update draws the rest
    void selfTest();      // everything lit, then the bars sweep down (blocks ~1.5 s)
    void update(const RetroView& v, unsigned long nowMs);
    RetroTap tapAt(int x, int y, const RetroView& v) const;

private:
    LGFX_W5& lcd;
    bool ready = false;
};
