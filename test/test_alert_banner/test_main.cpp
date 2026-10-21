#include <unity.h>
#include "alert_banner.h"

void setUp() {}
void tearDown() {}

void test_a_text_shows_then_goes_away_on_its_own() {
    AlertBanner b;
    b.message("Messages", "Sam", "On my way", 1000);
    TEST_ASSERT_TRUE(b.visible(1000 + ALERT_MESSAGE_MS - 1));
    TEST_ASSERT_FALSE(b.visible(1000 + ALERT_MESSAGE_MS));
}

void test_a_call_stays_up_while_the_phone_rings() {
    AlertBanner b;
    b.callRinging("Mom", 1000);
    TEST_ASSERT_TRUE(b.visible(1000 + 60000));
}

void test_the_call_banner_goes_when_the_ringing_stops() {
    AlertBanner b;
    b.callRinging("Mom", 1000);
    b.callEnded();  // answered, declined or missed
    TEST_ASSERT_FALSE(b.visible(1001));
}

void test_a_text_during_a_call_does_not_cover_it() {
    AlertBanner b;
    b.callRinging("Mom", 1000);
    b.message("Messages", "Sam", "On my way", 2000);
    TEST_ASSERT_EQUAL(ALERT_CALL, b.kind());
    TEST_ASSERT_EQUAL_STRING("Mom", b.title());
}

void test_a_text_shows_the_app_and_the_message() {
    AlertBanner b;
    b.message("WhatsApp", "Sam", "On my way", 1000);
    TEST_ASSERT_EQUAL(ALERT_MESSAGE, b.kind());
    TEST_ASSERT_EQUAL_STRING("WhatsApp", b.app());
    TEST_ASSERT_EQUAL_STRING("On my way", b.text());
}

void test_a_tap_clears_the_banner_early() {
    AlertBanner b;
    b.callRinging("Mom", 1000);
    b.dismiss();  // the phone keeps ringing; the dash gets out of the way
    TEST_ASSERT_FALSE(b.visible(1001));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_text_shows_then_goes_away_on_its_own);
    RUN_TEST(test_a_call_stays_up_while_the_phone_rings);
    RUN_TEST(test_the_call_banner_goes_when_the_ringing_stops);
    RUN_TEST(test_a_text_during_a_call_does_not_cover_it);
    RUN_TEST(test_a_text_shows_the_app_and_the_message);
    RUN_TEST(test_a_tap_clears_the_banner_early);
    return UNITY_END();
}
