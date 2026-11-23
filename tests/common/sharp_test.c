/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Henning Berge
 *
 * Sharp cells (sharp.h) against host tables made here: a port of interface 1.35 (no sharp_cells),
 * one that says 1.36 with a table too short for it, one with the field but saying 1.35, and one of
 * 1.36 with it. Only the last is called: once, with a page for each font (256 x 128, scale 6, the
 * ten cells of row 96), each cell's RGB the painted cell's as the GS shows it and its sharp drawing
 * 144 x 96, on the same clear colour, and the port's answer handed back.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paint.h"
#include "sharp.h"
#include "stadia_art.h"
#include "test_font.h"

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/* A 1.36 table: 1.35's, then sharp_cells. */
typedef struct api_136 {
    openrac_api api;
    stadia_icons_sharp_cells_fn sharp_cells;
} api_136;

static int s_calls;
static openrac_mod* s_mod_seen;
static int32_t s_count;
static int32_t s_answer;
static openrac_sharp_page s_pages_seen[3];

static int32_t host_sharp_cells(openrac_mod* mod, const openrac_sharp_page* pages, int32_t count) {
    ++s_calls;
    s_mod_seen = mod;
    s_count = count;
    for (int32_t i = 0; i < count && i < 3; ++i) {
        s_pages_seen[i] = pages[i];
    }
    return s_answer;
}

static api_136 make(uint32_t minor, uint32_t size) {
    api_136 t;
    memset(&t, 0, sizeof t);
    t.api.size = size;
    t.api.api_major = OPENRAC_MOD_API_MAJOR;
    t.api.api_minor = minor;
    t.sharp_cells = host_sharp_cells;
    return t;
}

int main(void) {
    /* sharp_cells sits where 1.36 appends it. */
    CHECK(offsetof(api_136, sharp_cells) == STADIA_ICONS_SHARP_OFFSET);

    static stadia_icons_plan plan;
    uint8_t* fonts[3] = {s_fonts[0], s_fonts[1], s_fonts[2]};
    test_lay();
    CHECK(stadia_icons_plan_paint(fonts, 3, s_palette, &plan) == 0);
    openrac_mod* mod = (openrac_mod*)&plan; /* any handle: the mod only passes it back */

    /* No sharp_cells: not read, not called. */
    const api_136 old = make(35, sizeof(openrac_api));
    const api_136 short_table = make(36, sizeof(openrac_api));
    const api_136 old_minor = make(35, sizeof(api_136));
    CHECK(stadia_icons_sharp_cells(&old.api) == NULL);
    CHECK(stadia_icons_sharp_cells(&short_table.api) == NULL);
    CHECK(stadia_icons_sharp_cells(&old_minor.api) == NULL);
    CHECK(stadia_icons_send_sharp(&old.api, mod, &plan, 3) == -1);
    CHECK(stadia_icons_send_sharp(&short_table.api, mod, &plan, 3) == -1);
    CHECK(stadia_icons_send_sharp(&old_minor.api, mod, &plan, 3) == -1);
    CHECK(s_calls == 0);

    /* 1.36: one call, a page a font. */
    const api_136 now = make(36, sizeof(api_136));
    CHECK(stadia_icons_sharp_cells(&now.api) == host_sharp_cells);
    s_answer = OPENRAC_OK;
    CHECK(stadia_icons_send_sharp(&now.api, mod, &plan, 3) == OPENRAC_OK);
    CHECK(s_calls == 1 && s_mod_seen == mod && s_count == 3);

    /* What the cells show once painted, to hold the pages to. */
    stadia_icons_apply(&plan, fonts, 3, s_palette);
    const uint8_t* clear = test_colour(s_palette, plan.clear);
    for (int p = 0; p < 3; ++p) {
        const openrac_sharp_page* page = &s_pages_seen[p];
        CHECK(page->size == sizeof(openrac_sharp_page));
        CHECK(page->width == 256 && page->height == 128 && page->scale == 6);
        CHECK(page->count == STADIA_ICONS_CELLS && page->cells != NULL);
        for (int c = 0; c < page->count; ++c) {
            const openrac_sharp_cell* cell = &page->cells[c];
            CHECK(cell->size == sizeof(openrac_sharp_cell));
            CHECK(cell->x == (int32_t)stadia_icons_cell_u(c) && cell->y == 96 && cell->w == 24 &&
                  cell->h == 16);
            CHECK(cell->rgb != NULL && cell->sharp != NULL);
            /* rgb: the painted texels of every font, the palette applied. */
            for (int f = 0; f < 3; ++f) {
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 24; ++x) {
                        const uint8_t i = *test_at(f, (unsigned)(cell->x + x), (unsigned)(96 + y));
                        CHECK(memcmp(cell->rgb + (y * 24 + x) * 3, test_colour(s_palette, i), 3) == 0);
                    }
                }
            }
            /* sharp: 144 x 96, the clear colour where the picture is not, and the picture where
             * it is; averaged 6 x 6 near the painted cell where the cell is the picture's own (the
             * face buttons: the others' labels are written again in the cells). */
            CHECK(memcmp(cell->sharp, clear, 3) == 0);
            CHECK(memcmp(cell->sharp + (95 * 144 + 143) * 3, clear, 3) == 0);
            int coloured = 0;
            long error = 0;
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 24; ++x) {
                    int sum = 0;
                    for (int sy = 0; sy < 6; ++sy) {
                        for (int sx = 0; sx < 6; ++sx) {
                            const uint8_t* px = cell->sharp + ((y * 6 + sy) * 144 + x * 6 + sx) * 3;
                            CHECK(px[0] == px[1] && px[1] == px[2]); /* greys, on a grey */
                            sum += px[0];
                            coloured += memcmp(px, clear, 3) != 0;
                        }
                    }
                    const uint8_t* texel = cell->rgb + (y * 24 + x) * 3;
                    error += abs(sum / 36 - texel[0]);
                }
            }
            CHECK(coloured > 144 * 96 / 6);
            if (c < 4) {
                CHECK(error / (24 * 16) <= 6);
            }
        }
        /* The fonts are painted alike: their pages are the same cells. */
        CHECK(page->cells == s_pages_seen[0].cells);
    }

    /* The title's two fonts: two pages; and the port's refusal is handed back. */
    s_answer = OPENRAC_E_UNKNOWN;
    CHECK(stadia_icons_send_sharp(&now.api, mod, &plan, 2) == OPENRAC_E_UNKNOWN);
    CHECK(s_calls == 2 && s_count == 2);
    CHECK(stadia_icons_send_sharp(&now.api, mod, &plan, 0) == -1 && s_calls == 2);

    printf("stadia_icons_sharp_test: ok\n");
    return 0;
}
