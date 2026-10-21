#pragma once

#include "dash_logic.h"
#include "lgfx_w5.h"

// The two round gauges. The RGB panel streams its frame buffer from PSRAM
// continuously, and heavy drawing into PSRAM starves that stream (the
// picture jumps sideways). So the dials are drawn once, the coloured arcs
// are updated only by the slice that changed, the centre numbers are built
// in a small buffer in internal RAM, and the ring is redrawn only when the
// shift light changes.

struct RpmView {
    int rpm;
    int gear;          // -1 = no data, 0 = neutral
    bool live;         // false: show "--" and an empty arc
    ShiftStage shift;  // SOON: ring steady green; NOW: ring flashes red
    bool flashOn;      // which half of the red flash this frame is in
};

struct SpeedView {
    int mph;
    bool live;
    int fuelTenths;       // miles since gas fill, in tenths
};

class Gauges {
public:
    explicit Gauges(LGFX_W5& lcd);
    bool begin();         // allocates the centre buffer; false if out of RAM
    void drawDials();     // the parts that never change; call once per page

    void drawRpm(const RpmView& v);
    void drawSpeed(const SpeedView& v);

private:
    struct ArcState {
        int ox, oy;       // top-left of the gauge's square on screen
        int max;
        bool zoned;
        int shown = 0;    // value the arc currently shows
    };
    void moveArc(ArcState& a, int value);
    void drawRing(int ox, int oy, uint16_t color);
    void clearCentre();
    void pushCentre(const ArcState& a);

    LGFX_W5& lcd;
    LGFX_Sprite centre;   // internal RAM, reused for both gauges
    LGFX_Sprite rpmText;  // just the RPM number, for when only that changes
    ArcState rpmArc, speedArc;
    RpmView lastRpm{-1, -2, false, SHIFT_NONE, false};
    SpeedView lastSpeed{-1, false, -1};
    uint16_t ringColor = 0;
};
