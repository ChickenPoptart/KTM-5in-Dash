#include <string.h>
#include <unity.h>
#include "icon_scale.h"

void setUp() {}
void tearDown() {}

static const int N = 48;
static uint8_t bits[N * N / 8];

void test_inside_a_solid_shape_is_fully_covered() {
    memset(bits, 0xFF, sizeof(bits));
    TEST_ASSERT_EQUAL(255, iconCoverage(bits, N, 2, 48, 48));
}

void test_blank_icon_covers_nothing() {
    memset(bits, 0, sizeof(bits));
    TEST_ASSERT_EQUAL(0, iconCoverage(bits, N, 2, 48, 48));
}

// The lower-left half set: a stair-stepped diagonal edge
static void diagonal() {
    memset(bits, 0, sizeof(bits));
    for (int y = 0; y < N; y++)
        for (int x = 0; x <= y; x++) bits[(y * N + x) / 8] |= 0x80 >> ((y * N + x) % 8);
}

void test_a_diagonal_edge_is_smoothed_not_blocky() {
    diagonal();
    int between = 0;
    for (int ox = 30; ox < 52; ox++) {  // across the edge, on one row
        uint8_t c = iconCoverage(bits, N, 2, ox, 41);
        if (c > 0 && c < 255) between++;
    }
    TEST_ASSERT_GREATER_THAN(0, between);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_inside_a_solid_shape_is_fully_covered);
    RUN_TEST(test_blank_icon_covers_nothing);
    RUN_TEST(test_a_diagonal_edge_is_smoothed_not_blocky);
    return UNITY_END();
}
