#pragma once

#include <ctype.h>
#include <string.h>

// Blanks between the end of scrolling text and its start coming round again
const int MARQUEE_GAP = 3;

// A `width`-character window onto text for a segment display: upper case,
// with "!" (a blank as wide as a character in the DSEG fonts) for spaces and
// padding. Text too long to fit scrolls round by `offset` characters.
inline void marqueeWindow(const char* text, int width, int offset, char* out, int size) {
    int len = strlen(text);
    int loop = len + MARQUEE_GAP;
    int n = 0;
    for (; n < width && n < size - 1; n++) {
        int k = len <= width ? n : (offset + n) % loop;  // fits: no scrolling
        char c = k < len ? text[k] : ' ';
        out[n] = c == ' ' ? '!' : (char)toupper((unsigned char)c);
    }
    out[n] = '\0';
}
