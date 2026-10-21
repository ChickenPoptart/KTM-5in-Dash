#pragma once

#include <stdio.h>

// Minutes from nowMinute (minutes since local midnight) until a clock time
// like "9:24 AM", as Google Maps gives the ETA. An ETA earlier in the day
// than now is tomorrow's. -1 if it isn't a time (blank while rerouting).
inline int minutesUntil(const char* clock, int nowMinute) {
    int h, m;
    char half[3];
    if (sscanf(clock, "%d:%d %2s", &h, &m, half) != 3) return -1;
    int etaMinute = (h % 12 + (half[0] == 'P' ? 12 : 0)) * 60 + m;
    return ((etaMinute - nowMinute) % (24 * 60) + 24 * 60) % (24 * 60);
}
