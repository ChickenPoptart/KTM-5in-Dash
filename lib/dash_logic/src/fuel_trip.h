#pragma once

#include <stdint.h>

// Saving to flash this often loses at most this much at key-off.
const double FUEL_SAVE_EVERY_MI = 0.1;

// Miles ridden since the tank was last filled.
class FuelTrip {
public:
    void ride(uint32_t dtMs, bool speedLive, int mph) {
        if (speedLive) total += (double)mph * dtMs / 3600000.0;
    }

    // Power-up: carry on from the count saved before the key went off
    void restore(double savedMiles) { total = saved = savedMiles; }

    // Filled up: start counting again
    void reset() {
        total = 0;
        resetUnsaved = true;
    }

    double miles() const { return total; }

    // True once the count has moved far enough to be worth saving to flash
    // (less a hair, so float rounding on exactly 0.1 mi still counts)
    bool needsSave() const { return resetUnsaved || total - saved >= FUEL_SAVE_EVERY_MI - 1e-9; }
    void markSaved() {
        saved = total;
        resetUnsaved = false;
    }

private:
    double total = 0;
    double saved = 0;
    bool resetUnsaved = false;
};
