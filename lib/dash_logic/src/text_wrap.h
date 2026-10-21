#pragma once

#include <stdio.h>
#include <string.h>

// Splits text into two lines at a space so the first fits maxWidth, as
// measured by width(). Whatever doesn't fit goes on the second line.
inline void wrapTwoLines(const char* text, int maxWidth, int (*width)(const char*),
                         char* line1, char* line2, int cap) {
    snprintf(line1, cap, "%s", text);
    line2[0] = '\0';
    if (width(line1) <= maxWidth) return;
    // Try each space from the end until the part before it fits
    for (int i = strlen(line1) - 1; i > 0; i--) {
        if (line1[i] != ' ') continue;
        line1[i] = '\0';
        if (width(line1) <= maxWidth) {
            snprintf(line2, cap, "%s", text + i + 1);
            return;
        }
    }
    // Even the first word is too wide: break after it anyway (each line
    // gets shortened to fit when it's drawn)
    const char* space = strchr(text, ' ');
    snprintf(line1, cap, "%.*s", space ? (int)(space - text) : cap, text);
    if (space) snprintf(line2, cap, "%s", space + 1);
}
