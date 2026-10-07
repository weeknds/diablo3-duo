// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"

#define MAP_MAX_TILES 64u
#define MAP_MAX_NODES 128u
#define MAP_TILE_BYTES 256u
#define MAP_MAX_READS 600u
#define MAP_MAX_BYTES 60000u
#define MAP_CONTEXT_MAX_READS 64u
#define MAP_CONTEXT_MAX_BYTES 1024u

/* Actual exploration masks. These describe revealed areas, not walkable walls. */
typedef struct MapExplorationTile {
    uint32_t scene_id;
    float min_x, min_y, max_x, max_y;
    uint16_t columns, rows;
    uint8_t fully_revealed;
    /* Two bits per cell, column-major, highest pair first in each byte. */
    uint8_t cells[MAP_TILE_BYTES];
} MapExplorationTile;

typedef struct MapProbeResult {
    int available, shared_identity_valid, exploration_available;
    uint32_t world_id;
    float x, y, z;
    uint32_t tile_count;
    MapExplorationTile tiles[MAP_MAX_TILES];
    uint32_t reads, bytes;
    const char *reason, *exploration_reason;
} MapProbeResult;

void map_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
               MapProbeResult *result);
/* Small bracketed presentation-position read; no exploration-list or mask scan.
 * On failure outputs are UINT32_MAX, 0, 0; all three outputs are required. */
int map_current_position(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
                         uint32_t *world_id, float *x, float *y);
/* Compatibility wrapper using the same position/identity validation. */
int map_current_world(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
                      uint32_t *world_id);
/* 0 means unexplored; 1/2 are the native mask's revealed / visited shades. */
unsigned map_cell_visibility(const MapExplorationTile *tile, unsigned column,
                             unsigned row);
