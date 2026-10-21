#include <unity.h>
#include "marquee.h"

void setUp() {}
void tearDown() {}

void test_short_text_fits_padded_with_blank_digits() {
    char out[16];
    marqueeWindow("Main St", 10, 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("MAIN!ST!!!", out);  // segment font: upper case, "!" blanks
}

void test_long_text_scrolls_round_with_a_gap() {
    char out[16];
    marqueeWindow("Highway Tune", 8, 0, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("HIGHWAY!", out);
    marqueeWindow("Highway Tune", 8, 5, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("AY!TUNE!", out);
    marqueeWindow("Highway Tune", 8, 13, out, sizeof(out));  // past the end: a gap, then round again
    TEST_ASSERT_EQUAL_STRING("!!HIGHWA", out);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_short_text_fits_padded_with_blank_digits);
    RUN_TEST(test_long_text_scrolls_round_with_a_gap);
    return UNITY_END();
}
