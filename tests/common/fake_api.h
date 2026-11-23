/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2026 the OpenRAC contributors
 * Copyright (c) 2026 Henning Berge
 *
 * The host table the load tests hand the library (openrac_mod.h), as the port does: console memory
 * of the test's own, a log that counts its lines, settings with Trace on, no hook points but the
 * tick callback, which the test keeps and calls; relocate_data as the executable's own (no level's
 * copy). make_api is a port of the SDK's interface with sharp_cells unset (the mod's check for a
 * port without sharp cells); make_api_sharp the same with sharp_cells, which keeps what it is
 * handed.
 */
#ifndef STADIA_ICONS_FAKE_API_H
#define STADIA_ICONS_FAKE_API_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openrac/openrac_mod.h"

#include "paint.h"
#include "sharp.h"
#include "stadia_icons.h"

#if defined(_WIN32)
#include <windows.h>
static void* open_library(const char* path) {
    return (void*)LoadLibraryA(path);
}
static void* symbol(void* lib, const char* name) {
    return (void*)GetProcAddress((HMODULE)lib, name);
}
#else
#include <dlfcn.h>
static void* open_library(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}
static void* symbol(void* lib, const char* name) {
    return dlsym(lib, name);
}
#endif

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

#define MEMORY_SIZE STADIA_ICONS_RAM
#define BANK 0x01000000u /* where the tests put the effect bank */
static uint8_t* s_memory;
static char s_failed[256];
static int s_hooks;
static int s_lines;
static openrac_frame_fn s_tick;
static void* s_tick_user;

static void host_log(openrac_mod* mod, int level, const char* text) {
    (void)mod;
    ++s_lines;
    printf("[%d] stadia-icons: %s\n", level, text);
}
static void host_fail(openrac_mod* mod, const char* reason) {
    (void)mod;
    snprintf(s_failed, sizeof s_failed, "%s", reason);
}
static int setting_bool(openrac_mod* mod, const char* key) {
    (void)mod;
    return strcmp(key, "trace") == 0;
}
static int64_t setting_int(openrac_mod* mod, const char* key) {
    (void)mod, (void)key;
    return 0;
}
static double setting_float(openrac_mod* mod, const char* key) {
    (void)mod, (void)key;
    return 0.0;
}
static const char* setting_string(openrac_mod* mod, const char* key) {
    (void)mod, (void)key;
    return "";
}
static int host_hook(openrac_mod* mod, const char* name, const char* signature, void* replacement,
                     void** original) {
    (void)mod, (void)name, (void)signature, (void)replacement, (void)original;
    ++s_hooks;
    return OPENRAC_HOOK_UNKNOWN;
}
static int host_on_frame(openrac_mod* mod, openrac_frame_fn fn, void* user) {
    (void)mod;
    s_tick = fn;
    s_tick_user = user;
    return OPENRAC_HOOK_OK;
}
static int host_on_pad(openrac_mod* mod, openrac_pad_fn fn, void* user) {
    (void)mod, (void)fn, (void)user;
    return OPENRAC_HOOK_OK;
}
static void* host_function(const char* name) {
    (void)name;
    return NULL;
}
static void* host_function_at(openrac_gaddr a) {
    (void)a;
    return NULL;
}
static openrac_gaddr identity(openrac_gaddr a) {
    return a;
}
static openrac_gaddr level_data(int level, openrac_gaddr a) {
    (void)level;
    return a;
}
static int loaded_level(void) {
    return 3;
}
static openrac_gaddr stack_push(uint32_t size, openrac_gaddr* saved) {
    (void)size;
    *saved = 0;
    return 0x001F0000u;
}
static void stack_pop(openrac_gaddr saved) {
    (void)saved;
}

static openrac_api make_api(const char* game, const char* version) {
    openrac_api api;
    memset(&api, 0, sizeof api);
    api.size = sizeof api;
    api.api_major = OPENRAC_MOD_API_MAJOR;
    api.api_minor = OPENRAC_MOD_API_MINOR;
    api.game = game;
    api.version = version;
    api.openrac_version = "test";
    api.platform = "test";
    api.memory = s_memory;
    api.log = host_log;
    api.fail = host_fail;
    api.setting_bool = setting_bool;
    api.setting_int = setting_int;
    api.setting_float = setting_float;
    api.setting_string = setting_string;
    api.hook = host_hook;
    api.on_frame = host_on_frame;
    api.on_pad = host_on_pad;
    api.function = host_function;
    api.function_at = host_function_at;
    api.relocate_data = identity;
    api.relocate_level_data = level_data;
    api.loaded_level = loaded_level;
    api.stack_push = stack_push;
    api.stack_pop = stack_pop;
    return api;
}

static int s_sharp_calls;
static int s_sharp_count;
static const openrac_sharp_page* s_sharp_pages;
static uint32_t s_sharp_hash; /* the fonts' hash when sharp_cells was called */
static uint32_t fonts_hash(void);

static int host_sharp_cells(openrac_mod* mod, const openrac_sharp_page* pages, int count) {
    (void)mod;
    ++s_sharp_calls;
    s_sharp_count = count;
    s_sharp_pages = pages;
    s_sharp_hash = fonts_hash();
    return OPENRAC_HOOK_OK;
}

static openrac_api make_api_sharp(const char* game, const char* version) {
    openrac_api api = make_api(game, version);
    api.sharp_cells = host_sharp_cells;
    return api;
}

static void put_u32(openrac_gaddr a, uint32_t v) {
    memcpy(s_memory + a, &v, 4);
}
static void put_u16(openrac_gaddr a, uint16_t v) {
    memcpy(s_memory + a, &v, 2);
}


/* The effect table of disc D: the three fonts' records, palette at bank + 0x800, the fonts at
 * + 0xC00, + 0x8C00, + 0x10C00 (as every level of both games has them). */
static void lay_table(const stadia_icons_disc* d, uint32_t bank) {
    put_u32(d->bank, bank);
    for (uint32_t f = 0; f < 3; ++f) {
        put_u16(d->table + 16u * (f + 1u) + 8u, (uint16_t)((0xC00u + f * 0x8000u) / 16u));
        put_u16(d->table + 16u * (f + 1u) + 0xAu, 0x800u / 16u);
    }
}
static uint32_t fonts_hash(void) {
    uint8_t* fonts[3] = {s_memory + BANK + 0xC00u, s_memory + BANK + 0x8C00u,
                         s_memory + BANK + 0x10C00u};
    return stadia_icons_hash(fonts, 3, s_memory + BANK + 0x800u);
}

/* A decompiled level's effect bank (core_index.bin and unpacked/core_data.bin, the core header's
 * effect bank at 0x68: the same in both games), from the environment variable VAR; NULL when there
 * is none. */
#define LEVEL_BANK_BYTES 0x18C00u
static uint8_t* read_level(const char* var) {
    const char* dir = getenv(var);
    if (dir == NULL || dir[0] == 0) {
        return NULL;
    }
    char path[1024];
    uint8_t index[0x70];
    snprintf(path, sizeof path, "%s/core_index.bin", dir);
    FILE* f = fopen(path, "rb");
    CHECK(f != NULL && fread(index, 1, sizeof index, f) == sizeof index);
    fclose(f);
    int32_t bank;
    memcpy(&bank, index + 0x68, 4);
    snprintf(path, sizeof path, "%s/unpacked/core_data.bin", dir);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    uint8_t* b = malloc(LEVEL_BANK_BYTES);
    CHECK(b != NULL);
    CHECK(fseek(f, bank, SEEK_SET) == 0 && fread(b, 1, LEVEL_BANK_BYTES, f) == LEVEL_BANK_BYTES);
    fclose(f);
    return b;
}

/* The library's entry points, and its query checked: its id, its version and the interface its
 * manifest names (1.36), whatever the SDK it was built with. */
static openrac_mod_info s_info;
static openrac_mod_load_fn open_mod(const char* path) {
    void* lib = open_library(path);
    CHECK(lib != NULL);
    openrac_mod_query_fn query = (openrac_mod_query_fn)symbol(lib, OPENRAC_MOD_QUERY);
    openrac_mod_load_fn load = (openrac_mod_load_fn)symbol(lib, OPENRAC_MOD_LOAD);
    CHECK(query != NULL && load != NULL);
    memset(&s_info, 0, sizeof s_info);
    s_info.size = sizeof s_info;
    query(&s_info);
    CHECK(s_info.api_major == OPENRAC_MOD_API_MAJOR && s_info.api_minor == 36);
    CHECK(strcmp(s_info.id, "stadia-icons") == 0 && strcmp(s_info.version, "1.0.0") == 0);
    s_memory = calloc(1, MEMORY_SIZE);
    CHECK(s_memory != NULL);
    return load;
}

/* The game's fonts (a level's bank, `bank`) on a port with sharp cells: on the first tick the sharp cells
 * are handed over, a page for each of the three fonts, while the fonts are still the game's
 * (`known` their hash); then the fonts are painted as on a port without sharp cells (`painted`),
 * and each page's cells show what the painted texels do. */
static void play_sharp(const char* game, const stadia_icons_disc* d, openrac_mod* mod,
                       openrac_mod_load_fn load, const uint8_t* bank, uint32_t known,
                       uint32_t painted) {
    memset(s_memory, 0, MEMORY_SIZE);
    openrac_api api = make_api_sharp(game, d->version);
    s_tick = NULL;
    CHECK(load(&api, mod) == 0 && s_tick != NULL);
    lay_table(d, BANK);
    memcpy(s_memory + BANK, bank, LEVEL_BANK_BYTES);
    s_sharp_calls = 0;
    s_tick(s_tick_user);
    CHECK(s_sharp_calls == 1 && s_sharp_count == 3 && s_sharp_hash == known);
    CHECK(fonts_hash() == painted);
    const uint8_t* palette = s_memory + BANK + 0x800u;
    for (int f = 0; f < 3; ++f) {
        const uint8_t* font = s_memory + BANK + 0xC00u + (uint32_t)f * 0x8000u;
        const openrac_sharp_page* page = &s_sharp_pages[f];
        CHECK(page->width == 256 && page->height == 128 && page->scale == 6 && page->count == 10);
        for (int c = 0; c < page->count; ++c) {
            const openrac_sharp_cell* cell = &page->cells[c];
            CHECK(cell->w == 24 && cell->h == 16 && cell->y == 96 && cell->sharp != NULL);
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 24; ++x) {
                    const unsigned i = font[(96u + (unsigned)y) * 256u + (unsigned)(cell->x + x)];
                    const unsigned e = (i & ~0x18u) | ((i & 8u) << 1) | ((i & 0x10u) >> 1);
                    CHECK(memcmp(cell->rgb + (y * 24 + x) * 3, palette + e * 4u, 3) == 0);
                }
            }
        }
    }
    s_tick(s_tick_user);
    CHECK(s_sharp_calls == 1 && fonts_hash() == painted);
}

#endif /* STADIA_ICONS_FAKE_API_H */
