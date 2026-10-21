#include <unity.h>
#include "brightness.h"

void setUp() {}
void tearDown() {}

void test_full_brightness_changes_no_colour() {
    for (uint32_t c = 0; c <= 0xFFFF; c += 7) {
        TEST_ASSERT_EQUAL_HEX16(c, dimmed565(c, 100));
    }
}

void test_half_brightness_halves_each_channel() {
    TEST_ASSERT_EQUAL_HEX16((15 << 11) | (31 << 5) | 15, dimmed565(0xFFFF, 50));  // white -> grey
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_full_brightness_changes_no_colour);
    RUN_TEST(test_half_brightness_halves_each_channel);
    return UNITY_END();
}
