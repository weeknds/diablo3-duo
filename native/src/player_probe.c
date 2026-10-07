// SPDX-License-Identifier: GPL-3.0-or-later
// Independently implemented from the current executable's documented factual route.
// Evidence: ../research-analysis/player-level-candidate.txt. Private candidate only.
#include "probe_internal.h"
#include <limits.h>
#include <string.h>

#define MAX_READS 27u
#define MAX_BYTES 212u


void player_probe(const EdenDsmodHostApi *host, PlayerProbeResult *result) {
    *result = (PlayerProbeResult){.reason = "UNAVAILABLE: invalid host/main region"};
    if (!host || !host->userdata || !host->read_memory || !host->is_mapped ||
        !host->main_base || host->main_base % 8 || host->main_size < ROOT_OFFSET + 8 ||
        host->main_base > UINT64_MAX - host->main_size) return;
    Reader reader = {host, result, MAX_READS, MAX_BYTES, NULL}; Snapshot before, after;
    if (!snapshot(&reader, &before)) return;
    int32_t level = 0;
    result->reason = "UNAVAILABLE: candidate level range/read";
    if (!read_offset(&reader, before.player, 0xD68C, &level, 4, 4) || level < 1 || level > 70) return;
    if (!snapshot(&reader, &after)) return;
    if (memcmp(&before, &after, sizeof before)) {
        result->reason = "UNAVAILABLE: snapshot identity changed";
        return;
    }
    result->available = 1; result->level = level; result->identity = before;
    result->reason = "UNVERIFIED RESEARCH: candidate character level";
}
