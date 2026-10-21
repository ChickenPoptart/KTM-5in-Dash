#pragma once

#include "lgfx_w5.h"
#include "maneuver.h"

// The dash's own turn-by-turn arrows, about 96 px square, centred on
// (cx, cy). Drawn with anti-aliased lines over a panel already cleared to
// bg. `dim` is for the roads not taken (at a fork, a ramp, a merge).
void drawManeuver(lgfx::LGFXBase& g, int cx, int cy, const Maneuver& m, uint16_t color, uint16_t dim,
                  uint16_t bg);
