// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dsmod_module_abi.h"
typedef struct PlayerProbeIdentity {
    uint64_t globals, client, selector, players, player, pool, pages, page, actor;
    int32_t game_type, slots[4], count, player_index;
    uint32_t acd_id, actor_id, capacity, shift, actor_identity;
} PlayerProbeIdentity;
typedef struct PlayerProbeResult { int available; int32_t level; uint32_t reads, bytes; const char *reason; PlayerProbeIdentity identity; } PlayerProbeResult;
void player_probe(const EdenDsmodHostApi *host, PlayerProbeResult *result);
