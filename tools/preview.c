/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * Paints fonts as the mod does (paint.h) and writes the ten pad icon cells of the first font as a
 * PNG, three rows of them, each cell 144 x 96 on a dark backdrop: the cells as they were, the
 * painted cells (both blown up 6 times, texel by texel), and the sharp drawings a port with sharp
 * cells draws instead. The fonts are the test's own (tests/common/test_font.h), or a level's that
 * openrac-extractor --decompile wrote (core_index.bin and unpacked/core_data.bin):
 *
 *   preview <out.png> [level folder]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paint.h"
#include "stadia_art.h"
#include "test_font.h"

#define CELL_PX 144 /* a cell, 6 times */
#define GAP 8
#define IMG_W (STADIA_ICONS_CELLS * (CELL_PX + GAP) + GAP)
#define IMG_H (3 * (96 + GAP) + GAP)

static uint8_t s_image[IMG_H][IMG_W][3];

/* ---- PNG, uncompressed (stored deflate blocks) ---------------------------------------------- */

static uint32_t s_crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t* p, size_t n) {
    if (s_crc_table[1] == 0) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) {
                v = v & 1u ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            }
            s_crc_table[i] = v;
        }
    }
    c = ~c;
    for (size_t i = 0; i < n; ++i) {
        c = s_crc_table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    }
    return ~c;
}
static void be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}
static void chunk(FILE* out, const char* type, const uint8_t* data, uint32_t n) {
    uint8_t head[8];
    be32(head, n);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, out);
    fwrite(data, 1, n, out);
    uint8_t tail[4];
    be32(tail, crc(crc(0, (const uint8_t*)type, 4), data, n));
    fwrite(tail, 1, 4, out);
}
static int write_png(const char* path) {
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, out);
    uint8_t ihdr[13] = {0};
    be32(ihdr, IMG_W);
    be32(ihdr + 4, IMG_H);
    ihdr[8] = 8; /* bits */
    ihdr[9] = 2; /* RGB */
    chunk(out, "IHDR", ihdr, sizeof ihdr);

    const size_t row = 1 + IMG_W * 3, raw_n = row * IMG_H;
    uint8_t* raw = malloc(raw_n);
    for (size_t y = 0; y < IMG_H; ++y) {
        raw[y * row] = 0; /* no filter */
        memcpy(raw + y * row + 1, s_image[y], IMG_W * 3);
    }
    const size_t blocks = (raw_n + 65534) / 65535;
    uint8_t* z = malloc(2 + raw_n + blocks * 5 + 4);
    size_t n = 0;
    z[n++] = 0x78, z[n++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t at = 0; at < raw_n;) {
        const size_t len = raw_n - at < 65535 ? raw_n - at : 65535;
        z[n++] = at + len == raw_n;
        z[n++] = (uint8_t)len, z[n++] = (uint8_t)(len >> 8);
        z[n++] = (uint8_t)~len, z[n++] = (uint8_t)(~len >> 8);
        memcpy(z + n, raw + at, len);
        for (size_t i = 0; i < len; ++i) {
            a = (a + raw[at + i]) % 65521u;
            b = (b + a) % 65521u;
        }
        n += len;
        at += len;
    }
    be32(z + n, (b << 16) | a);
    n += 4;
    chunk(out, "IDAT", z, (uint32_t)n);
    chunk(out, "IEND", NULL, 0);
    free(z);
    free(raw);
    return fclose(out) != 0;
}

/* ---- the picture --------------------------------------------------------------------------- */

static void blend(int x, int y, const uint8_t* rgb, int alpha) {
    if (x < 0 || y < 0 || x >= IMG_W || y >= IMG_H) {
        return;
    }
    for (int k = 0; k < 3; ++k) {
        s_image[y][x][k] = (uint8_t)((rgb[k] * alpha + s_image[y][x][k] * (255 - alpha) + 127) / 255);
    }
}
static unsigned csm1(unsigned i) {
    return (i & ~0x18u) | ((i & 8u) << 1) | ((i & 0x10u) >> 1);
}
/* A font's cell, 6 times, texel by texel. */
static void draw_cell(const uint8_t* font, const uint8_t* palette, int cell, int x0, int y0) {
    for (int y = 0; y < 96; ++y) {
        for (int x = 0; x < CELL_PX; ++x) {
            const uint8_t i =
                font[(STADIA_ICONS_ROW + (unsigned)y / 6u) * 256u + stadia_icons_cell_u(cell) + (unsigned)x / 6u];
            const uint8_t* c = palette + csm1(i) * 4u;
            blend(x0 + x, y0 + y, c, c[3] >= 0x80 ? 255 : c[3] * 2);
        }
    }
}
/* A sharp drawing, its coverage from the art (the port's RGB has none). */
static void draw_sharp(const stadia_icons_plan* plan, int cell, int x0, int y0) {
    static uint8_t rgb[STADIA_ICONS_SHARP_BYTES];
    stadia_icons_cell_sharp(plan, cell, rgb);
    int p = 0;
    for (uint32_t i = 0; i + 1 < k_art_sharp_size[cell]; i += 2) {
        const int alpha = k_art_sharp_colours[k_art_sharp[cell][i + 1]][1];
        for (int n = 0; n < k_art_sharp[cell][i]; ++n, ++p) {
            blend(x0 + p % CELL_PX, y0 + p / CELL_PX, rgb + p * 3, alpha);
        }
    }
}

static uint8_t* slurp(const char* dir, const char* name, long* size) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* b = malloc((size_t)*size);
    if (b == NULL || fread(b, 1, (size_t)*size, f) != (size_t)*size) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    fclose(f);
    return b;
}
static int32_t s32(const uint8_t* p) {
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: preview <out.png> [level folder]\n");
        return 2;
    }
    uint8_t* fonts[3];
    uint8_t* palette;
    if (argc == 3) {
        long index_size, core_size;
        const uint8_t* index = slurp(argv[2], "core_index.bin", &index_size);
        uint8_t* core = slurp(argv[2], "unpacked/core_data.bin", &core_size);
        /* The core header: the FX textures' {count, offset} at 0x58, the FX bank at 0x68. */
        const int32_t fx = s32(index + 0x5C), bank = s32(index + 0x68);
        for (int f = 0; f < 3; ++f) {
            const uint8_t* e = index + fx + 16 * (f + 1);
            fonts[f] = core + bank + s32(e + 4);
            palette = core + bank + s32(e);
        }
    } else {
        test_lay();
        for (int f = 0; f < 3; ++f) {
            fonts[f] = s_fonts[f];
        }
        palette = s_palette;
    }

    for (int y = 0; y < IMG_H; ++y) {
        for (int x = 0; x < IMG_W; ++x) {
            const int light = ((x / 12) + (y / 12)) & 1;
            s_image[y][x][0] = light ? 50 : 40;
            s_image[y][x][1] = light ? 72 : 60;
            s_image[y][x][2] = light ? 104 : 90;
        }
    }
    printf("fonts   %08x\n", stadia_icons_hash(fonts, 3, palette));
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        draw_cell(fonts[0], palette, c, GAP + c * (CELL_PX + GAP), GAP);
    }
    static stadia_icons_plan plan;
    if (stadia_icons_plan_paint(fonts, 3, palette, &plan) != 0) {
        fprintf(stderr, "too few free palette entries\n");
        return 1;
    }
    stadia_icons_apply(&plan, fonts, 3, palette);
    printf("painted %08x, %d of %d colours their own, clear entry %d\n",
           stadia_icons_hash(fonts, 3, palette), plan.own, plan.colours, plan.clear);
    for (int c = 0; c < STADIA_ICONS_CELLS; ++c) {
        draw_cell(fonts[0], palette, c, GAP + c * (CELL_PX + GAP), 2 * GAP + 96);
        draw_sharp(&plan, c, GAP + c * (CELL_PX + GAP), 3 * GAP + 2 * 96);
    }
    return write_png(argv[1]);
}
