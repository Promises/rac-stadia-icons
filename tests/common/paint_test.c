/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * Painting (paint.h) on fonts made here (test_font.h). After it: the texels outside the ten cells
 * and the palette entries they use are as they were; every texel of every cell is the Stadia
 * picture's, in its own grey; no symbol is left. With few free palette entries the commonest greys
 * still get theirs and the rest share (nothing outside the cells changing); with too few nothing
 * changes at all. Two fonts (Going Commando's title) paint two; none or four are refused.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paint.h"
#include "stadia_art.h"
#include "test_font.h"

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static uint8_t s_before[3][TEST_FONT];
static uint8_t s_palette_before[1024];

static int in_cells(unsigned u, unsigned v) {
    if (v < 96 || v >= 112) {
        return 0;
    }
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        if (u >= stadia_icons_cell_u(c) && u < stadia_icons_cell_u(c) + 24u) {
            return 1;
        }
    }
    return 0;
}

/* The art's colour of value k (k_art_cells) as the palette holds it. */
static void art_colour(int k, uint8_t out[4]) {
    if (k == 0) {
        out[0] = out[1] = out[2] = STADIA_ART_CLEAR_GREY;
        out[3] = 0;
        return;
    }
    out[0] = out[1] = out[2] = k_art_colours[k - 1][0];
    out[3] = (uint8_t)((k_art_colours[k - 1][1] * 0x80u + 127u) / 255u);
}

static void snapshot(void) {
    memcpy(s_before, s_fonts, sizeof s_before);
    memcpy(s_palette_before, s_palette, sizeof s_palette_before);
}

/* Outside the cells, every texel of the `count` fonts as it was, and each entry those use. */
static void check_outside(int count) {
    for (int f = 0; f < count; ++f) {
        for (unsigned v = 0; v < 128; ++v) {
            for (unsigned u = 0; u < 256; ++u) {
                if (in_cells(u, v)) {
                    continue;
                }
                const uint8_t i = *test_at(f, u, v);
                CHECK(i == s_before[f][v * 256 + u]);
                CHECK(memcmp(test_colour(s_palette, i), test_colour(s_palette_before, i), 4) == 0);
            }
        }
    }
}

static int distance(const uint8_t* a, const uint8_t* b) {
    const int dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2], da = 2 * (a[3] - b[3]);
    return dr * dr + dg * dg + db * db + da * da;
}

/* Painting with only the entries of `spare` free (the others used outside the cells, on row 0 of
 * the first font); with `opaque` no entry is clear. */
static int paint_with_free(const uint8_t* spare, int n, int opaque, stadia_icons_plan* plan) {
    test_lay();
    if (opaque) {
        test_set(0, 128, 128, 128, 0x80);
    }
    uint8_t is_spare[256] = {0};
    for (int i = 0; i < n; ++i) {
        is_spare[spare[i]] = 1;
    }
    unsigned u = 0;
    for (unsigned i = 0; i < 256; ++i) {
        if (!is_spare[i]) {
            *test_at(0, u % 256u, u / 256u) = (uint8_t)i;
            ++u;
        }
    }
    snapshot();
    uint8_t* fonts[3] = {s_fonts[0], s_fonts[1], s_fonts[2]};
    const int r = stadia_icons_plan_paint(fonts, 3, s_palette, plan);
    if (r == 0) {
        stadia_icons_apply(plan, fonts, 3, s_palette);
    }
    return r;
}

int main(void) {
    static stadia_icons_plan plan;
    uint8_t* fonts[3] = {s_fonts[0], s_fonts[1], s_fonts[2]};

    /* The cells are where the glyph tables put them, in the order of the glyph bytes. */
    static const unsigned u_of[STADIA_ICONS_CELLS] = {0, 72, 48, 24, 96, 144, 120, 168, 208, 232};
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        CHECK(stadia_icons_cell_u(c) == u_of[c]);
    }

    test_lay();
    snapshot();
    const uint32_t game = stadia_icons_hash(fonts, 3, s_palette);
    CHECK(stadia_icons_plan_paint(fonts, 3, s_palette, &plan) == 0);
    /* Planning writes nothing. */
    CHECK(memcmp(s_before, s_fonts, sizeof s_before) == 0);
    CHECK(memcmp(s_palette_before, s_palette, sizeof s_palette) == 0);
    stadia_icons_apply(&plan, fonts, 3, s_palette);
    CHECK(stadia_icons_hash(fonts, 3, s_palette) != game);
    CHECK(plan.own == plan.colours && plan.colours == 1 + STADIA_ART_COLOURS);
    CHECK(plan.written == plan.colours); /* clear too: there were entries enough */
    check_outside(3);
    for (int f = 0; f < 3; ++f) {
        for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
            int clear = 0, opaque = 0;
            for (unsigned y = 0; y < 16; ++y) {
                for (unsigned x = 0; x < 24; ++x) {
                    const uint8_t i = *test_at(f, stadia_icons_cell_u(c) + x, 96 + y);
                    const int k = k_art_cells[c][y * 24 + x];
                    uint8_t want[4];
                    art_colour(k, want);
                    /* The picture's own grey, in an entry of its own. */
                    CHECK(memcmp(test_colour(s_palette, i), want, 4) == 0);
                    CHECK(i == plan.cells[c][y][x]);
                    clear += k == 0;
                    opaque += k != 0 && want[3] == 0x80;
                }
            }
            /* Every cell painted: a picture in it, clear round it. */
            CHECK(opaque > 60 && clear > 60);
        }
    }

    /* The title's two fonts (Going Commando's menus load no third): those two painted, the third
     * left as it was, and the pair's hash is not the three's. */
    test_lay();
    snapshot();
    CHECK(stadia_icons_hash(fonts, 2, s_palette) != stadia_icons_hash(fonts, 3, s_palette));
    CHECK(stadia_icons_paint(fonts, 2, s_palette) == 0);
    CHECK(memcmp(s_before[0], s_fonts[0], TEST_FONT) != 0 && memcmp(s_before[1], s_fonts[1], TEST_FONT) != 0);
    CHECK(memcmp(s_before[2], s_fonts[2], TEST_FONT) == 0);
    check_outside(2);
    /* No font, or more than a set has: refused, nothing changed. */
    CHECK(stadia_icons_paint(fonts, 0, s_palette) == 1 && stadia_icons_paint(fonts, 4, s_palette) == 1);

    /* Few free entries (six: some of the symbols'): the six commonest greys get them, the clear
     * texels the fonts' own clear entry, the other greys the nearest colour there is; outside the
     * cells nothing changes, and only the six entries are written. */
    static const uint8_t six[] = {41, 42, 50, 51, 52, 53};
    CHECK(paint_with_free(six, 6, 0, &plan) == 0);
    CHECK(plan.written == 6 && plan.own == 7 && plan.clear == 0);
    check_outside(3);
    for (unsigned i = 0; i < 256; ++i) {
        const int spare = memchr(six, (int)i, sizeof six) != NULL;
        CHECK(spare || memcmp(test_colour(s_palette, (uint8_t)i), test_colour(s_palette_before, (uint8_t)i), 4) == 0);
    }
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        for (unsigned y = 0; y < 16; ++y) {
            for (unsigned x = 0; x < 24; ++x) {
                const int k = k_art_cells[c][y * 24 + x];
                const uint8_t i = *test_at(1, stadia_icons_cell_u(c) + x, 96 + y);
                uint8_t want[4];
                art_colour(k, want);
                if (k == 0) {
                    CHECK(i == 0); /* the fonts' clear */
                } else if (k <= 6) {
                    CHECK(memcmp(test_colour(s_palette, i), want, 4) == 0); /* its own */
                } else {
                    /* Shared: the nearest colour there is, the six greys written among them. */
                    const int d = distance(test_colour(s_palette, i), want);
                    for (int o = 1; o <= 6; ++o) {
                        uint8_t other[4];
                        art_colour(o, other);
                        CHECK(d <= distance(other, want));
                    }
                }
            }
        }
    }

    /* The least that paints: clear (the fonts' own) and three greys. Two free entries: refused,
     * nothing changed. */
    static const uint8_t four[] = {50, 51, 52, 53};
    CHECK(paint_with_free(four, 3, 0, &plan) == 0 && plan.own == STADIA_ICONS_MIN_COLOURS);
    CHECK(paint_with_free(four, 2, 0, &plan) == 1);
    CHECK(memcmp(s_before, s_fonts, sizeof s_before) == 0);
    CHECK(memcmp(s_palette_before, s_palette, sizeof s_palette) == 0);

    /* No clear entry the fonts keep: clear takes a free entry, so it takes four. */
    CHECK(paint_with_free(four, 4, 1, &plan) == 0 && plan.written == 4 && plan.clear == 50);
    CHECK(test_colour(s_palette, 50)[3] == 0);
    check_outside(3);
    CHECK(paint_with_free(four, 3, 1, &plan) == 1);
    CHECK(memcmp(s_before, s_fonts, sizeof s_before) == 0);
    CHECK(memcmp(s_palette_before, s_palette, sizeof s_palette) == 0);

    /* A grey an entry outside the cells holds already: used as it is, no entry written for it. */
    test_lay();
    {
        uint8_t first[4];
        art_colour(1, first);
        test_set(7, first[0], first[1], first[2], first[3]);
    }
    snapshot();
    CHECK(stadia_icons_plan_paint(fonts, 3, s_palette, &plan) == 0);
    CHECK(plan.own == plan.colours && plan.written == plan.colours - 1);
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        for (unsigned t = 0; t < 16 * 24; ++t) {
            CHECK((k_art_cells[c][t] == 1) == (plan.cells[c][t / 24][t % 24] == 7));
        }
    }

    printf("stadia_icons_paint_test: ok\n");
    return 0;
}
