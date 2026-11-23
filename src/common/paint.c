/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * Stadia buttons painted over the game's PlayStation 2 ones (paint.h).
 */
#include "paint.h"

#include <string.h>

#include "stadia_art.h"

#define W STADIA_ICONS_WIDTH
#define ROW STADIA_ICONS_ROW
#define CELL_W STADIA_ICONS_CELL_W
#define CELL_H STADIA_ICONS_CELL_H
#define SCALE STADIA_ICONS_SHARP_SCALE
#define COLOURS (1 + STADIA_ART_COLOURS) /* clear, then the art's greys */

#if STADIA_ART_CELLS != STADIA_ICONS_CELLS || STADIA_ART_SCALE != STADIA_ICONS_SHARP_SCALE
#error "stadia_art.h does not match paint.h: run tools/stadia_art.py again"
#endif

unsigned stadia_icons_cell_u(int cell) {
    return k_art_u[cell];
}

/* A palette index's place in CSM1 order. */
static unsigned csm1(unsigned i) {
    return (i & ~0x18u) | ((i & 8u) << 1) | ((i & 0x10u) >> 1);
}
static const uint8_t* colour(const uint8_t* palette, unsigned i) {
    return palette + csm1(i) * 4u;
}
static uint8_t* texel(uint8_t* font, unsigned u, unsigned y) {
    return font + (ROW + y) * W + u;
}
/* The ten cells: row 96's u 0..191 and 208..255 (192..207 holds letters, kept). */
static int in_cells(unsigned u, unsigned v) {
    if (v < ROW || v >= ROW + CELL_H) {
        return 0;
    }
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        if (u >= k_art_u[c] && u < k_art_u[c] + CELL_W) {
            return 1;
        }
    }
    return 0;
}
static int distance(const uint8_t* a, const uint8_t* b) {
    const int dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2], da = 2 * (a[3] - b[3]);
    return dr * dr + dg * dg + db * db + da * da;
}

uint32_t stadia_icons_hash(uint8_t* const fonts[], int count, const uint8_t* palette) {
    uint32_t h = 2166136261u;
    for (int f = 0; f < count; ++f) {
        for (unsigned y = 0; y < CELL_H; ++y) {
            for (unsigned u = 0; u < STADIA_ICONS_HASH_WIDTH; ++u) {
                h = (h ^ *texel(fonts[f], u, y)) * 16777619u;
            }
        }
    }
    for (unsigned i = 0; i < 1024; ++i) {
        h = (h ^ palette[i]) * 16777619u;
    }
    return h;
}

/* The art's colour c as a palette entry: 0 clear (the edges' grey, no coverage), else a grey and
 * its coverage in the GS's alpha (0x80 opaque). */
static void wanted(int c, uint8_t out[4]) {
    if (c == 0) {
        out[0] = out[1] = out[2] = STADIA_ART_CLEAR_GREY;
        out[3] = 0;
        return;
    }
    const uint8_t* g = k_art_colours[c - 1];
    out[0] = out[1] = out[2] = g[0];
    out[3] = (uint8_t)((g[1] * 0x80u + 127u) / 255u);
}

int stadia_icons_plan_paint(uint8_t* const fonts[], int count, const uint8_t* palette,
                            stadia_icons_plan* plan) {
    if (count < 1 || count > STADIA_ICONS_FONTS) {
        return 1;
    }
    /* The palette entries some texel outside the cells uses: they stay as they are. */
    uint8_t kept[256] = {0};
    for (int f = 0; f < count; ++f) {
        for (unsigned v = 0; v < STADIA_ICONS_HEIGHT; ++v) {
            for (unsigned u = 0; u < W; ++u) {
                if (!in_cells(u, v)) {
                    kept[fonts[f][v * W + u]] = 1;
                }
            }
        }
    }
    uint8_t free_entries[256];
    int free_count = 0;
    for (unsigned i = 0; i < 256; ++i) {
        if (!kept[i]) {
            free_entries[free_count++] = (uint8_t)i;
        }
    }

    memcpy(plan->palette, palette, sizeof plan->palette);
    uint8_t want[COLOURS][4];
    int map[COLOURS];
    uint8_t written[256] = {0};
    int next = 0, own = 0;
    for (int c = 0; c < COLOURS; ++c) {
        wanted(c, want[c]);
        map[c] = -1;
        /* A colour a kept entry holds already is used as it is. */
        for (unsigned e = 0; e < 256 && map[c] < 0; ++e) {
            if (kept[e] && memcmp(colour(palette, e), want[c], 4) == 0) {
                map[c] = (int)e;
                ++own;
            }
        }
    }
#define TAKE(c)                                                                                    \
    do {                                                                                           \
        const uint8_t e_ = free_entries[next++];                                                   \
        memcpy(plan->palette + csm1(e_) * 4u, want[c], 4);                                         \
        written[e_] = 1;                                                                           \
        map[c] = e_;                                                                               \
        ++own;                                                                                     \
    } while (0)
    /* Clear: an entry of its own when every grey has one too, else a clear entry the fonts keep,
     * else the first free one. */
    if (map[0] < 0) {
        int needed = 0;
        for (int c = 1; c < COLOURS; ++c) {
            needed += map[c] < 0;
        }
        if (free_count > needed) {
            TAKE(0);
        } else {
            int best = -1, best_d = 0;
            for (unsigned e = 0; e < 256; ++e) {
                const int d = distance(colour(palette, e), want[0]);
                if (kept[e] && colour(palette, e)[3] == 0 && (best < 0 || d < best_d)) {
                    best = (int)e;
                    best_d = d;
                }
            }
            if (best >= 0) {
                map[0] = best;
                ++own;
            } else if (next < free_count) {
                TAKE(0);
            } else {
                return 1;
            }
        }
    }
    /* The greys, the commonest first, while free entries last. */
    for (int c = 1; c < COLOURS && next < free_count; ++c) {
        if (map[c] < 0) {
            TAKE(c);
        }
    }
#undef TAKE
    if (own < STADIA_ICONS_MIN_COLOURS) {
        return 1;
    }
    /* The rest: the nearest colour an entry holds, of those kept and those written. */
    for (int c = 1; c < COLOURS; ++c) {
        if (map[c] >= 0) {
            continue;
        }
        int best_d = 0;
        for (unsigned e = 0; e < 256; ++e) {
            if (kept[e] || written[e]) {
                const int d = distance(colour(plan->palette, e), want[c]);
                if (map[c] < 0 || d < best_d) {
                    map[c] = (int)e;
                    best_d = d;
                }
            }
        }
    }

    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        for (unsigned y = 0; y < CELL_H; ++y) {
            for (unsigned x = 0; x < CELL_W; ++x) {
                plan->cells[c][y][x] = (uint8_t)map[k_art_cells[c][y * CELL_W + x]];
            }
        }
    }
    plan->clear = (uint8_t)map[0];
    plan->own = own;
    plan->colours = COLOURS;
    plan->written = next;
    return 0;
}

void stadia_icons_apply(const stadia_icons_plan* plan, uint8_t* const fonts[], int count,
                        uint8_t* palette) {
    memcpy(palette, plan->palette, sizeof plan->palette);
    for (int f = 0; f < count; ++f) {
        for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
            for (unsigned y = 0; y < CELL_H; ++y) {
                memcpy(texel(fonts[f], k_art_u[c], y), plan->cells[c][y], CELL_W);
            }
        }
    }
}

int stadia_icons_paint(uint8_t* const fonts[], int count, uint8_t* palette) {
    static stadia_icons_plan plan;
    if (stadia_icons_plan_paint(fonts, count, palette, &plan) != 0) {
        return 1;
    }
    stadia_icons_apply(&plan, fonts, count, palette);
    return 0;
}

void stadia_icons_cell_rgb(const stadia_icons_plan* plan, int cell,
                           uint8_t rgb[STADIA_ICONS_CELL_H * STADIA_ICONS_CELL_W * 3]) {
    for (unsigned y = 0; y < CELL_H; ++y) {
        for (unsigned x = 0; x < CELL_W; ++x) {
            memcpy(rgb + (y * CELL_W + x) * 3u, colour(plan->palette, plan->cells[cell][y][x]), 3);
        }
    }
}

void stadia_icons_cell_sharp(const stadia_icons_plan* plan, int cell,
                             uint8_t sharp[STADIA_ICONS_SHARP_BYTES]) {
    const uint8_t* bg = colour(plan->palette, plan->clear);
    const uint8_t* runs = k_art_sharp[cell];
    uint8_t* out = sharp;
    uint8_t* const end = sharp + STADIA_ICONS_SHARP_BYTES;
    for (uint32_t i = 0; i + 1 < k_art_sharp_size[cell] && out < end; i += 2) {
        const uint8_t* ga = k_art_sharp_colours[runs[i + 1]];
        uint8_t px[3];
        for (int k = 0; k < 3; ++k) {
            px[k] = (uint8_t)((ga[0] * ga[1] + bg[k] * (255 - ga[1]) + 127) / 255);
        }
        for (unsigned n = 0; n < runs[i] && out < end; ++n, out += 3) {
            memcpy(out, px, 3);
        }
    }
}
