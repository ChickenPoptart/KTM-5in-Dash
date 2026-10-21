#pragma once

#include <math.h>
#include "style.h"

// Roughly this bike's gearing (engine rpm per mph, from ride logs), and how
// fast the simulated rider pulls in each gear (rpm per second)
static const int DEMO_RPM_PER_MPH[7] = {1, 207, 145, 111, 92, 80, 72};
static const int DEMO_PULL_RATE[7] = {0, 5500, 3800, 2800, 2100, 1500, 1200};
static const int DEMO_SHIFT_AT = SHIFT_NOW_RPM + 300;  // a little past the red light

// A simulated ride for showing off the gauges with no bike connected:
// idle, pull through the gears past the shift light, cruise, slow down
// through the gears, idle again. Repeats.
class DemoRide {
public:
    int rpm = 1700;
    int gear = 0;
    int mph = 0;

    void update(unsigned long nowMs) {
        unsigned long dt = lastMs ? nowMs - lastMs : 0;
        lastMs = nowMs;
        float s = dt / 1000.0f;

        switch (phase) {
        case IDLE:
            gear = 0;
            mph = 0;
            rpm = 1700 + (int)(30 * sinf(nowMs / 90.0f));
            if (nowMs - phaseMs > 3000) start(PULL, nowMs, 1);
            break;
        case PULL:
            rpm += (int)(DEMO_PULL_RATE[gear] * s);
            mph = rpm / DEMO_RPM_PER_MPH[gear];
            if (gear < 5 && rpm >= DEMO_SHIFT_AT) {
                gear++;
                rpm = mph * DEMO_RPM_PER_MPH[gear];
            } else if (gear == 5 && rpm >= 6800) {
                start(CRUISE, nowMs, 5);
            }
            break;
        case CRUISE:
            rpm = 6800 + (int)(60 * sinf(nowMs / 400.0f));
            mph = rpm / DEMO_RPM_PER_MPH[gear];
            if (nowMs - phaseMs > 3000) start(SLOW, nowMs, 5);
            break;
        case SLOW:
            rpm -= (int)(1500 * s);
            mph = rpm / DEMO_RPM_PER_MPH[gear];
            if (rpm <= 3200) {
                if (gear > 1) {
                    gear--;
                    rpm = mph * DEMO_RPM_PER_MPH[gear];
                } else {
                    start(IDLE, nowMs, 0);
                }
            }
            break;
        }
    }

private:
    enum Phase { IDLE, PULL, CRUISE, SLOW };

    void start(Phase p, unsigned long nowMs, int g) {
        phase = p;
        phaseMs = nowMs;
        gear = g;
        if (p == PULL) rpm = 2500;
    }

    Phase phase = IDLE;
    unsigned long phaseMs = 0, lastMs = 0;
};
