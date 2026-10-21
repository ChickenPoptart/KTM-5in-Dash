#include <string.h>
#include <unity.h>
#include "text_wrap.h"

void setUp() {}
void tearDown() {}

// Every character 1 unit wide, so widths are just lengths
static int charCount(const char* s) { return strlen(s); }

void test_text_that_fits_stays_on_one_line() {
    char a[32], b[32];
    wrapTwoLines("Main St", 10, charCount, a, b, sizeof(a));
    TEST_ASSERT_EQUAL_STRING("Main St", a);
    TEST_ASSERT_EQUAL_STRING("", b);
}

void test_long_text_breaks_at_the_last_space_that_fits() {
    char a[32], b[32];
    wrapTwoLines("Turn right onto Main St", 16, charCount, a, b, sizeof(a));
    TEST_ASSERT_EQUAL_STRING("Turn right onto", a);
    TEST_ASSERT_EQUAL_STRING("Main St", b);
}

void test_a_first_word_too_wide_still_breaks_at_the_first_space() {
    char a[32], b[32];
    wrapTwoLines("Northbound Rd", 5, charCount, a, b, sizeof(a));
    TEST_ASSERT_EQUAL_STRING("Northbound", a);
    TEST_ASSERT_EQUAL_STRING("Rd", b);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_text_that_fits_stays_on_one_line);
    RUN_TEST(test_long_text_breaks_at_the_last_space_that_fits);
    RUN_TEST(test_a_first_word_too_wide_still_breaks_at_the_first_space);
    return UNITY_END();
}
