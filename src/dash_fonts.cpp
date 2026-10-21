#include "dash_fonts.h"

#include "fonts/font_huge.h"
#include "fonts/font_large.h"
#include "fonts/font_medium.h"
#include "fonts/font_seg14.h"
#include "fonts/font_seg14_small.h"
#include "fonts/font_seg_big.h"
#include "fonts/font_seg_gear.h"
#include "fonts/font_seg_small.h"
#include "fonts/font_seg_speed.h"
#include "fonts/font_seg_tiny.h"
#include "fonts/font_small.h"
#include "fonts/font_xl.h"

static const uint8_t* const FONTS[] = {VLW_SMALL, VLW_MEDIUM, VLW_LARGE, VLW_XL, VLW_HUGE,
                                       VLW_SEG_GEAR, VLW_SEG_SPEED, VLW_SEG_SMALL, VLW_SEG_BIG,
                                       VLW_SEG_TINY, VLW_SEG14, VLW_SEG14_SMALL};

// Last font loaded on each drawing target (screen, sprites)
struct Loaded { lgfx::LGFXBase* target; DashFont font; };
static Loaded loaded[4] = {};

void useFont(lgfx::LGFXBase& target, DashFont font) {
    for (Loaded& l : loaded) {
        if (l.target == &target) {
            if (l.font == font) return;
            target.loadFont(FONTS[font]);
            l.font = font;
            return;
        }
    }
    for (Loaded& l : loaded) {
        if (!l.target) {
            l = {&target, font};
            break;
        }
    }
    target.loadFont(FONTS[font]);
}
