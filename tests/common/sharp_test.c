/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Henning Berge
 *
 * Sharp cells (sharp.h) against host tables made here (the SDK's openrac_api). The manifest asks
 * for 1.36, so the launcher gives the mod no port without sharp_cells; the mod checks anyway, and
 * these are the ports it must not call: one of 1.35 (a table that ends before sharp_cells), one
 * that says 1.36 with a table too short for it, one with the field but saying 1.35, and one of
 * 1.36 with the field unset. A port of 1.36 or later with it set is called: once, with a page for
 * each font (256 x 128, scale 6, the ten cells of row 96), each cell's RGB the painted cell's as
 * the GS shows it and its sharp drawing 144 x 96, on the same clear colour, and the port's answer
 * handed back.
 */
#include <stddef.h>
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

static int s_calls;
static openrac_mod* s_mod_seen;
static int s_count;
static int s_answer;
static openrac_sharp_page s_pages_seen[3];

static int host_sharp_cells(openrac_mod* mod, const openrac_sharp_page* pages, int count) {
    ++s_calls;
    s_mod_seen = mod;
    s_count = count;
    for (int i = 0; i < count && i < 3; ++i) {
        s_pages_seen[i] = pages[i];
    }
    return s_answer;
}

/* A table of interface 1.`minor`, `size` bytes of it the port's, sharp_cells set. */
static openrac_api make(uint32_t minor, size_t size) {
    openrac_api t;
    memset(&t, 0, sizeof t);
    t.size = (uint32_t)size;
    t.api_major = OPENRAC_MOD_API_MAJOR;
    t.api_minor = minor;
    t.sharp_cells = host_sharp_cells;
    return t;
}

int main(void) {
    static stadia_icons_plan plan;
    uint8_t* fonts[3] = {s_fonts[0], s_fonts[1], s_fonts[2]};
    test_lay();
    CHECK(stadia_icons_plan_paint(fonts, 3, s_palette, &plan) == 0);
    openrac_mod* mod = (openrac_mod*)&plan; /* any handle: the mod only passes it back */

    /* No sharp_cells: not called. */
    const size_t before = offsetof(openrac_api, sharp_cells);
    const size_t with = before + sizeof(((openrac_api*)0)->sharp_cells);
    const openrac_api old = make(35, before);
    const openrac_api short_table = make(36, before);
    const openrac_api old_minor = make(35, with);
    openrac_api unset = make(36, with);
    unset.sharp_cells = NULL;
    const openrac_api* none[] = {&old, &short_table, &old_minor, &unset};
    for (size_t i = 0; i < sizeof none / sizeof none[0]; ++i) {
        CHECK(!stadia_icons_has_sharp(none[i]));
        CHECK(stadia_icons_send_sharp(none[i], mod, &plan, 3) == STADIA_ICONS_NO_SHARP);
    }
    CHECK(s_calls == 0);

    /* 1.36 (and a later port's whole table): one call, a page a font. */
    CHECK(stadia_icons_has_sharp(&(openrac_api){.size = (uint32_t)with, .api_minor = 36,
                                                .sharp_cells = host_sharp_cells}));
    const openrac_api now = make(OPENRAC_MOD_API_MINOR, sizeof(openrac_api));
    CHECK(stadia_icons_has_sharp(&now));
    s_answer = OPENRAC_OK;
    CHECK(stadia_icons_send_sharp(&now, mod, &plan, 3) == OPENRAC_OK);
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
    CHECK(stadia_icons_send_sharp(&now, mod, &plan, 2) == OPENRAC_E_UNKNOWN);
    CHECK(s_calls == 2 && s_count == 2);
    CHECK(stadia_icons_send_sharp(&now, mod, &plan, 0) == STADIA_ICONS_NO_SHARP && s_calls == 2);

    printf("stadia_icons_sharp_test: ok\n");
    return 0;
}
