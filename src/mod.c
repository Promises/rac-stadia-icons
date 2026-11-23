/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * Stadia button icons, a mod of OpenRAC's native port (README.md), for Ratchet & Clank (rac1) and
 * Going Commando (rac2), forked from xbox-icons. The game prints the controller's buttons as glyphs
 * of its three fonts; each tick (on_frame), the mod finds the fonts in game memory (stadia_icons.h;
 * where, the game's folder says: src/rac1/rac1.c, src/rac2/rac2.c) and, when they are the game's
 * own, paints Stadia buttons over the PlayStation 2 ones (paint.h). On a port that draws sharp
 * cells (interface 1.36), it hands the port the same buttons drawn 6 times as large first
 * (sharp.h). Painted fonts are no longer the game's, so they are painted once a level, and fonts
 * the mod does not know are left alone.
 */
#include <stdio.h>
#include <string.h>

#include "openrac/openrac_mod.h"

#include "paint.h"
#include "sharp.h"
#include "stadia_icons.h"

OPENRAC_MOD_DEFINE("stadia-icons", "1.0.0")

static const openrac_api* s_api;
static openrac_mod* s_mod;
static const stadia_icons_game* s_game;
static const stadia_icons_disc* s_disc;
static int s_trace;
static uint32_t s_unknown; /* the last fonts not known, said once */
static stadia_icons_plan s_plan;

static uint32_t get_u32(openrac_gaddr a) {
    uint32_t v;
    memcpy(&v, s_api->memory + a, 4);
    return v;
}
static uint16_t get_u16(openrac_gaddr a) {
    uint16_t v;
    memcpy(&v, s_api->memory + a, 2);
    return v;
}

static void say(const char* text) {
    if (s_trace) {
        s_api->log(s_mod, OPENRAC_LOG_INFO, text);
    }
}

static void tick(void* user) {
    (void)user;
    const uint32_t bank = get_u32(s_api->relocate_data(s_disc->bank));
    const openrac_gaddr table = s_api->relocate_data(s_disc->table);
    if (bank == 0 || bank >= STADIA_ICONS_RAM) {
        return;
    }
    /* Fonts 1 and 2 always; the third where it is loaded (a level has it, the title's menus do not:
     * its record is zero there). Each with the palette the first has. */
    uint8_t* fonts[STADIA_ICONS_FONTS];
    int count = 0;
    uint32_t palette = 0;
    for (int f = 0; f < STADIA_ICONS_FONTS; ++f) {
        const openrac_gaddr record = table + 16u * (uint32_t)(f + 1);
        const uint16_t texel_offset = get_u16(record + 8u), clut_offset = get_u16(record + 0xAu);
        const uint32_t texels = bank + texel_offset * 16u;
        const uint32_t clut = bank + clut_offset * 16u;
        if (texel_offset == 0 && clut_offset == 0 && f >= 2) {
            break;
        }
        if (texels + STADIA_ICONS_FONT_BYTES > STADIA_ICONS_RAM || clut + 1024u > STADIA_ICONS_RAM ||
            (f > 0 && clut != palette)) {
            return;
        }
        fonts[count++] = s_api->memory + texels;
        palette = clut;
    }
    const uint32_t hash = stadia_icons_hash(fonts, count, s_api->memory + palette);
    int known = 0;
    for (size_t i = 0; i < s_game->font_count; ++i) {
        known |= hash == s_game->fonts[i];
    }
    if (!known) {
        if (hash != s_unknown) {
            s_unknown = hash;
            char line[96];
            snprintf(line, sizeof line, "%d fonts %08x at 0x%08x: not the game's, left as they are",
                     count, (unsigned)hash, (unsigned)bank);
            say(line);
        }
        return;
    }
    if (stadia_icons_plan_paint(fonts, count, s_api->memory + palette, &s_plan) != 0) {
        if (hash != s_unknown) {
            s_unknown = hash;
            s_api->log(s_mod, OPENRAC_LOG_WARN,
                       "too few free palette entries for the Stadia buttons: left as they are");
        }
        return;
    }
    /* The sharp cells first: the port is to know them before it sees the painted texels. */
    const int sharp = stadia_icons_send_sharp(s_api, s_mod, &s_plan, count);
    stadia_icons_apply(&s_plan, fonts, count, s_api->memory + palette);
    s_unknown = stadia_icons_hash(fonts, count, s_api->memory + palette); /* painted: no word */
    char line[160];
    snprintf(line, sizeof line,
             "painted the %d fonts %08x at 0x%08x, %d of %d colours their own; sharp cells: %s",
             count, (unsigned)hash, (unsigned)bank, s_plan.own, s_plan.colours,
             sharp < 0 ? "not in this port" : sharp == OPENRAC_OK ? "sent" : "refused");
    say(line);
}

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    static const stadia_icons_game* const games[] = {&stadia_icons_rac1, &stadia_icons_rac2};
    s_api = api;
    s_mod = mod;
    s_game = NULL;
    s_disc = NULL;
    s_unknown = 0;
    for (size_t i = 0; i < sizeof games / sizeof games[0]; ++i) {
        if (strcmp(api->game, games[i]->game) == 0) {
            s_game = games[i];
        }
    }
    if (s_game == NULL) {
        api->fail(mod, "stadia-icons is for Ratchet & Clank (rac1) and Going Commando (rac2)");
        return 1;
    }
    for (size_t i = 0; i < s_game->disc_count; ++i) {
        if (strcmp(api->version, s_game->discs[i].version) == 0) {
            s_disc = &s_game->discs[i];
        }
    }
    if (s_disc == NULL) {
        char reason[128];
        snprintf(reason, sizeof reason, "stadia-icons knows %s, not %s", s_game->discs_known,
                 api->version);
        api->fail(mod, reason);
        return 1;
    }
    s_trace = api->setting_bool(mod, "trace");
    if (api->on_frame(mod, tick, NULL) != 0) {
        api->fail(mod, "no tick callback in this OpenRAC");
        return 1;
    }
    return 0;
}
