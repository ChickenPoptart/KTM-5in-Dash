#include <unity.h>
#include "fuel_trip.h"

void setUp() {}
void tearDown() {}

void test_riding_adds_miles_from_wheel_speed() {
    FuelTrip trip;
    trip.ride(3600000, true, 30);  // an hour at 30 mph
    TEST_ASSERT_FLOAT_WITHIN(0.001, 30.0, trip.miles());
}

void test_rides_add_up_between_fills() {
    FuelTrip trip;
    trip.ride(1800000, true, 40);  // half an hour at 40: 20 mi
    trip.ride(60000, true, 60);    // a minute at 60: 1 mi
    TEST_ASSERT_FLOAT_WITHIN(0.001, 21.0, trip.miles());
}

void test_no_miles_while_wheel_speed_is_missing() {
    FuelTrip trip;
    trip.ride(3600000, false, 30);  // stale reading from before the dropout
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, trip.miles());
}

void test_filling_up_starts_again_from_zero() {
    FuelTrip trip;
    trip.ride(3600000, true, 30);
    trip.reset();
    trip.ride(60000, true, 60);  // 1 mi since the fill
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.0, trip.miles());
}

void test_count_carries_on_from_the_saved_value_after_key_off() {
    FuelTrip trip;
    trip.restore(87.4);
    trip.ride(60000, true, 60);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 88.4, trip.miles());
}

void test_asks_to_be_saved_every_tenth_of_a_mile() {
    FuelTrip trip;
    trip.restore(87.4);
    trip.ride(5000, true, 36);  // 0.05 mi: not worth a flash write yet
    TEST_ASSERT_FALSE(trip.needsSave());
    trip.ride(5000, true, 36);  // 0.1 mi since the last save
    TEST_ASSERT_TRUE(trip.needsSave());
}

void test_stops_asking_once_saved() {
    FuelTrip trip;
    trip.ride(10000, true, 36);  // 0.1 mi
    trip.markSaved();
    TEST_ASSERT_FALSE(trip.needsSave());
}

void test_a_reset_is_saved_straight_away() {
    FuelTrip trip;
    trip.restore(87.4);
    trip.reset();  // key off at the pump must not bring 87.4 back
    TEST_ASSERT_TRUE(trip.needsSave());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_riding_adds_miles_from_wheel_speed);
    RUN_TEST(test_rides_add_up_between_fills);
    RUN_TEST(test_no_miles_while_wheel_speed_is_missing);
    RUN_TEST(test_filling_up_starts_again_from_zero);
    RUN_TEST(test_count_carries_on_from_the_saved_value_after_key_off);
    RUN_TEST(test_asks_to_be_saved_every_tenth_of_a_mile);
    RUN_TEST(test_stops_asking_once_saved);
    RUN_TEST(test_a_reset_is_saved_straight_away);
    return UNITY_END();
}
