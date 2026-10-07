// SPDX-License-Identifier: GPL-3.0-or-later
#include "map_probe.h"
#include "probe_internal.h"
#include <math.h>
#include <stddef.h>

/* Exact-build executable facts (2607A74F5DF7754C...):
 * ActorGetWorld at 0x923F60 calls the ACD accessor at 0x477EA0:
 * ACD+0x60 is XYZ; ACD+0xA0 is its world instance ID.
 * 0x41820 supplies the visible world's ID from client+0x800 -> +0x38.
 * The minimap renderer at 0x3EAC20 traverses main+0x1878FE8 -> +0x20,
 * matching node+8 to that world. Node+0x10 contains XY rectangle bounds.
 * 0x3EB360 allocates at most 32 by 32 two-bit cells; 0x3ED2D0 maps
 * world X to node+0x34, world Y to +0x30. 0x3EF0E0 writes the packed
 * column-major cells. 0x3EB124 renders their native visibility shades.
 * These are exploration masks, not navigation meshes or room geometry.
 * No guest function is called and no guest data is retained between calls.
 */
#define MAP_ROOT_OFFSET UINT64_C(0x1878FE8)
#define MAX_COORDINATE 1000000.0f

typedef struct MapIdentity {
    Snapshot player;
    uint64_t acd_globals, pool, pages, page, acd, visible_world;
    uint32_t capacity, shift, acd_identity, world_id, visible_world_id;
} MapIdentity;

typedef struct MapNode {
    uint32_t identity, scene_id, world_id, scene_sno;
    float bounds[4];
    uint32_t texture, padding;
    uint64_t cells;
    uint32_t rows, columns, last_used, fully_revealed;
    uint64_t unknown, previous, next;
} MapNode;
_Static_assert(sizeof(MapNode) == 0x58, "Minimap node layout");
_Static_assert(offsetof(MapNode, next) == 0x50, "Minimap next link");

typedef struct MapList {
    uint64_t first, last;
    uint32_t count;
} MapList;

static int common(Reader *r, const PlayerProbeIdentity *expected, MapIdentity *s) {
    memset(s, 0, sizeof *s);
    if (!snapshot(r, &s->player) || memcmp(&s->player, expected, sizeof *expected)) return 0;
    const uint32_t low = expected->acd_id & 0xFFFFu;
    return pointer(r, expected->client, 0x9A0, &s->acd_globals) &&
        pointer(r, s->acd_globals, 0, &s->pool) &&
        read_offset(r, s->pool, 0x100, &s->capacity, 4, 4) && low < s->capacity &&
        read_offset(r, s->pool, 0x168, &s->shift, 4, 4) && s->shift <= 16 &&
        pointer(r, s->pool, 0x120, &s->pages) &&
        pointer(r, s->pages, (uint64_t)(low >> s->shift) * 8, &s->page) &&
        add(s->page, (uint64_t)(low & ((UINT32_C(1) << s->shift) - 1)) * 0x360, &s->acd) &&
        read_at(r, s->acd, &s->acd_identity, 4, 4) && s->acd_identity == expected->acd_id &&
        read_offset(r, s->acd, 0xA0, &s->world_id, 4, 4) && s->world_id != UINT32_MAX &&
        pointer(r, expected->client, 0x800, &s->visible_world) &&
        read_offset(r, s->visible_world, 0x38, &s->visible_world_id, 4, 4) &&
        s->visible_world_id == s->world_id;
}

static int coordinate(float v) { return isfinite(v) && fabsf(v) <= MAX_COORDINATE; }

static int node_same(const MapNode *a, const MapNode *b) {
    /* Texture handles and last-used ticks can change without changing ownership. */
    return a->identity == b->identity && a->scene_id == b->scene_id &&
        a->world_id == b->world_id && a->scene_sno == b->scene_sno &&
        !memcmp(a->bounds, b->bounds, sizeof a->bounds) && a->cells == b->cells &&
        a->rows == b->rows && a->columns == b->columns &&
        a->fully_revealed == b->fully_revealed &&
        a->previous == b->previous && a->next == b->next;
}

static int list_read(Reader *r, uint64_t manager, MapList *list) {
    memset(list, 0, sizeof *list);
    return read_offset(r, manager, 0x20, &list->first, 8, 8) &&
        read_offset(r, manager, 0x28, &list->last, 8, 8) &&
        read_offset(r, manager, 0x30, &list->count, 4, 4) &&
        list->count <= MAP_MAX_NODES &&
        ((list->count == 0) == (list->first == 0)) &&
        ((list->count == 0) == (list->last == 0));
}

static int exploration(Reader *r, uint32_t world, MapProbeResult *out) {
    uint64_t manager = 0, manager_after = 0;
    MapList list, after;
    MapNode nodes[MAP_MAX_NODES];
    uint64_t addresses[MAP_MAX_NODES];
    uint16_t tile_indices[MAP_MAX_NODES];
    if (r->host->main_size < MAP_ROOT_OFFSET + 8 ||
        !pointer(r, r->host->main_base, MAP_ROOT_OFFSET, &manager) ||
        !list_read(r, manager, &list)) return 0;
    uint64_t address = list.first, previous = 0;
    uint32_t tiles = 0;
    for (uint32_t i = 0; i < list.count; ++i) {
        for (uint32_t j = 0; j < i; ++j) if (addresses[j] == address) return 0;
        addresses[i] = address;
        tile_indices[i] = UINT16_MAX;
        MapNode *node = &nodes[i];
        if (!read_at(r, address, node, sizeof *node, 8) || node->previous != previous) return 0;
        previous = address;
        address = node->next;
        if (node->world_id != world || node->scene_sno == UINT32_MAX) continue;
        if (tiles == MAP_MAX_TILES || node->fully_revealed > 1) return 0;
        for (unsigned n = 0; n < 4; ++n) if (!coordinate(node->bounds[n])) return 0;
        if (node->bounds[2] <= node->bounds[0] || node->bounds[3] <= node->bounds[1]) return 0;
        MapExplorationTile *tile = &out->tiles[tiles];
        tile->scene_id = node->scene_id;
        tile->min_x = node->bounds[0]; tile->min_y = node->bounds[1];
        tile->max_x = node->bounds[2]; tile->max_y = node->bounds[3];
        tile->fully_revealed = (uint8_t)node->fully_revealed;
        if (node->fully_revealed) {
            tile->columns = tile->rows = 1;
        } else {
            if (!node->columns || node->columns > 32 || !node->rows || node->rows > 32 ||
                !node->cells) return 0;
            tile->columns = (uint16_t)node->columns; tile->rows = (uint16_t)node->rows;
            const uint32_t bytes = (node->columns * node->rows + 3u) / 4u;
            if (!read_at(r, node->cells, tile->cells, bytes, 1)) return 0;
        }
        tile_indices[i] = (uint16_t)tiles++;
    }
    if (address || previous != list.last) return 0;
    /* Reobserve every link and retained mask before publishing. A failure cannot
     * leave a half-filled map visible. The caller still rechecks the player. */
    for (uint32_t i = 0; i < list.count; ++i) {
        MapNode node;
        if (!read_at(r, addresses[i], &node, sizeof node, 8) || !node_same(&nodes[i], &node)) return 0;
        if (tile_indices[i] == UINT16_MAX || node.fully_revealed) continue;
        uint8_t cells[MAP_TILE_BYTES];
        const uint32_t bytes = (node.columns * node.rows + 3u) / 4u;
        if (!read_at(r, node.cells, cells, bytes, 1) ||
            memcmp(cells, out->tiles[tile_indices[i]].cells, bytes)) return 0;
    }
    if (!pointer(r, r->host->main_base, MAP_ROOT_OFFSET, &manager_after) || manager != manager_after ||
        !list_read(r, manager_after, &after) || memcmp(&list, &after, sizeof list)) return 0;
    out->tile_count = tiles;
    return 1;
}

void map_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
               MapProbeResult *result) {
    if (!result) return;
    *result = (MapProbeResult){.reason = "UNAVAILABLE: invalid map host/identity",
        .exploration_reason = "UNAVAILABLE: exploration has not been read"};
    if (!expected || !host || !host->userdata || !host->read_memory || !host->is_mapped ||
        !host->main_base || host->main_base % 8 || host->main_size < ROOT_OFFSET + 8 ||
        host->main_base > UINT64_MAX - host->main_size) return;
    PlayerProbeResult work = {0};
    Reader reader = {host, &work, MAP_MAX_READS, MAP_MAX_BYTES, NULL};
    MapIdentity first, last;
    float xyz[3] = {0};
    const int first_valid = common(&reader, expected, &first);
    int explored = 0, position = 0;
    if (first_valid) {
        /* Reserve ample reads for final identity even if map traversal hits a limit. */
        reader.max_reads -= 64; reader.max_bytes -= 512;
        explored = exploration(&reader, first.world_id, result);
        reader.max_reads = MAP_MAX_READS; reader.max_bytes = MAP_MAX_BYTES;
        position = read_offset(&reader, first.acd, 0x60, xyz, sizeof xyz, 4);
    }
    const int last_valid = common(&reader, expected, &last);
    result->reads = work.reads; result->bytes = work.bytes;
    if (!first_valid || !last_valid || memcmp(&first, &last, sizeof first)) {
        const uint32_t reads = result->reads, bytes = result->bytes;
        *result = (MapProbeResult){.reads = reads, .bytes = bytes,
            .reason = "UNAVAILABLE: player/world identity changed or unreadable",
            .exploration_reason = "UNAVAILABLE: player/world identity required"};
        return;
    }
    result->shared_identity_valid = 1;
    if (!position || !coordinate(xyz[0]) || !coordinate(xyz[1]) || !coordinate(xyz[2])) {
        memset(result->tiles, 0, sizeof result->tiles); result->tile_count = 0;
        result->reason = "UNAVAILABLE: position invalid or unreadable";
        result->exploration_reason = "UNAVAILABLE: valid position required";
        return;
    }
    result->available = 1; result->world_id = first.world_id;
    result->x = xyz[0]; result->y = xyz[1]; result->z = xyz[2];
    result->reason = "UNVERIFIED RESEARCH: exact-build world position";
    result->exploration_available = explored;
    result->exploration_reason = explored ? "UNVERIFIED RESEARCH: native exploration coverage" :
        "UNAVAILABLE: exploration changed, unreadable, or exceeds the bounded reader";
    if (!explored) { result->tile_count = 0; memset(result->tiles, 0, sizeof result->tiles); }
}

int map_current_world(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
                      uint32_t *world_id) {
    if (!world_id) return 0;
    *world_id=UINT32_MAX;
    if (!expected || !host || !host->userdata || !host->read_memory || !host->is_mapped ||
        !host->main_base || host->main_base%8 || host->main_size<ROOT_OFFSET+8 ||
        host->main_base>UINT64_MAX-host->main_size) return 0;
    PlayerProbeResult work={0};
    Reader reader={host,&work,MAP_CONTEXT_MAX_READS,MAP_CONTEXT_MAX_BYTES,NULL};
    MapIdentity first,last;
    const int first_valid=common(&reader,expected,&first);
    const int last_valid=common(&reader,expected,&last);
    if (!first_valid || !last_valid || memcmp(&first,&last,sizeof first)) return 0;
    *world_id=first.world_id;
    return 1;
}

unsigned map_cell_visibility(const MapExplorationTile *tile, unsigned column, unsigned row) {
    if (!tile || !tile->columns || !tile->rows || tile->columns > 32 || tile->rows > 32 ||
        column >= tile->columns || row >= tile->rows) return 0;
    if (tile->fully_revealed == 1) return 2;
    const unsigned index = column * tile->rows + row;
    const unsigned state = (tile->cells[index / 4] >> (6 - 2 * (index % 4))) & 3u;
    return (state & 1u) ? 2u : (state & 2u ? 1u : 0u);
}
