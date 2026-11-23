/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * The stadia-icons mod (README.md): Stadia buttons painted over the PlayStation 2 ones of the
 * game's three fonts. A fork of xbox-icons' painting: the same fonts, cells, hash and palette rule.
 *
 * A font is an effect texture, 256 x 128 texels of 8 bits, and the three share one palette of 256
 * colours (32-bit, alpha 0x80 opaque, in the GS's CSM1 order: within each 32 entries the middle two
 * groups of 8 trade places). The pad icons are the cells of row 96, 24 x 16 texels each (the glyph
 * tables: rac1's port/assets/world/font.h; Going Commando's are the same, src/rac2/rac2.c):
 *
 *   u    0     24      48        72      96  120  144  168  192..207  208  232
 *   byte 0x10  0x13    0x12      0x11    0x14 0x16 0x15 0x17 (letters) 0x18 0x19
 *        Cross Square  Triangle  Circle  L1   L2   R1   R2   kept      L3   R3
 *        A     X       Y         B       L1   L2   R1   R2             L    R   (Stadia)
 *
 * Every texel of the ten cells is painted from the Stadia pictures (stadia_art.h): a few greys
 * with a coverage, and clear. The greys go to palette entries no texel outside the cells uses (the
 * PlayStation symbols' own among them: in the game's fonts, all but some fifteen entries); an entry
 * a texel outside the cells uses is never changed, though a grey it already holds is used as it is.
 * With too few free entries for every grey, the commonest greys get one and the others take the
 * nearest colour there is (fewer greys); with fewer than STADIA_ICONS_MIN_COLOURS of the art's
 * colours placed, nothing is painted.
 */
#ifndef STADIA_ICONS_PAINT_H
#define STADIA_ICONS_PAINT_H

#include <stdint.h>

#define STADIA_ICONS_WIDTH 256u       /* texels a row of a font */
#define STADIA_ICONS_HEIGHT 128u
#define STADIA_ICONS_ROW 96u          /* the pad icons' row of cells */
#define STADIA_ICONS_CELL_W 24u
#define STADIA_ICONS_CELL_H 16u
#define STADIA_ICONS_CELLS 10         /* glyph bytes 0x10 to 0x19 */
#define STADIA_ICONS_HASH_WIDTH 192u  /* the eight cells the hash reads, u 0..191 (xbox-icons') */
#define STADIA_ICONS_FONTS 3          /* the most fonts a set has: a level's three */
#define STADIA_ICONS_MIN_COLOURS 4    /* clear and three greys, at the least */

/* A cell's u in row 96; the cells go in the order of their glyph bytes (0x10 Cross ... 0x19 R3). */
unsigned stadia_icons_cell_u(int cell);

/* FNV-1a over the eight cells of u 0..191 of each of the `count` fonts and the palette: which fonts
 * these are (the game's, or painted). The same hash as xbox-icons', so the same known fonts. */
uint32_t stadia_icons_hash(uint8_t* const fonts[], int count, const uint8_t* palette);

/* What painting a set of fonts writes, worked out before anything is written. */
typedef struct stadia_icons_plan {
    uint8_t palette[1024]; /* the palette as painted (CSM1 order, as the game's) */
    uint8_t cells[STADIA_ICONS_CELLS][STADIA_ICONS_CELL_H][STADIA_ICONS_CELL_W]; /* as painted */
    uint8_t clear;         /* the entry of the clear texels */
    int own;               /* the art's colours (clear among them) with an entry of their own */
    int colours;           /* the art's colours, clear among them */
    int written;           /* palette entries written */
} stadia_icons_plan;

/* Works out the painting of the `count` fonts (1 to STADIA_ICONS_FONTS) and their palette into
 * `plan`, writing nothing. Returns 0, or 1 when too few palette entries are free (fewer than
 * STADIA_ICONS_MIN_COLOURS colours of the art's own) or `count` is out of range. */
int stadia_icons_plan_paint(uint8_t* const fonts[], int count, const uint8_t* palette,
                            stadia_icons_plan* plan);

/* Writes a plan: its palette entries, and the ten cells of each of the `count` fonts. */
void stadia_icons_apply(const stadia_icons_plan* plan, uint8_t* const fonts[], int count,
                        uint8_t* palette);

/* Both: returns 0, or 1 with nothing changed (stadia_icons_plan_paint). */
int stadia_icons_paint(uint8_t* const fonts[], int count, uint8_t* palette);

/* A planned cell as the GS shows it: 24 x 16 RGB, the palette applied (alpha left out). */
void stadia_icons_cell_rgb(const stadia_icons_plan* plan, int cell,
                           uint8_t rgb[STADIA_ICONS_CELL_H * STADIA_ICONS_CELL_W * 3]);

/* A cell's sharp drawing, STADIA_ICONS_SHARP_SCALE times the cell (144 x 96 RGB), composited on
 * the plan's clear colour as the painted cell's clear texels have it. */
#define STADIA_ICONS_SHARP_SCALE 6
#define STADIA_ICONS_SHARP_BYTES                                                                   \
    (STADIA_ICONS_CELL_W * STADIA_ICONS_SHARP_SCALE * STADIA_ICONS_CELL_H *                         \
     STADIA_ICONS_SHARP_SCALE * 3u)
void stadia_icons_cell_sharp(const stadia_icons_plan* plan, int cell,
                             uint8_t sharp[STADIA_ICONS_SHARP_BYTES]);

#endif /* STADIA_ICONS_PAINT_H */
