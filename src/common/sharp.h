/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 Henning Berge
 *
 * The stadia-icons mod (README.md): sharp cells, for a port that draws them (mod interface 1.36,
 * openrac_api.sharp_cells, openrac_sharp_page, openrac_sharp_cell: the SDK's own). The mod hands
 * the port, for each font it paints, the cells as painted and the same cells drawn 6 times as
 * large; such a port draws the large ones where the font's cells are shown.
 *
 * The manifest asks for 1.36, so the launcher loads the mod only on a port that has sharp_cells;
 * the mod still checks the port's table before it calls it (api_minor 36 or above, a table whose
 * size covers the field, the field set), as the SDK's C++ layer checks its own (mod.hpp, has_...()),
 * and paints the game-resolution buttons alone on a port without it.
 */
#ifndef STADIA_ICONS_SHARP_H
#define STADIA_ICONS_SHARP_H

#include "openrac/openrac_mod.h"

#include "paint.h"

#if OPENRAC_MOD_API_MINOR < 36
#error "stadia-icons needs an OpenRAC mod SDK of interface 1.36 or later (sharp_cells)"
#endif

/* What stadia_icons_send_sharp returns when the port has no sharp cells: not a code sharp_cells
 * returns (OPENRAC_OK or OPENRAC_E_UNKNOWN). */
#define STADIA_ICONS_NO_SHARP 1

/* 1 when the port has sharp_cells, else 0. */
int stadia_icons_has_sharp(const openrac_api* api);

/* Hands the port a page for each of `fonts` painted fonts (256 x 128, scale 6, the ten cells of
 * row 96 from `plan`). Returns STADIA_ICONS_NO_SHARP when the port has no sharp cells, or for a
 * count out of range (nothing is called then), else what sharp_cells returned. The pages and their pictures stay valid until the next call. */
int stadia_icons_send_sharp(const openrac_api* api, openrac_mod* mod,
                            const stadia_icons_plan* plan, int fonts);

#endif /* STADIA_ICONS_SHARP_H */
