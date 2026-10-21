#pragma once

enum GestureKind { GESTURE_NONE, GESTURE_TAP, GESTURE_SWIPE_LEFT, GESTURE_SWIPE_RIGHT, GESTURE_HOLD };

struct Gesture {
    GestureKind kind = GESTURE_NONE;
    int x = 0, y = 0;  // where the finger first came down
};

// Turns touch readings into taps, holds and sideways swipes.
class GestureTracker {
public:
    // How long the screen must read untouched before the finger counts as
    // lifted (the touch chip skips reports under a steady finger)
    static const unsigned long RELEASE_MS = 60;
    // A drag this far sideways (and mostly sideways) is a swipe
    static const int SWIPE_PX = 100;
    // A finger that slides further than this isn't a tap
    static const int TAP_SLOP_PX = 30;
    // Held still this long: a hold, reported while the finger is still down
    static const unsigned long HOLD_MS = 1000;

    // x, y only matter while touching
    Gesture update(bool touching, int x, int y, unsigned long nowMs) {
        Gesture g;
        if (touching) {
            if (!down) {
                startX = x;
                startY = y;
                downMs = nowMs;
                held = false;
            }
            down = true;
            lastTouchMs = nowMs;
            lastX = x;
            lastY = y;
            if (!held && nowMs - downMs >= HOLD_MS && still()) {
                held = true;
                g.kind = GESTURE_HOLD;
                g.x = startX;
                g.y = startY;
            }
            return g;
        }
        if (down && nowMs - lastTouchMs >= RELEASE_MS) {
            down = false;
            if (held) return g;  // already reported
            int dx = lastX - startX, dy = lastY - startY;
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            if (adx >= SWIPE_PX && adx > 2 * ady) g.kind = dx < 0 ? GESTURE_SWIPE_LEFT : GESTURE_SWIPE_RIGHT;
            else if (still()) g.kind = GESTURE_TAP;
            g.x = startX;
            g.y = startY;
        }
        return g;
    }

private:
    bool still() const {
        int dx = lastX - startX, dy = lastY - startY;
        return dx >= -TAP_SLOP_PX && dx <= TAP_SLOP_PX && dy >= -TAP_SLOP_PX && dy <= TAP_SLOP_PX;
    }

    bool down = false, held = false;
    unsigned long downMs = 0;
    int startX = 0, startY = 0, lastX = 0, lastY = 0;
    unsigned long lastTouchMs = 0;
};
