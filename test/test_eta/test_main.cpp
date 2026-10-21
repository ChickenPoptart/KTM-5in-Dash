#include <stdio.h>
#include <unity.h>
#include "eta.h"

void setUp() {}
void tearDown() {}

static int at(int hour, int minute) { return hour * 60 + minute; }

void test_minutes_left_until_a_morning_eta() {
    TEST_ASSERT_EQUAL(4, minutesUntil("9:24 AM", at(9, 20)));
}

// A clock time written the way Google Maps writes it: "9:24 AM", "12:05 PM"
static void mapsClock(char* out, int size, int minuteOfDay) {
    int h = minuteOfDay / 60, m = minuteOfDay % 60;
    int h12 = h % 12 == 0 ? 12 : h % 12;
    snprintf(out, size, "%d:%02d %s", h12, m, h < 12 ? "AM" : "PM");
}

void test_minutes_left_matches_the_gap_for_any_time_and_eta() {
    char eta[16];
    for (int now = 0; now < 24 * 60; now += 7) {         // every 7 min of the day
        for (int gap = 0; gap < 24 * 60; gap += 11) {    // ETAs up to a day ahead
            mapsClock(eta, sizeof(eta), (now + gap) % (24 * 60));
            TEST_ASSERT_EQUAL_MESSAGE(gap, minutesUntil(eta, now), eta);
        }
    }
}

void test_no_minutes_for_a_blank_eta_while_rerouting() {
    TEST_ASSERT_EQUAL(-1, minutesUntil("", at(9, 20)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_minutes_left_until_a_morning_eta);
    RUN_TEST(test_minutes_left_matches_the_gap_for_any_time_and_eta);
    RUN_TEST(test_no_minutes_for_a_blank_eta_while_rerouting);
    return UNITY_END();
}
