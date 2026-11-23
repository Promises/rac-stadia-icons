/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Henning Berge
 *
 * The stadia-icons mod (README.md): sharp cells, for a port that draws them (mod interface 1.36,
 * openrac_api.sharp_cells). The mod hands the port, for each font it paints, the cells as painted
 * and the same cells drawn 6 times as large; such a port draws the large ones where the font's
 * cells are shown. A port without it draws the painted texels as they are.
 *
 * Until the SDK this mod is built with has interface 1.36, the call's types are declared here as
 * the port's are, and the call is read from the table at the place 1.36 appends it (just after
 * 1.35's last entry, moby_uid), only when the port says it has it: api_minor 36 or above and a
 * table whose size covers it, as the SDK's C++ layer checks its own (mod.hpp, has_...()).
 */
#ifndef STADIA_ICONS_SHARP_H
#define STADIA_ICONS_SHARP_H

#include <stddef.h>
#include <stdint.h>

#include "openrac/openrac_mod.h"

#include "paint.h"

#if OPENRAC_MOD_API_MINOR >= 36
/* The SDK's own declarations. */
#define STADIA_ICONS_SHARP_OFFSET offsetof(openrac_api, sharp_cells)
#else
/* One cell of a font page: where it is (texels), what it shows as painted (w * h RGB) and what to
 * draw instead (w * scale by h * scale RGB). */
typedef struct openrac_sharp_cell {
    uint32_t size; /* sizeof(openrac_sharp_cell) */
    int32_t x, y, w, h;
    const uint8_t* rgb;
    const uint8_t* sharp;
} openrac_sharp_cell;

/* One texture's sharp cells: its size in texels, how much larger the sharp cells are. */
typedef struct openrac_sharp_page {
    uint32_t size; /* sizeof(openrac_sharp_page) */
    int32_t width, height, scale;
    const openrac_sharp_cell* cells;
    int32_t count;
} openrac_sharp_page;

/* Where interface 1.36 puts sharp_cells: after the 1.35 table's last entry. */
#define STADIA_ICONS_SHARP_OFFSET (offsetof(openrac_api, moby_uid) + sizeof(void (*)(void)))
#endif

/* openrac_api.sharp_cells: replaces the mod's pages with `count` new ones (0 clears them); called
 * before the texels are painted. Returns OPENRAC_OK, or an OPENRAC_E_ code. */
typedef int32_t (*stadia_icons_sharp_cells_fn)(openrac_mod* mod, const openrac_sharp_page* pages,
                                               int32_t count);

/* The port's sharp_cells, or NULL when it has none. */
stadia_icons_sharp_cells_fn stadia_icons_sharp_cells(const openrac_api* api);

/* Hands the port a page for each of `fonts` painted fonts (256 x 128, scale 6, the ten cells of
 * row 96 from `plan`). Returns -1 when the port has no sharp cells (nothing is called), else what
 * sharp_cells returned. The pages and their pictures stay valid until the next call. */
int stadia_icons_send_sharp(const openrac_api* api, openrac_mod* mod,
                            const stadia_icons_plan* plan, int fonts);

#endif /* STADIA_ICONS_SHARP_H */
