#pragma once

#include <stdio.h>
#include <string.h>
#include "dash_fonts.h"
#include "lgfx_w5.h"

extern LGFX_W5 lcd;  // the screen (main.cpp)

// Text that only repaints when it changes

struct Field {
    int x, y, w, h;           // box cleared before drawing; text sits in it
    textdatum_t datum;        // middle_left, middle_center or middle_right
    DashFont font;
    const uint16_t& bg;       // (a palette colour, which changes with brightness)
    char shown[64];
    uint16_t shownColor;

    void draw(const char* text, uint16_t color) {
        if (strcmp(text, shown) == 0 && color == shownColor) return;
        snprintf(shown, sizeof(shown), "%s", text);
        shownColor = color;
        lcd.fillRect(x, y - h / 2, w, h, bg);
        useFont(lcd, font);
        lcd.setTextDatum(datum);
        lcd.setTextColor(color, bg);
        // Shorten anything too long for its box
        char fit[64];
        snprintf(fit, sizeof(fit), "%s", text);
        int len = strlen(fit);
        while (len > 3 && lcd.textWidth(fit) > w) {
            fit[--len] = '\0';
            memcpy(fit + len - 3, "...", 3);
        }
        int tx = datum == textdatum_t::middle_left ? x
               : datum == textdatum_t::middle_right ? x + w : x + w / 2;
        // The font paints its background a full line high: keep it in the box
        lcd.setClipRect(x, y - h / 2, w, h);
        lcd.drawString(fit, tx, y);
        lcd.clearClipRect();
    }
};

