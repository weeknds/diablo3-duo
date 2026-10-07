// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"

#define NAV_MAX_GRIDS 64u
#define NAV_MAX_SCENE_SLOTS 1024u
#define NAV_MAX_CELLS 65536u
#define NAV_CELL_BYTES (NAV_MAX_CELLS / 8u)
#define NAV_MAX_READS 8192u
#define NAV_MAX_BYTES 700000u

/* A complete terrain grid for one supported leaf scene. Coordinates are world
 * XY, before any map projection. Corner(column,row) = origin + column*axis_x
 * + row*axis_y. Cells are row-major, one bit per cell, least-significant first.
 * The reader does not apply exploration fog. A caller must do so before display.
 * This is static terrain navigation, not dynamic actor/door collision. */
typedef struct NavTerrainGrid {
    uint32_t scene_id;
    uint16_t columns, rows;
    float origin_x, origin_y;
    float axis_x_x, axis_x_y, axis_y_x, axis_y_y;
    uint32_t cell_offset;
} NavTerrainGrid;

typedef struct NavProbeResult {
    int available, shared_identity_valid, partial;
    uint32_t world_id, grid_count, unavailable_scene_count, cell_count;
    NavTerrainGrid grids[NAV_MAX_GRIDS];
    uint8_t ground[NAV_CELL_BYTES];
    uint32_t reads, bytes;
    const char *reason;
} NavProbeResult;

/* The module must gate the exact supported executable before calling. This
 * function performs no guest calls or writes and retains no guest pointers. */
void nav_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
               uint32_t expected_world_id, NavProbeResult *result);
int nav_cell_ground(const NavProbeResult *result, unsigned grid,
                    unsigned column, unsigned row);
