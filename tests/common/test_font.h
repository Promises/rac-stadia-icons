/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * Fonts made in the test as the game's are laid out (xbox-icons' test fonts, with the sticks):
 * letters in grey outside the cells, four face buttons of one dark sphere with a coloured symbol
 * each, four tabs, two stick balls, and letters of their own between the tabs and the sticks
 * (u 192..207 of row 96, which the mod keeps).
 *
 * Entries: 0 clear (kept: the letters' background uses it), 1..40 the letters' greys (a shade
 * blue, so that none is one of the Stadia pictures' greys), 41..48 the
 * sphere's greys, 50..53 the symbols' colours, 54 the tabs' text white, 55..58 the sticks' greys,
 * 60..63 the letters at u 192..207, the rest unused (a colour no texel has).
 */
#ifndef STADIA_ICONS_TEST_FONT_H
#define STADIA_ICONS_TEST_FONT_H

#include <stdint.h>
#include <string.h>

#define TEST_FONT (256 * 128)

static uint8_t s_fonts[3][TEST_FONT];
static uint8_t s_palette[1024];

static inline unsigned test_csm1(unsigned i) {
    return (i & ~0x18u) | ((i & 8u) << 1) | ((i & 0x10u) >> 1);
}
static inline void test_set(unsigned i, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    uint8_t* e = s_palette + test_csm1(i) * 4u;
    e[0] = r, e[1] = g, e[2] = b, e[3] = a;
}
static inline const uint8_t* test_colour(const uint8_t* palette, uint8_t i) {
    return palette + test_csm1(i) * 4u;
}
static inline uint8_t* test_at(int f, unsigned u, unsigned v) {
    return &s_fonts[f][v * 256u + u];
}

static inline void test_lay(void) {
    for (unsigned i = 0; i < 256; ++i) {
        test_set(i, 200, 0, 200, 0x80);
    }
    test_set(0, 128, 128, 128, 0);
    for (unsigned i = 1; i <= 40; ++i) {
        test_set(i, (uint8_t)(i * 6), (uint8_t)(i * 6), (uint8_t)(i * 6 + 3), 0x80);
    }
    for (unsigned i = 41; i <= 48; ++i) {
        const uint8_t g = (uint8_t)(30 + (i - 41) * 8);
        test_set(i, g, g, g, 0x80);
    }
    test_set(50, 100, 104, 230, 0x80); /* Cross: colours none of the letters has */
    test_set(51, 232, 135, 135, 0x80); /* Square */
    test_set(52, 12, 150, 60, 0x80);   /* Triangle */
    test_set(53, 174, 45, 45, 0x80);   /* Circle */
    test_set(54, 255, 255, 255, 0x80);
    for (unsigned i = 55; i <= 58; ++i) {
        const uint8_t g = (uint8_t)(10 + (i - 55) * 12);
        test_set(i, g, g, g, 0x80);
    }
    for (unsigned i = 60; i <= 63; ++i) {
        test_set(i, (uint8_t)(150 + i), (uint8_t)(150 + i), 90, 0x80);
    }
    for (int f = 0; f < 3; ++f) {
        memset(s_fonts[f], 0, TEST_FONT);
        for (unsigned v = 0; v < 96; ++v) {
            for (unsigned u = 0; u < 256; ++u) {
                *test_at(f, u, v) = (uint8_t)(1 + (u * 7 + v * 3 + (unsigned)f) % 40);
            }
        }
        for (unsigned c = 0; c < 4; ++c) {
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 24; ++x) {
                    const int dx = x - 10, dy = y - 8, d2 = dx * dx + dy * dy;
                    uint8_t i = d2 <= 64 ? (uint8_t)(41 + (dy + 8) / 2) : 0;
                    /* Each symbol covers its own texels: a cross, a square, a triangle, a ring. */
                    const int on = d2 <= 30 && ((c == 0 && (dx == dy || dx == -dy)) ||
                                                (c == 1 && (dx == 3 || dy == 3)) ||
                                                (c == 2 && dy == 4) || (c == 3 && d2 >= 20));
                    if (on) {
                        i = (uint8_t)(50 + c);
                    }
                    *test_at(f, c * 24u + (unsigned)x, 96u + (unsigned)y) = i;
                    *test_at(f, 96u + c * 24u + (unsigned)x, 96u + (unsigned)y) =
                        (x >= 3 && y >= 2 && y < 15) ? (x == 6 + (int)c ? 54 : 44) : 0;
                }
            }
        }
        /* The letters between the tabs and the sticks (kept), and the two sticks' balls. */
        for (unsigned v = 96; v < 112; ++v) {
            for (unsigned u = 192; u < 208; ++u) {
                *test_at(f, u, v) = (uint8_t)(60 + (u + v) % 4);
            }
            for (unsigned s = 0; s < 2; ++s) {
                for (unsigned x = 0; x < 24; ++x) {
                    const int dx = (int)x - 7, dy = (int)v - 96 - 8;
                    *test_at(f, 208 + s * 24 + x, v) =
                        dx * dx + dy * dy <= 42 ? (uint8_t)(55 + (unsigned)(dy + 8) / 4) : 0;
                }
            }
        }
    }
}

#endif /* STADIA_ICONS_TEST_FONT_H */
