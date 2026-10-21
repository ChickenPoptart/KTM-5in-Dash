#pragma once

#include "lgfx_w5.h"

// Small icons drawn from shapes, each centred on (x, y) and about 32 px
// across. bg is the colour behind the icon, for the parts cut out of it.

void iconCoolant(lgfx::LGFXBase& g, int x, int y, uint16_t color);
void iconMountain(lgfx::LGFXBase& g, int x, int y, uint16_t color);
void iconPhone(lgfx::LGFXBase& g, int x, int y, uint16_t color, uint16_t bg);
void iconFuel(lgfx::LGFXBase& g, int x, int y, uint16_t color, uint16_t bg);
void iconSatellite(lgfx::LGFXBase& g, int x, int y, uint16_t color);

void iconPrevious(lgfx::LGFXBase& g, int x, int y, uint16_t color);
void iconNext(lgfx::LGFXBase& g, int x, int y, uint16_t color);
void iconPlay(lgfx::LGFXBase& g, int x, int y, uint16_t color);
void iconPause(lgfx::LGFXBase& g, int x, int y, uint16_t color);

// A big turn arrow, about 84 px across: -1 left, 0 straight on, 1 right
void iconTurn(lgfx::LGFXBase& g, int x, int y, int direction, uint16_t color);
