// SPDX-License-Identifier: GPL-3.0-or-later
#include "duo_font.h"
#include <string.h>

_Static_assert(sizeof(EdenDsmodFontGlyph) == DUO_FONT_RECORD, "Pinned font glyph ABI changed");
_Static_assert(sizeof(EdenDsmodFontExtensions) == 24, "Pinned font extension ABI changed");

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
static int16_t signed16(const uint8_t *p) {
    const uint16_t n = le16(p);
    return (int16_t)(n <= 32767 ? (int32_t)n : (int32_t)n - 65536);
}

EdenDsmodBool duo_font_decode(void *instance, const uint8_t *bytes, size_t size, void *receiver, EdenDsmodFontSink sink) {
    (void)instance;
    // Exact version1, fixed1024x512 atlas, cap-height32, ASCII32..126,
    // 95 records*14bytes. All header/reserved bytes are canonical little-endian.
    static const uint8_t header[DUO_FONT_HEADER] = {
        'D','U','O','S','A','N','S','1', 1,0, 32,0, 0,4, 0,2,
        32,0, 32,0, 95,0, 14,0, 0x32,0x05,0,0, 0,0,0,0
    };
    if (!bytes || !sink || size != DUO_FONT_SIZE || memcmp(bytes, header, sizeof header)) return EDEN_DSMOD_FALSE;
    EdenDsmodFontGlyph glyphs[DUO_FONT_COUNT];
    for (uint32_t i = 0; i < DUO_FONT_COUNT; ++i) {
        const uint8_t *p = bytes + DUO_FONT_HEADER + i * DUO_FONT_RECORD;
        EdenDsmodFontGlyph *g = &glyphs[i];
        *g = (EdenDsmodFontGlyph){le16(p), le16(p+2), le16(p+4), le16(p+6), signed16(p+8), signed16(p+10), le16(p+12)};
        if (g->x != (i % 16) * 64 + 2 || g->y != (i / 16) * 80 + 2 ||
            g->w > 60 || g->h > 76 || (uint32_t)g->x + g->w > 1024 || (uint32_t)g->y + g->h > 512 ||
            g->bearing_x < -64 || g->bearing_x > 64 || g->bearing_y < -64 || g->bearing_y > 64 ||
            !g->advance || g->advance > 64) return EDEN_DSMOD_FALSE;
        if (i == 0) {
            if (g->w || g->h || g->bearing_x || g->bearing_y) return EDEN_DSMOD_FALSE;
        } else if (!g->w || !g->h) return EDEN_DSMOD_FALSE;
    }
    // Pinned host copies every record synchronously. No input/receiver pointer persists.
    sink(receiver, 32, DUO_FONT_FIRST, glyphs, DUO_FONT_COUNT);
    return EDEN_DSMOD_TRUE;
}

static const EdenDsmodFontExtensions extension = {
    EDEN_DSMOD_FONT_EXT_VERSION, sizeof(EdenDsmodFontExtensions), EDEN_DSMOD_FONT_EXT_HASH, duo_font_decode
};
#if defined(__GNUC__)
__attribute__((visibility("default")))
#endif
const EdenDsmodFontExtensions *eden_dsmod_get_font_extensions(uint32_t version, uint64_t hash) {
    return version == EDEN_DSMOD_FONT_EXT_VERSION && hash == EDEN_DSMOD_FONT_EXT_HASH ? &extension : NULL;
}
