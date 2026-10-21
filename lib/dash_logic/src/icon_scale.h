#pragma once

#include <stdint.h>

// One pixel of a 1-bit icon: n x n, rows top to bottom, high bit leftmost
inline bool iconBit(const uint8_t* bits, int n, int x, int y) {
    if (x < 0 || y < 0 || x >= n || y >= n) return false;
    int i = y * n + x;
    return (bits[i / 8] >> (7 - i % 8)) & 1;
}

// How much of screen pixel (ox, oy) a 1-bit icon covers (0-255) when it's
// drawn `scale` times bigger. Blends the four icon pixels around it, then
// sharpens the blend again so edges come out smooth but not blurry: the
// stair steps of a diagonal become an anti-aliased line.
inline uint8_t iconCoverage(const uint8_t* bits, int n, int scale, int ox, int oy) {
    // Where this screen pixel's centre falls in the icon
    float sx = (ox + 0.5f) / scale - 0.5f, sy = (oy + 0.5f) / scale - 0.5f;
    int x0 = sx < 0 ? -1 : (int)sx, y0 = sy < 0 ? -1 : (int)sy;
    float fx = sx - x0, fy = sy - y0;
    float top = iconBit(bits, n, x0, y0) * (1 - fx) + iconBit(bits, n, x0 + 1, y0) * fx;
    float bottom = iconBit(bits, n, x0, y0 + 1) * (1 - fx) + iconBit(bits, n, x0 + 1, y0 + 1) * fx;
    float v = top * (1 - fy) + bottom * fy;
    // Sharpen: smoothstep over the middle of the blend
    const float lo = 0.3f, hi = 0.7f;
    if (v <= lo) return 0;
    if (v >= hi) return 255;
    float t = (v - lo) / (hi - lo);
    return (uint8_t)(255 * t * t * (3 - 2 * t) + 0.5f);
}
