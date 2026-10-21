#include <unity.h>
#include "gesture.h"

void setUp() {}
void tearDown() {}

// The finger held at (x, y) from t0 to t1, reported every 10 ms
static Gesture hold(GestureTracker& g, int x, int y, unsigned long t0, unsigned long t1) {
    Gesture last;
    for (unsigned long t = t0; t <= t1; t += 10) {
        Gesture e = g.update(true, x, y, t);
        if (e.kind != GESTURE_NONE) last = e;
    }
    return last;
}

// The finger dragged in a straight line from (x0, y0) to (x1, y1)
static void drag(GestureTracker& g, int x0, int y0, int x1, int y1, unsigned long t0, unsigned long t1) {
    for (unsigned long t = t0; t <= t1; t += 10) {
        float f = (float)(t - t0) / (t1 - t0);
        g.update(true, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, t);
    }
}

// The finger lifted at t: returns what that release made
static Gesture lift(GestureTracker& g, unsigned long t) {
    Gesture last;
    for (unsigned long u = t; u <= t + 200; u += 10) {
        Gesture e = g.update(false, 0, 0, u);
        if (e.kind != GESTURE_NONE) last = e;
    }
    return last;
}

void test_a_short_touch_is_a_tap_where_it_landed() {
    GestureTracker g;
    hold(g, 300, 200, 0, 120);
    Gesture e = lift(g, 130);
    TEST_ASSERT_EQUAL(GESTURE_TAP, e.kind);
    TEST_ASSERT_EQUAL(300, e.x);
    TEST_ASSERT_EQUAL(200, e.y);
}

void test_dragging_left_is_a_swipe_left() {
    GestureTracker g;
    drag(g, 600, 240, 300, 250, 0, 250);
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_LEFT, lift(g, 260).kind);
}

void test_dragging_right_is_a_swipe_right() {
    GestureTracker g;
    drag(g, 200, 240, 500, 230, 0, 250);
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_RIGHT, lift(g, 260).kind);
}

void test_a_mostly_vertical_drag_is_nothing() {
    GestureTracker g;
    drag(g, 300, 100, 420, 320, 0, 250);  // 120 across but 220 down
    TEST_ASSERT_EQUAL(GESTURE_NONE, lift(g, 260).kind);
}

void test_holding_still_for_a_second_is_a_hold() {
    GestureTracker g;
    Gesture held = hold(g, 400, 240, 0, 1100);  // reported while still down
    TEST_ASSERT_EQUAL(GESTURE_HOLD, held.kind);
    TEST_ASSERT_EQUAL(GESTURE_NONE, lift(g, 1110).kind);  // and no tap after it
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_short_touch_is_a_tap_where_it_landed);
    RUN_TEST(test_dragging_left_is_a_swipe_left);
    RUN_TEST(test_dragging_right_is_a_swipe_right);
    RUN_TEST(test_a_mostly_vertical_drag_is_nothing);
    RUN_TEST(test_holding_still_for_a_second_is_a_hold);
    return UNITY_END();
}
