// SPDX-License-Identifier: GPL-3.0-or-later
// Independently implemented from the current executable's documented factual route.
// Exact-build evidence: docs/character-fields-research.md. Read-only candidate.
#include "probe_internal.h"
#include <limits.h>
#include <string.h>

void player_probe(const EdenDsmodHostApi *host, PlayerProbeResult *result) {
    *result = (PlayerProbeResult){.reason = "UNAVAILABLE: invalid host/main region"};
    if (!host || !host->userdata || !host->read_memory || !host->is_mapped ||
        !host->main_base || host->main_base % 8 || host->main_size < ROOT_OFFSET + 8 ||
        host->main_base > UINT64_MAX - host->main_size) return;
    Reader reader = {host, result, PLAYER_MAX_READS, PLAYER_MAX_BYTES, NULL}; Snapshot before, after;
    if (!snapshot(&reader, &before)) return;
    struct { int32_t hero_class, level, paragon; } values = {0}, repeated = {0};
    _Static_assert(sizeof values == 12, "Character observation must be twelve bytes");
    result->reason = "UNAVAILABLE: character block range/read";
    if (!read_offset(&reader, before.player, 0xD688, &values, sizeof values, 4) ||
        values.level < 1 || values.level > 70 ||
        !read_offset(&reader, before.player, 0xD688, &repeated, sizeof repeated, 4)) return;
    if (!snapshot(&reader, &after)) return;
    if (memcmp(&before, &after, sizeof before) || memcmp(&values, &repeated, sizeof values)) {
        result->reason = "UNAVAILABLE: character or snapshot identity changed";
        return;
    }
    result->available = 1; result->level = values.level; result->identity = before;
    if (values.hero_class >= 0 && values.hero_class <= 6) {
        result->hero_class_available = 1; result->hero_class = values.hero_class;
    }
    // The exact build's Alt_Level registration sets its upper limit to 20000.
    if (values.paragon >= 0 && values.paragon <= 20000) {
        result->paragon_available = 1; result->paragon = values.paragon;
    }
    result->reason = "UNVERIFIED RESEARCH: candidate character identity";
}
