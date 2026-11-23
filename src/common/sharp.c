/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Henning Berge
 *
 * Sharp cells for a port that draws them (sharp.h).
 */
#include "sharp.h"

#include <string.h>

#if OPENRAC_MOD_API_MINOR == 35
/* 1.35's table ends with moby_uid: 1.36's sharp_cells comes right after it. */
_Static_assert(STADIA_ICONS_SHARP_OFFSET == sizeof(openrac_api),
               "openrac_api 1.35 does not end with moby_uid");
#endif

stadia_icons_sharp_cells_fn stadia_icons_sharp_cells(const openrac_api* api) {
    stadia_icons_sharp_cells_fn fn = NULL;
    if (api->api_minor < 36 || api->size < STADIA_ICONS_SHARP_OFFSET + sizeof fn) {
        return NULL;
    }
    memcpy(&fn, (const char*)api + STADIA_ICONS_SHARP_OFFSET, sizeof fn);
    return fn;
}

/* One set of cells serves every page: the fonts are painted alike, with one palette. */
static uint8_t s_rgb[STADIA_ICONS_CELLS][STADIA_ICONS_CELL_H * STADIA_ICONS_CELL_W * 3];
static uint8_t s_sharp[STADIA_ICONS_CELLS][STADIA_ICONS_SHARP_BYTES];
static openrac_sharp_cell s_cells[STADIA_ICONS_CELLS];
static openrac_sharp_page s_pages[STADIA_ICONS_FONTS];

int stadia_icons_send_sharp(const openrac_api* api, openrac_mod* mod,
                            const stadia_icons_plan* plan, int fonts) {
    const stadia_icons_sharp_cells_fn sharp_cells = stadia_icons_sharp_cells(api);
    if (sharp_cells == NULL || fonts < 1 || fonts > STADIA_ICONS_FONTS) {
        return -1;
    }
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        stadia_icons_cell_rgb(plan, c, s_rgb[c]);
        stadia_icons_cell_sharp(plan, c, s_sharp[c]);
        memset(&s_cells[c], 0, sizeof s_cells[c]);
        s_cells[c].size = sizeof s_cells[c];
        s_cells[c].x = (int32_t)stadia_icons_cell_u(c);
        s_cells[c].y = (int32_t)STADIA_ICONS_ROW;
        s_cells[c].w = (int32_t)STADIA_ICONS_CELL_W;
        s_cells[c].h = (int32_t)STADIA_ICONS_CELL_H;
        s_cells[c].rgb = s_rgb[c];
        s_cells[c].sharp = s_sharp[c];
    }
    for (int f = 0; f < fonts; ++f) {
        memset(&s_pages[f], 0, sizeof s_pages[f]);
        s_pages[f].size = sizeof s_pages[f];
        s_pages[f].width = (int32_t)STADIA_ICONS_WIDTH;
        s_pages[f].height = (int32_t)STADIA_ICONS_HEIGHT;
        s_pages[f].scale = STADIA_ICONS_SHARP_SCALE;
        s_pages[f].cells = s_cells;
        s_pages[f].count = STADIA_ICONS_CELLS;
    }
    return sharp_cells(mod, s_pages, fonts);
}
