// SPDX-License-Identifier: GPL-3.0-or-later
#include "markers_probe.h"
#include "probe_internal.h"
#include <float.h>
#include <math.h>
#include <stddef.h>

/* Exact-build facts: 0x3EA5B0 renders the two minimap marker hash tables;
 * 0x3E92A0 renders the separate map-marker list. 0x3EE870/0x2CE320 write
 * their records. 0x4DA2C0/0x4DFF34 register the named texture settings used
 * below, and 0x3E8510 compares destination texture SNOs with those settings.
 * Resource names/IDs are read from the game; no guest calls or writes occur.
 * Unknown icon types and quest-distance exceptions are deliberately omitted. */
#define MINIMAP_ROOT UINT64_C(0x1878FE8)
#define MARKER_ROOT UINT64_C(0x185B590)
#define QUEST_OPTIONS_READY UINT64_C(0x181AAC4)
#define QUEST_OPTIONS_VISIBLE UINT64_C(0x181BD80)
#define MAX_NODES 128u
#define MAX_BUCKETS 256u
#define MAX_COORDINATE 1000000.0f
#define MAX_DISTANCE_SQUARED 4000000000000.0f

typedef struct Context {
    Snapshot player;
    uint64_t acd_globals, pool, pages, page, acd, visible_world;
    uint32_t capacity, shift, acd_id, world, visible_world_id;
} Context;

typedef struct Settings {
    uint64_t vector, data, length, capacity;
    uint32_t entries[4][4];
    uint32_t quest_visible;
    uint8_t initialized;
} Settings;

typedef struct Table {
    uint32_t mask, count;
    uint64_t pool, buckets;
    uint32_t bucket_count, padding;
} Table;

typedef struct ObjectiveNode {
    uint64_t next;
    uint32_t id;
    float x, y, z;
    uint32_t world, texture, atlas, unknown24, unknown28, actor_sno, quest_sno;
    uint32_t arrow, explored_only, unknown3c, special_arrow;
    float max_distance_squared, min_distance_squared, reveal_distance_squared;
    uint32_t player_mask;
    float quest_timing;
} ObjectiveNode;

typedef struct MarkerList { uint64_t first, last; uint32_t count, padding; } MarkerList;
typedef struct MarkerNode {
    uint32_t id, actor_sno;
    float x, y, z;
    uint32_t world, unknown18, unknown1c;
    float unknown20;
    uint32_t type, flags, texture, atlas, unknown34, unknown38;
    float min_distance_squared, max_distance_squared;
    uint32_t unknown44[11];
    uint64_t previous, next;
} MarkerNode;

_Static_assert(sizeof(ObjectiveNode) == 0x58, "Objective marker layout");
_Static_assert(offsetof(ObjectiveNode, max_distance_squared) == 0x44, "Objective distance");
_Static_assert(sizeof(MarkerNode) == 0x80, "Map marker layout");
_Static_assert(offsetof(MarkerNode, previous) == 0x70, "Map marker links");

static int coordinate(float v) { return isfinite(v) && fabsf(v) <= MAX_COORDINATE; }
static int distance_value(float v) {
    return isfinite(v) && v >= 0 && v <= MAX_DISTANCE_SQUARED;
}

static int context(Reader *r, const PlayerProbeIdentity *expected, Context *out) {
    memset(out, 0, sizeof *out);
    if (!snapshot(r, &out->player) || memcmp(&out->player, expected, sizeof *expected)) return 0;
    const uint32_t low = expected->acd_id & 0xffffu;
    return pointer(r, expected->client, 0x9A0, &out->acd_globals) &&
        pointer(r, out->acd_globals, 0, &out->pool) &&
        read_offset(r, out->pool, 0x100, &out->capacity, 4, 4) && low < out->capacity &&
        read_offset(r, out->pool, 0x168, &out->shift, 4, 4) && out->shift <= 16 &&
        pointer(r, out->pool, 0x120, &out->pages) &&
        pointer(r, out->pages, (uint64_t)(low >> out->shift) * 8, &out->page) &&
        add(out->page, (uint64_t)(low & ((UINT32_C(1) << out->shift) - 1)) * 0x360, &out->acd) &&
        read_at(r, out->acd, &out->acd_id, 4, 4) && out->acd_id == expected->acd_id &&
        read_offset(r, out->acd, 0xA0, &out->world, 4, 4) && out->world != UINT32_MAX &&
        pointer(r, expected->client, 0x800, &out->visible_world) &&
        read_offset(r, out->visible_world, 0x38, &out->visible_world_id, 4, 4) &&
        out->world == out->visible_world_id;
}

static int settings(Reader *r, const Context *ctx, Settings *out) {
    static const uint32_t indices[4] = {0x267, 0x268, 0x541, 0x543};
    memset(out, 0, sizeof *out);
    if (!pointer(r, ctx->player.globals, 0x1100, &out->vector) ||
        !pointer(r, out->vector, 0, &out->data) ||
        !read_offset(r, out->vector, 8, &out->length, 8, 8) ||
        !read_offset(r, out->vector, 0x10, &out->capacity, 8, 8) ||
        out->length <= 0x543 || out->length > 8192 ||
        (out->capacity & UINT64_C(0x7FFFFFFFFFFFFFFF)) < out->length ||
        !read_offset(r, r->host->main_base, QUEST_OPTIONS_READY, &out->initialized, 1, 1) ||
        !read_offset(r, r->host->main_base, QUEST_OPTIONS_VISIBLE, &out->quest_visible, 4, 4)) return 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (!read_offset(r, out->data, (uint64_t)indices[i] * 16, out->entries[i], 16, 4) ||
            out->entries[i][0] != 2 || out->entries[i][1] != 44) return 0;
    }
    return 1;
}

static MarkerKind classify(const Settings *s, uint32_t texture) {
    static const MarkerKind kinds[4] = {MARKER_PORTAL, MARKER_PORTAL, MARKER_QUEST, MARKER_WAYPOINT};
    MarkerKind result = MARKER_NONE;
    if (texture == UINT32_MAX) return result;
    for (unsigned i = 0; i < 4; ++i) {
        if (texture != s->entries[i][2]) continue;
        if (result && result != kinds[i]) return MARKER_NONE;
        result = kinds[i];
    }
    if (result == MARKER_QUEST && (!(s->initialized & 1u) || !s->quest_visible)) return MARKER_NONE;
    return result;
}

static int explored(const MapProbeResult *map, float x, float y) {
    for (uint32_t i = 0; i < map->tile_count; ++i) {
        const MapExplorationTile *t = &map->tiles[i];
        if (!coordinate(t->min_x) || !coordinate(t->min_y) || !coordinate(t->max_x) ||
            !coordinate(t->max_y) || t->max_x <= t->min_x || t->max_y <= t->min_y ||
            !t->columns || t->columns > 32 || !t->rows || t->rows > 32 ||
            t->fully_revealed > 1) continue;
        if (x < t->min_x || x >= t->max_x || y < t->min_y || y >= t->max_y) continue;
        const unsigned column = (unsigned)((x - t->min_x) / (t->max_x - t->min_x) * t->columns);
        const unsigned row = (unsigned)((y - t->min_y) / (t->max_y - t->min_y) * t->rows);
        if (map_cell_visibility(t, column, row)) return 1;
    }
    return 0;
}

static int append(MarkersProbeResult *out, uint32_t id, MarkerKind kind, float x, float y) {
    for (uint32_t i = 0; i < out->count; ++i) {
        const Marker *m = &out->items[i];
        if (m->id == id && m->kind == kind && m->x == x && m->y == y) return 1;
    }
    if (out->count == MARKERS_MAX_ITEMS) return 0;
    out->items[out->count++] = (Marker){id, kind, x, y};
    return 1;
}

static uint32_t bucket_hash(uint32_t key) {
    uint32_t hash = UINT32_C(0x811C9DC5);
    for (unsigned i = 0; i < 4; ++i) hash = (hash ^ ((key >> (8 * i)) & 255u)) * UINT32_C(0x01000193);
    return hash;
}

static int table_header(Reader *r, uint64_t address, Table *out) {
    memset(out, 0, sizeof *out);
    if (!read_at(r, address, out, sizeof *out, 8)) return 0;
    /* Padding is not ownership state and need not be stable. */
    out->padding = 0;
    return out->count <= MAX_NODES && out->bucket_count && out->bucket_count <= MAX_BUCKETS &&
        !(out->bucket_count & (out->bucket_count - 1)) && out->mask == out->bucket_count - 1 &&
        out->buckets && !(out->buckets % 8);
}

static int objectives(Reader *r, uint64_t manager, uint64_t offset, const Settings *icons,
                      const MapProbeResult *map, const float *player_xy, MarkersProbeResult *out) {
    uint64_t table, after_table;
    Table first, last;
    uint64_t buckets[MAX_BUCKETS], after_buckets[MAX_BUCKETS], addresses[MAX_NODES];
    ObjectiveNode nodes[MAX_NODES];
    if (!pointer(r, manager, offset, &table) || !table_header(r, table, &first) ||
        !read_at(r, first.buckets, buckets, first.bucket_count * 8, 8)) return 0;
    uint32_t total = 0;
    for (uint32_t b = 0; b < first.bucket_count; ++b) {
        uint64_t address = buckets[b];
        while (address) {
            if (total == first.count) return 0;
            for (uint32_t j = 0; j < total; ++j) if (addresses[j] == address) return 0;
            addresses[total] = address;
            ObjectiveNode *node = &nodes[total++];
            if (!read_at(r, address, node, sizeof *node, 8) ||
                (bucket_hash(node->id) & first.mask) != b) return 0;
            address = node->next;
            if (node->world != map->world_id) continue;
            const MarkerKind kind = classify(icons, node->texture);
            if (!kind) continue;
            if (!coordinate(node->x) || !coordinate(node->y) ||
                !distance_value(node->min_distance_squared) || !distance_value(node->max_distance_squared)) return 0;
            const float dx = player_xy[0] - node->x, dy = player_xy[1] - node->y;
            const float distance = dx * dx + dy * dy;
            /* Far quest markers can have additional quest/timing exceptions.
             * Omit that branch instead of assuming the quest is active. */
            if ((node->max_distance_squared > FLT_EPSILON && distance > node->max_distance_squared) ||
                (node->min_distance_squared > FLT_EPSILON && distance < node->min_distance_squared) ||
                !explored(map, node->x, node->y)) continue;
            if (!append(out, node->id, kind, node->x, node->y)) return 0;
        }
    }
    if (total != first.count) return 0;
    for (uint32_t i = 0; i < total; ++i) {
        ObjectiveNode node;
        if (!read_at(r, addresses[i], &node, sizeof node, 8) || memcmp(&node, &nodes[i], sizeof node)) return 0;
    }
    return pointer(r, manager, offset, &after_table) && after_table == table &&
        table_header(r, table, &last) && !memcmp(&first, &last, sizeof first) &&
        read_at(r, first.buckets, after_buckets, first.bucket_count * 8, 8) &&
        !memcmp(buckets, after_buckets, first.bucket_count * 8);
}

/* Native generic markers defer to active actors. Check both direct and mapped
 * handles, conservatively excluding either form without assuming game mode. */
static int active_acd(Reader *r, const Context *ctx, uint32_t id, int *active) {
    *active = 0;
    const uint32_t low = id & 0xffffu;
    if (id == UINT32_MAX || low >= ctx->capacity) return 1;
    uint64_t page, actor;
    uint32_t observed;
    if (!pointer(r, ctx->pages, (uint64_t)(low >> ctx->shift) * 8, &page) ||
        !add(page, (uint64_t)(low & ((UINT32_C(1) << ctx->shift) - 1)) * 0x360, &actor) ||
        !read_at(r, actor, &observed, 4, 4)) return 0;
    *active = observed == id;
    return 1;
}

static int active_marker(Reader *r, const Context *ctx, uint32_t id, int *active) {
    if (!active_acd(r, ctx, id, active) || *active || id == UINT32_MAX) return *active || id == UINT32_MAX;
    if (((id >> 2) & 0x3fffu) > 0xee6u) return 1;
    uint64_t mapping;
    uint32_t mapped_id, after;
    if (!pointer(r, ctx->acd_globals, 8, &mapping) ||
        !read_offset(r, mapping, (uint64_t)(id & 0xffffu) * 4, &mapped_id, 4, 4) ||
        !active_acd(r, ctx, mapped_id, active) ||
        !read_offset(r, mapping, (uint64_t)(id & 0xffffu) * 4, &after, 4, 4) || after != mapped_id) return 0;
    uint64_t after_mapping;
    return pointer(r, ctx->acd_globals, 8, &after_mapping) && after_mapping == mapping;
}

static int marker_list(Reader *r, uint64_t address, MarkerList *out) {
    memset(out, 0, sizeof *out);
    return read_offset(r, address, 0, &out->first, 8, 8) &&
        read_offset(r, address, 8, &out->last, 8, 8) &&
        read_offset(r, address, 0x10, &out->count, 4, 4) && out->count <= MAX_NODES &&
        ((out->count == 0) == (out->first == 0)) && ((out->count == 0) == (out->last == 0));
}

static int generic_markers(Reader *r, const Context *ctx, const Settings *icons,
                           const MapProbeResult *map, const float *player_xy, MarkersProbeResult *out) {
    uint64_t owner, list, after_owner, after_list, addresses[MAX_NODES];
    uint32_t player_markers[4], player_markers_after[4];
    MarkerList first, last;
    MarkerNode nodes[MAX_NODES];
    if (!pointer(r, r->host->main_base, MARKER_ROOT, &owner) || !pointer(r, owner, 8, &list) ||
        !marker_list(r, list, &first)) return 0;
    for (unsigned i = 0; i < 4; ++i)
        if (!read_offset(r, ctx->player.players, (uint64_t)i * 0xEBF8 + 0xE7D0, &player_markers[i], 4, 4)) return 0;
    uint64_t address = first.first, previous = 0;
    for (uint32_t i = 0; i < first.count; ++i) {
        for (uint32_t j = 0; j < i; ++j) if (addresses[j] == address) return 0;
        addresses[i] = address;
        MarkerNode *node = &nodes[i];
        if (!read_at(r, address, node, sizeof *node, 8) || node->previous != previous) return 0;
        previous = address; address = node->next;
        if (node->world != map->world_id || (node->type >= 1 && node->type <= 7) ||
            node->type == 9 || (node->flags & 0xeu)) continue;
        /* Native special-mode texture substitutions are not interpreted. */
        if (node->actor_sno != UINT32_MAX && node->type >= 12 && node->type <= 14) continue;
        const MarkerKind kind = classify(icons, node->texture);
        if (!kind) continue;
        int player = 0;
        for (unsigned p = 0; p < 4; ++p) if (node->id == player_markers[p]) player = 1;
        if (player) continue;
        if (!coordinate(node->x) || !coordinate(node->y) ||
            !distance_value(node->min_distance_squared) || !distance_value(node->max_distance_squared)) return 0;
        const float dx = player_xy[0] - node->x, dy = player_xy[1] - node->y;
        const float distance = dx * dx + dy * dy;
        if ((node->min_distance_squared > FLT_EPSILON && distance < node->min_distance_squared) ||
            (node->max_distance_squared > FLT_EPSILON && distance > node->max_distance_squared) ||
            !explored(map, node->x, node->y)) continue;
        int active;
        if (!active_marker(r, ctx, node->id, &active)) return 0;
        if (!active && !append(out, node->id, kind, node->x, node->y)) return 0;
    }
    if (address || previous != first.last) return 0;
    for (uint32_t i = 0; i < first.count; ++i) {
        MarkerNode node;
        if (!read_at(r, addresses[i], &node, sizeof node, 8) || memcmp(&node, &nodes[i], sizeof node)) return 0;
    }
    for (unsigned i = 0; i < 4; ++i)
        if (!read_offset(r, ctx->player.players, (uint64_t)i * 0xEBF8 + 0xE7D0, &player_markers_after[i], 4, 4)) return 0;
    return !memcmp(player_markers, player_markers_after, sizeof player_markers) &&
        pointer(r, r->host->main_base, MARKER_ROOT, &after_owner) && after_owner == owner &&
        pointer(r, owner, 8, &after_list) && after_list == list && marker_list(r, list, &last) &&
        !memcmp(&first, &last, sizeof first);
}

void markers_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
                   const MapProbeResult *map, MarkersProbeResult *result) {
    if (!result) return;
    *result = (MarkersProbeResult){.reason = "UNAVAILABLE: marker context required"};
    if (!expected || !map || !map->available || !map->shared_identity_valid || !map->exploration_available ||
        map->world_id == UINT32_MAX || map->tile_count > MAP_MAX_TILES ||
        !host || !host->userdata || !host->read_memory || !host->is_mapped ||
        !host->main_base || host->main_base % 8 || host->main_size < MINIMAP_ROOT + 8 ||
        host->main_base > UINT64_MAX - host->main_size) return;
    PlayerProbeResult work = {0};
    Reader reader = {host, &work, MARKERS_MAX_READS - 64, MARKERS_MAX_BYTES - 1024, NULL};
    Context first, last;
    Settings icons, after_icons;
    uint64_t manager = 0, after_manager = 0;
    uint32_t flags = 0, after_flags = 0;
    float player_xy[2];
    const int initial = context(&reader, expected, &first) && first.world == map->world_id;
    int valid = initial && settings(&reader, &first, &icons) &&
        pointer(&reader, host->main_base, MINIMAP_ROOT, &manager) &&
        read_offset(&reader, manager, 0x98, &flags, 4, 4) &&
        read_offset(&reader, first.acd, 0x60, player_xy, sizeof player_xy, 4) &&
        coordinate(player_xy[0]) && coordinate(player_xy[1]) &&
        objectives(&reader, manager, 8, &icons, map, player_xy, result) &&
        objectives(&reader, manager, 0x10, &icons, map, player_xy, result) &&
        ((flags & 4u) || generic_markers(&reader, &first, &icons, map, player_xy, result)) &&
        settings(&reader, &first, &after_icons) && !memcmp(&icons, &after_icons, sizeof icons) &&
        pointer(&reader, host->main_base, MINIMAP_ROOT, &after_manager) && manager == after_manager &&
        read_offset(&reader, manager, 0x98, &after_flags, 4, 4) && flags == after_flags;
    reader.max_reads = MARKERS_MAX_READS; reader.max_bytes = MARKERS_MAX_BYTES;
    const int final_context = context(&reader, expected, &last);
    const int same = initial && final_context && !memcmp(&first, &last, sizeof first);
    valid = valid && same;
    result->reads = work.reads; result->bytes = work.bytes;
    result->shared_identity_valid = same;
    if (!valid) {
        result->count = 0; memset(result->items, 0, sizeof result->items);
        result->reason = "UNAVAILABLE: markers changed, unreadable, or outside reader limits";
        return;
    }
    result->available = 1; result->world_id = map->world_id;
    result->reason = "UNVERIFIED RESEARCH: explored game quest and location markers";
}
