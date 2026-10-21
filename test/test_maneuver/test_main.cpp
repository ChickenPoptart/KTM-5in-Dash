#include <unity.h>
#include "maneuver.h"

void setUp() {}
void tearDown() {}

void test_a_right_turn() {
    Maneuver m = maneuverFor("Turn right onto Main St");
    TEST_ASSERT_EQUAL(MANEUVER_TURN, m.kind);
    TEST_ASSERT_EQUAL(1, m.side);
}

void test_a_left_turn() {
    Maneuver m = maneuverFor("Turn left onto Canyon Rd");
    TEST_ASSERT_EQUAL(MANEUVER_TURN, m.kind);
    TEST_ASSERT_EQUAL(-1, m.side);
}

void test_text_that_is_not_a_maneuver_is_unknown() {
    TEST_ASSERT_EQUAL(MANEUVER_UNKNOWN, maneuverFor("Rerouting...").kind);
}

void test_a_slight_right() {
    Maneuver m = maneuverFor("Slight right onto Main St");
    TEST_ASSERT_EQUAL(MANEUVER_SLIGHT, m.kind);
    TEST_ASSERT_EQUAL(1, m.side);
}

void test_a_sharp_left() {
    Maneuver m = maneuverFor("Sharp left onto Ridge Loop");
    TEST_ASSERT_EQUAL(MANEUVER_SHARP, m.kind);
    TEST_ASSERT_EQUAL(-1, m.side);
}

void test_a_u_turn_swings_left() {
    Maneuver m = maneuverFor("Make a U-turn");
    TEST_ASSERT_EQUAL(MANEUVER_UTURN, m.kind);
    TEST_ASSERT_EQUAL(-1, m.side);  // riding on the right, it swings left
}

void test_heading_off_or_continuing_is_straight_on() {
    Maneuver m = maneuverFor("Head northeast on Canyon Rd");
    TEST_ASSERT_EQUAL(MANEUVER_STRAIGHT, m.kind);
    TEST_ASSERT_EQUAL(0, m.side);
    TEST_ASSERT_EQUAL(MANEUVER_STRAIGHT, maneuverFor("Continue onto Hwy 6").kind);
}

void test_keeping_right_at_a_fork() {
    Maneuver m = maneuverFor("Keep right at the fork");
    TEST_ASSERT_EQUAL(MANEUVER_KEEP, m.kind);
    TEST_ASSERT_EQUAL(1, m.side);
}

void test_ramps_and_exits() {
    Maneuver left = maneuverFor("Take the ramp on the left to I-15 N");
    TEST_ASSERT_EQUAL(MANEUVER_RAMP, left.kind);
    TEST_ASSERT_EQUAL(-1, left.side);
    Maneuver exit = maneuverFor("Take exit 271 toward Main St");  // US exits: right
    TEST_ASSERT_EQUAL(MANEUVER_RAMP, exit.kind);
    TEST_ASSERT_EQUAL(1, exit.side);
}

void test_a_roundabout_and_which_exit() {
    Maneuver m = maneuverFor("At the roundabout, take the 2nd exit onto Main St");
    TEST_ASSERT_EQUAL(MANEUVER_ROUNDABOUT, m.kind);
    TEST_ASSERT_EQUAL(2, m.exit);
}

void test_merging_onto_a_highway() {
    TEST_ASSERT_EQUAL(MANEUVER_MERGE, maneuverFor("Merge onto I-15 N").kind);
}

void test_arriving_at_the_destination() {
    Maneuver m = maneuverFor("Your destination will be on the right");
    TEST_ASSERT_EQUAL(MANEUVER_ARRIVE, m.kind);
    TEST_ASSERT_EQUAL(MANEUVER_ARRIVE, maneuverFor("Arrive at Main St").kind);
}

void test_the_side_comes_from_the_instruction_not_the_street_name() {
    TEST_ASSERT_EQUAL(1, maneuverFor("Turn right onto Cleft Ridge Rd").side);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_right_turn);
    RUN_TEST(test_a_left_turn);
    RUN_TEST(test_text_that_is_not_a_maneuver_is_unknown);
    RUN_TEST(test_a_slight_right);
    RUN_TEST(test_a_sharp_left);
    RUN_TEST(test_a_u_turn_swings_left);
    RUN_TEST(test_heading_off_or_continuing_is_straight_on);
    RUN_TEST(test_keeping_right_at_a_fork);
    RUN_TEST(test_ramps_and_exits);
    RUN_TEST(test_a_roundabout_and_which_exit);
    RUN_TEST(test_merging_onto_a_highway);
    RUN_TEST(test_arriving_at_the_destination);
    RUN_TEST(test_the_side_comes_from_the_instruction_not_the_street_name);
    return UNITY_END();
}
