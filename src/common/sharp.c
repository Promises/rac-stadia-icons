/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Henning Berge
 *
 * Sharp cells for a port that draws them (sharp.h).
 */
#include "sharp.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int stadia_icons_has_sharp(const openrac_api* api) {
    return api->api_minor >= 36 &&
           api->size >= offsetof(openrac_api, sharp_cells) + sizeof api->sharp_cells &&
           api->sharp_cells != NULL;
}

/* One set of cells serves every page: the fonts are painted alike, with one palette. */
static uint8_t s_rgb[STADIA_ICONS_CELLS][STADIA_ICONS_CELL_H * STADIA_ICONS_CELL_W * 3];
static uint8_t s_sharp[STADIA_ICONS_CELLS][STADIA_ICONS_SHARP_BYTES];
static openrac_sharp_cell s_cells[STADIA_ICONS_CELLS];
static openrac_sharp_page s_pages[STADIA_ICONS_FONTS];

int stadia_icons_send_sharp(const openrac_api* api, openrac_mod* mod,
                            const stadia_icons_plan* plan, int fonts) {
    if (!stadia_icons_has_sharp(api) || fonts < 1 || fonts > STADIA_ICONS_FONTS) {
        return STADIA_ICONS_NO_SHARP;
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
    return api->sharp_cells(mod, s_pages, fonts);
}
