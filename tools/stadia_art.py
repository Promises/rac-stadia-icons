#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Henning Berge
#
# Writes src/common/stadia_art.h, the Stadia buttons the stadia-icons mod paints (README.md), from
# the pictures in art/stadia/ (Xelu's controller prompts, CC0: art/CREDITS.md). Needs Pillow:
#
#   python3 tools/stadia_art.py > src/common/stadia_art.h
#
# Each of the ten pad icon cells (24 x 16 texels) gets its picture fitted into the place the game's
# own button takes in that cell (measured on the game's fonts; the numbers, not the pictures, are
# kept here), its shape kept. Each is drawn twice:
#
#   sharp   at 6 times the cell (144 x 96), the picture resized once (Lanczos, premultiplied), a
#           grey and a coverage per pixel. The mod composites it on the cell's clear colour for
#           ports that draw sharp cells (interface 1.36).
#   cell    the sharp drawing averaged down to 24 x 16, then each texel one of a few greys with a
#           coverage (k-means over every cell together) or clear. The mod puts those greys in free
#           palette entries, the commonest first. At 24 x 16 the shoulders' and the sticks' own
#           labels (L1, R2, L...) are two or three texels high, a smudge that cannot tell L1 from
#           R1: in those cells the label is painted over in the button's grey and written again,
#           in its grey, in letters of 3 x 5 texels. The face buttons' letters read as they are.
#
# The greys are the pictures' own: the art is flat grey (R = G = B).
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ART = ROOT / "art" / "stadia"

CELL_W, CELL_H, SCALE = 24, 16, 6
CLEAR_BELOW = 16      # a texel covered less than this (of 255) is clear
COLOURS = 20          # the cells' greys, clear aside
RIM = 40              # the pictures' dark rims are greys below this
COVERAGE_WEIGHT = 0.5 # a coverage step counts half a grey step when the greys are chosen
SHARP_COLOURS = 255   # the sharp drawings' (grey, coverage) pairs, clear aside

# Where the game's own button lies in its cell (x, y, w, h in texels; the fonts of both games):
# the face buttons' sphere, the L1/R1 tabs, the same tabs given the cell's full height for the
# tall triggers, and the sticks' ball.
FACE = (2, 0, 16, 16)
TAB = (3, 2, 21, 13)
TRIGGER = (3, 0, 21, 16)
STICK = (0, 1, 15, 15)

# The ten cells, in the order of the glyph bytes 0x10 to 0x19, with the texel u of each in row 96
# (the glyph tables: see src/common/paint.h), and the label written again in the cell (None: the
# picture's own).
CELLS = [
    ("Cross", 0x10, 0, "a", FACE, None),
    ("Circle", 0x11, 72, "b", FACE, None),
    ("Triangle", 0x12, 48, "y", FACE, None),
    ("Square", 0x13, 24, "x", FACE, None),
    ("L1", 0x14, 96, "l1", TAB, "L1"),
    ("R1", 0x15, 144, "r1", TAB, "R1"),
    ("L2", 0x16, 120, "l2", TRIGGER, "L2"),
    ("R2", 0x17, 168, "r2", TRIGGER, "R2"),
    ("L3", 0x18, 208, "l_stick", STICK, "L"),
    ("R3", 0x19, 232, "r_stick", STICK, "R"),
]

# The labels' letters, 3 x 5 texels.
GLYPHS = {
    "L": ["#..", "#..", "#..", "#..", "###"],
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"],
}


def fit(art, area):
    """The picture's box in sharp pixels: its opaque part scaled to fit the area, centred."""
    bx0, by0, bx1, by1 = art.getchannel("A").getbbox()
    aw, ah = bx1 - bx0, by1 - by0
    x, y, w, h = (v * SCALE for v in area)
    k = min(w / aw, h / ah)
    fw, fh = round(aw * k), round(ah * k)
    return (bx0, by0, bx1, by1), (x + (w - fw) // 2, y + (h - fh) // 2, fw, fh)


def sharp_of(name, area):
    art = Image.open(ART / f"{name}.png").convert("RGBA")
    crop, (x, y, w, h) = fit(art, area)
    body = art.crop(crop).convert("RGBa").resize((w, h), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBa", (CELL_W * SCALE, CELL_H * SCALE), (0, 0, 0, 0))
    canvas.paste(body, (x, y))
    return canvas


def grey_alpha(im):
    """(grey, coverage) per pixel of an RGBa (premultiplied) picture, row by row."""
    out = []
    for r, g, b, a in im.convert("RGBA").get_flattened_data():
        out.append(((r + g + b + 1) // 3 if a else 0, a))
    return out


def relabel(name, sharp, small, text):
    """Writes `text` again over the label of a cell (small, its (grey, coverage) texels): the
    label's grey is the picture's brightest (name), its place where the drawing (sharp) has that
    grey, and the button's grey the one most of the button around it has (its rim aside)."""
    count = {}
    for r, g, b, a in Image.open(ART / f"{name}.png").convert("RGBA").get_flattened_data():
        if a == 255:
            count[r] = count.get(r, 0) + 1
    label = max(g for g, n in count.items() if n >= 30)
    px = grey_alpha(sharp)
    sw = CELL_W * SCALE
    near = [(i % sw, i // sw) for i, (g, a) in enumerate(px) if a == 255 and g >= label - 25]
    x0, y0 = min(x for x, _ in near), min(y for _, y in near)
    x1, y1 = max(x for x, _ in near), max(y for _, y in near)
    around = {}
    for y in range(max(0, y0 - 6), min(CELL_H * SCALE, y1 + 7)):
        for x in range(max(0, x0 - 6), min(sw, x1 + 7)):
            g, a = px[y * sw + x]
            if a == 255 and RIM < g < label - 40:
                around[g] = around.get(g, 0) + 1
    face = max(around, key=around.get)
    # Over the label, the button's grey (not its dark rim); then the letters, centred on it.
    lx0, ly0, lx1, ly1 = x0 // SCALE - 1, y0 // SCALE - 1, x1 // SCALE + 1, y1 // SCALE + 1
    for y in range(max(0, ly0), min(CELL_H, ly1 + 1)):
        for x in range(max(0, lx0), min(CELL_W, lx1 + 1)):
            g, a = small[y * CELL_W + x]
            if a >= 250 and g > RIM:
                small[y * CELL_W + x] = (face, 255)
    tw = 4 * len(text) - 1
    cx, cy = (x0 + x1 + 1) / 2 / SCALE, (y0 + y1 + 1) / 2 / SCALE
    tx, ty = round(cx - tw / 2), round(cy - 2.5)
    for k, ch in enumerate(text):
        for y, row in enumerate(GLYPHS[ch]):
            for x, on in enumerate(row):
                if on == "#":
                    small[(ty + y) * CELL_W + tx + 4 * k + x] = (label, 255)


def kmeans(points, weights, k, rounds=40):
    """Weighted k-means of (grey, coverage) pairs; centres sorted by their weight, heaviest first."""
    uniq = sorted(set(points))
    if len(uniq) <= k:
        centres = list(uniq)
    else:
        centres = [uniq[round(i * (len(uniq) - 1) / (k - 1))] for i in range(k)]
    for _ in range(rounds):
        sums = [[0.0, 0.0, 0.0] for _ in centres]
        for p, w in zip(points, weights):
            j = nearest(p, centres)
            sums[j][0] += p[0] * w
            sums[j][1] += p[1] * w
            sums[j][2] += w
        new = [(round(s[0] / s[2]), round(s[1] / s[2])) if s[2] else c for s, c in zip(sums, centres)]
        if new == centres:
            break
        centres = new
    weight = [0] * len(centres)
    for p, w in zip(points, weights):
        weight[nearest(p, centres)] += w
    order = sorted(range(len(centres)), key=lambda c: (-weight[c], centres[c]))
    return [centres[c] for c in order if weight[c] > 0]


def nearest(p, centres):
    return min(range(len(centres)),
               key=lambda c: (p[0] - centres[c][0]) ** 2 + (COVERAGE_WEIGHT * (p[1] - centres[c][1])) ** 2)


def rle(indices):
    """(run, value) byte pairs, runs of 1 to 255."""
    out, i = [], 0
    while i < len(indices):
        j = i
        while j < len(indices) and j - i < 255 and indices[j] == indices[i]:
            j += 1
        out += [j - i, indices[i]]
        i = j
    return out


def table(values, indent="    ", width=100):
    lines, line = [], indent
    for v in values:
        item = f"{v},"
        if len(line) + len(item) > width:
            lines.append(line.rstrip())
            line = indent
        line += item
    if line.strip():
        lines.append(line.rstrip())
    return "\n".join(lines)


def main():
    sharps = [sharp_of(art, area) for _, _, _, art, area, _ in CELLS]
    small = [grey_alpha(s.resize((CELL_W, CELL_H), Image.Resampling.BOX)) for s in sharps]
    for (_, _, _, art, _, text), sharp, cell in zip(CELLS, sharps, small):
        if text:
            relabel(art, sharp, cell, text)
    big = [grey_alpha(s) for s in sharps]

    # The cells' greys: every texel of every cell that is not clear, by how often it comes.
    pts = [p for cell in small for p in cell if p[1] >= CLEAR_BELOW]
    counts = {}
    for p in pts:
        counts[p] = counts.get(p, 0) + 1
    colours = kmeans(list(counts), list(counts.values()), COLOURS)
    cells = [[0 if p[1] < CLEAR_BELOW else 1 + nearest(p, colours) for p in cell] for cell in small]

    # The sharp drawings' pairs: clear is 0, the rest quantized together.
    spts = {}
    for cell in big:
        for p in cell:
            if p[1]:
                spts[p] = spts.get(p, 0) + 1
    sharp_colours = kmeans(list(spts), list(spts.values()), SHARP_COLOURS, rounds=12)
    lookup = {p: 1 + nearest(p, sharp_colours) for p in spts}
    sharp_runs = [rle([lookup[p] if p[1] else 0 for p in cell]) for cell in big]

    # The clear texels' colour: the pictures' edges' grey, which a filtered texel next to the
    # edge blends with.
    edge = [g for cell in big for g, a in cell if 0 < a < 255]
    clear = round(sum(edge) / len(edge))

    w = sys.stdout.write
    w("/* SPDX-License-Identifier: GPL-3.0-or-later\n"
      " * Copyright (c) 2026 Henning Berge\n"
      " *\n"
      " * Written by tools/stadia_art.py from art/stadia/ (Xelu's controller prompts, CC0:\n"
      " * art/CREDITS.md): the Stadia buttons the stadia-icons mod paints, for the ten pad icon cells in\n"
      " * the order of their glyph bytes, 0x10 to 0x19. Do not edit; run the script again.\n"
      " */\n"
      "#ifndef STADIA_ART_H\n#define STADIA_ART_H\n\n#include <stdint.h>\n\n")
    w(f"#define STADIA_ART_CELLS {len(CELLS)}\n")
    w(f"#define STADIA_ART_SCALE {SCALE} /* the sharp drawings, to the cell */\n")
    w(f"#define STADIA_ART_COLOURS {len(colours)} /* the cells' greys, clear aside */\n")
    w(f"#define STADIA_ART_SHARP_COLOURS {len(sharp_colours)}\n")
    w(f"#define STADIA_ART_CLEAR_GREY {clear} /* a clear texel's colour: the edges' grey */\n\n")
    w("/* Each cell's texel u in row 96. */\n")
    w("static const uint8_t k_art_u[STADIA_ART_CELLS] = {"
      + ", ".join(str(u) for _, _, u, _, _, _ in CELLS) + "};\n\n")
    w("/* The cells' greys, the commonest first: {grey, coverage of 255}. */\n")
    w("static const uint8_t k_art_colours[STADIA_ART_COLOURS][2] = {\n"
      + table([f"{{{g},{a}}}" for g, a in colours]) + "\n};\n\n")
    w("/* Each cell's texels, 24 x 16 row by row: 0 clear, else 1 + a k_art_colours entry. */\n")
    w("static const uint8_t k_art_cells[STADIA_ART_CELLS][16 * 24] = {\n")
    for (name, glyph, u, art, _, _), cell in zip(CELLS, cells):
        w(f"    /* 0x{glyph:02X} {name}: {art}.png */\n    {{\n")
        for y in range(CELL_H):
            w("        " + ",".join(f"{v:2d}" for v in cell[y * CELL_W:(y + 1) * CELL_W]) + ",\n")
        w("    },\n")
    w("};\n\n")
    w("/* The sharp drawings' {grey, coverage of 255} pairs, entry 0 clear. */\n")
    w("static const uint8_t k_art_sharp_colours[1 + STADIA_ART_SHARP_COLOURS][2] = {\n"
      + table(["{0,0}"] + [f"{{{g},{a}}}" for g, a in sharp_colours]) + "\n};\n\n")
    w("/* Each sharp drawing, 144 x 96 row by row, as (run, entry) byte pairs. */\n")
    for i, ((name, _, _, art, _, _), runs) in enumerate(zip(CELLS, sharp_runs)):
        w(f"static const uint8_t k_art_sharp_{i}[{len(runs)}] = {{ /* {name}: {art}.png */\n"
          + table(runs) + "\n};\n")
    w("static const uint8_t* const k_art_sharp[STADIA_ART_CELLS] = {\n"
      + table([f"k_art_sharp_{i}" for i in range(len(CELLS))]) + "\n};\n")
    w("static const uint32_t k_art_sharp_size[STADIA_ART_CELLS] = {\n"
      + table([len(r) for r in sharp_runs]) + "\n};\n\n")
    w("#endif /* STADIA_ART_H */\n")


if __name__ == "__main__":
    main()
