// SPDX-License-Identifier: GPL-3.0-or-later
#include "nav_probe.h"
#include "probe_internal.h"
#include <math.h>
#include <stddef.h>

/* Exact-build evidence, main 2607A74F5DF7754C...:
 * 0x1345F0: client+0xB18 flat 0x7D8 scene pool, full handle at record+0.
 * 0x131B34: pool highest-used uint16 +0x108, active count +0x10C.
 * 0x6631E4: generic scene (record+8) owns ID/world/SNO and transform.
 * 0x70DA50: scene+0x188 points to the resource's copied 16-byte grid header,
 *   followed by the owning generic scene ID. Scene SNO is scene+0xE4.
 * 0x6C48D0/0x6C6EB8: resident SNO table and 18-bit generational pool handles.
 * 0x6C5CFC/0x6C5D18 publish a resource before load completion. 0x6C6DA0
 *   sets resource+8 bit 0x40 only after loading/postprocessing; 0x6C4B54
 *   marks a fallback resource with bit 0x2. Neither is checked by the fast
 *   resident lookup. Resource+0 is its SNO (also used by eviction 0x6C61BC).
 * 0x70FB40: child overrides, crop bounds/default flags, full uint16 flags and
 *   compressed nibbles. Bit zero is preserved by both representations.
 * 0x82E5F0/0x82E6D0: paired over-walkable/not-over-walkable AI conditions
 *   call 0x70EBD0 with bit zero (enum table 0x10CF060, dispatch 0x10FCC50).
 * 0x70E6A0: row-major cell centers, 2.5 spacing, scene quaternion/translation.
 * This deliberately supports planar leaf scenes only. Parent grids cannot be
 * ORed with child grids: a blocked child cell overrides walkable parent ground.
 */
#define SCENE_MANAGER_GOT UINT64_C(0x114B898)
#define SNO_MAX_GOT UINT64_C(0x11572E8)
#define SNO_TABLE_GOT UINT64_C(0x11572F0)
#define SCENE_STRIDE UINT64_C(0x7D8)
#define SCENE_GROUP 33u
#define MAX_DIMENSION 512
#define MAX_COORDINATE 1000000.0f

typedef struct WorldIdentity {
    Snapshot player;
    uint64_t acd_globals, pool, pages, page, acd, visible_world;
    uint32_t capacity, shift, acd_identity, world, visible_world_id;
} WorldIdentity;

typedef struct NavRoots {
    uint64_t scene_pool, scenes;
    uint64_t manager_owner, manager, resident_pool, records;
    uint64_t max_owner, table_owner, sno_table;
    uint32_t capacity, active_count, group, resident_capacity, max_sno;
    uint16_t highest;
} NavRoots;

typedef struct SceneHeader { uint32_t handle, reserved, id, world; } SceneHeader;
typedef struct SceneTransform { uint32_t sno; float q[4], translation[3]; } SceneTransform;
typedef struct SceneState {
    SceneHeader header;
    SceneTransform transform;
    uint64_t grid, children;
    uint32_t child_count;
} SceneState;
typedef struct GridHeader { uint32_t width, height, unknown, checksum, scene_id; } GridHeader;
typedef struct ResidentRecord { uint32_t handle, flags; uint64_t asset; } ResidentRecord;
typedef struct ResourceHeader { uint32_t sno, references, flags; } ResourceHeader;
typedef struct NavAsset {
    int32_t width, height;
    uint32_t unknown, checksum;
    int32_t top, bottom, left, right;
    float height_min, height_max, height_default;
    uint32_t default_flags, reserved[4];
    uint64_t full_descriptor, full_cells, packed_descriptor, packed_cells;
} NavAsset;
typedef struct RetainedGrid {
    uint64_t address, record_address, sno_address;
    SceneState scene;
    GridHeader header;
    ResidentRecord record;
    ResourceHeader resource;
    NavAsset asset;
    uint32_t slot;
} RetainedGrid;
_Static_assert(sizeof(SceneTransform) == 32, "Scene transform layout");
_Static_assert(sizeof(NavAsset) == 0x60 && offsetof(NavAsset, full_cells) == 0x48 &&
               offsetof(NavAsset, packed_cells) == 0x58, "Scene navigation resource layout");

static int world_identity(Reader *r, const PlayerProbeIdentity *expected,
                          uint32_t world, WorldIdentity *s) {
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
        read_offset(r, s->acd, 0xA0, &s->world, 4, 4) && s->world == world &&
        pointer(r, expected->client, 0x800, &s->visible_world) &&
        read_offset(r, s->visible_world, 0x38, &s->visible_world_id, 4, 4) &&
        s->visible_world_id == world;
}

static int roots_read(Reader *r, uint64_t client, NavRoots *s) {
    memset(s, 0, sizeof *s);
    return pointer(r, client, 0xB18, &s->scene_pool) &&
        read_offset(r, s->scene_pool, 0x100, &s->capacity, 4, 4) &&
        s->capacity > 0 && s->capacity <= 65534 &&
        read_offset(r, s->scene_pool, 0x108, &s->highest, 2, 2) &&
        read_offset(r, s->scene_pool, 0x10C, &s->active_count, 4, 4) &&
        s->active_count <= s->capacity &&
        (s->highest == UINT16_MAX ? s->active_count == 0 :
          s->highest < s->capacity && s->highest < NAV_MAX_SCENE_SLOTS &&
          s->active_count <= (uint32_t)s->highest + 1u) &&
        pointer(r, s->scene_pool, 0x120, &s->scenes) &&
        pointer(r, r->host->main_base, SCENE_MANAGER_GOT, &s->manager_owner) &&
        pointer(r, s->manager_owner, 0, &s->manager) &&
        read_offset(r, s->manager, 0x50, &s->group, 4, 4) && s->group == SCENE_GROUP &&
        pointer(r, s->manager, 0x20, &s->resident_pool) &&
        read_offset(r, s->resident_pool, 0x100, &s->resident_capacity, 4, 4) &&
        s->resident_capacity > 0 && s->resident_capacity <= 0x40000 &&
        pointer(r, s->resident_pool, 0x120, &s->records) &&
        pointer(r, r->host->main_base, SNO_MAX_GOT, &s->max_owner) &&
        read_at(r, s->max_owner, &s->max_sno, 4, 4) && s->max_sno > 0 && s->max_sno <= 0x1000000 &&
        pointer(r, r->host->main_base, SNO_TABLE_GOT, &s->table_owner) &&
        pointer(r, s->table_owner, 0, &s->sno_table);
}

static int scene_read(Reader *r, uint64_t address, const SceneHeader *header, SceneState *s) {
    memset(s, 0, sizeof *s); s->header = *header;
    return read_offset(r, address, 8 + 0xE4, &s->transform, sizeof s->transform, 4) &&
        read_offset(r, address, 8 + 0x188, &s->grid, 8, 8) &&
        read_offset(r, address, 8 + 0x1C0, &s->children, 8, 8) &&
        read_offset(r, address, 8 + 0x1D0, &s->child_count, 4, 4);
}

static int record_same(const ResidentRecord *a, const ResidentRecord *b) {
    /* Low 24 bits are a volatile last-used tick, not resource ownership. */
    return a->handle == b->handle && a->asset == b->asset && (a->flags >> 24) == (b->flags >> 24);
}

static int resource_valid(const ResourceHeader *resource, uint32_t sno) {
    return resource->sno == sno && (resource->flags & 0x40u) != 0 && (resource->flags & 2u) == 0;
}

static int resource_same(const ResourceHeader *a, const ResourceHeader *b) {
    /* References change when the game accesses a resource. They neither alter
     * the SNO nor establish that the payload has completed loading. */
    return a->sno == b->sno && a->flags == b->flags;
}

static int coordinate(float value) { return isfinite(value) && fabsf(value) <= MAX_COORDINATE; }

static int grid_shape(const SceneState *s, const NavAsset *a, NavTerrainGrid *g) {
    if (a->width <= 0 || a->height <= 0 || a->width > MAX_DIMENSION || a->height > MAX_DIMENSION ||
        a->top < 0 || a->bottom < 0 || a->left < 0 || a->right < 0 ||
        a->top > a->height || a->bottom > a->height - a->top ||
        a->left > a->width || a->right > a->width - a->left) return 0;
    const float *q = s->transform.q, *t = s->transform.translation;
    for (unsigned i = 0; i < 4; ++i) if (!isfinite(q[i])) return 0;
    for (unsigned i = 0; i < 3; ++i) if (!coordinate(t[i])) return 0;
    /* Height does not affect XY for planar rotations. Other rotations would need
     * per-cell decoded heights, and are deliberately unsupported here. */
    if (q[0] != 0.0f || q[1] != 0.0f ||
        fabsf(q[2] * q[2] + q[3] * q[3] - 1.0f) > 0.001f) return 0;
    const float cosine = q[3] * q[3] - q[2] * q[2];
    const float sine = 2.0f * q[3] * q[2];
    g->scene_id = s->header.id; g->columns = (uint16_t)a->width; g->rows = (uint16_t)a->height;
    g->origin_x = t[0]; g->origin_y = t[1];
    g->axis_x_x = 2.5f * cosine; g->axis_x_y = 2.5f * sine;
    g->axis_y_x = -2.5f * sine; g->axis_y_y = 2.5f * cosine;
    return 1;
}

static void ground_set(NavProbeResult *out, uint32_t index, int value) {
    const uint8_t mask = (uint8_t)(1u << (index % 8));
    if (value) out->ground[index / 8] |= mask;
    else out->ground[index / 8] &= (uint8_t)~mask;
}

/* Read at most 256 bytes at a time. On pass two compare semantic ground bits,
 * allowing irrelevant height/other navigation flags to change. */
static int grid_cells(Reader *r, const RetainedGrid *kept, const NavTerrainGrid *grid,
                       NavProbeResult *out, int verify) {
    const NavAsset *a = &kept->asset;
    const uint32_t width = (uint32_t)a->width, height = (uint32_t)a->height;
    const uint32_t crop_width = width - (uint32_t)a->left - (uint32_t)a->right;
    const uint32_t crop_height = height - (uint32_t)a->top - (uint32_t)a->bottom;
    const int packed = a->packed_cells != 0;
    const uint64_t address = packed ? a->packed_cells : a->full_cells;
    const uint32_t crop_count = crop_width * crop_height;
    if (!verify) for (uint32_t i = 0; i < width * height; ++i)
        ground_set(out, grid->cell_offset + i, (a->default_flags & 1u) != 0);
    if (!address || !crop_count) return 1; // Native full-cell getter uses the default for a null array.
    if ((!packed && address % 2) || address > UINT64_MAX - (uint64_t)crop_count * (packed ? 1u : 4u)) return 0;
    const uint32_t cells_per_read = packed ? 512u : 64u;
    for (uint32_t start = 0; start < crop_count; start += cells_per_read) {
        uint8_t bytes[256];
        const uint32_t count = crop_count - start < cells_per_read ? crop_count - start : cells_per_read;
        const uint32_t size = packed ? (count + 1u) / 2u : count * 4u;
        const uint64_t offset = packed ? start / 2u : (uint64_t)start * 4u;
        if (!read_offset(r, address, offset, bytes, size, 1)) return 0;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t source = start + i;
            const uint32_t target = grid->cell_offset +
                (source / crop_width + (uint32_t)a->top) * width + source % crop_width + (uint32_t)a->left;
            const unsigned ground = packed ? ((bytes[i / 2] >> (4u * (i % 2))) & 1u) : (bytes[i * 4u + 2] & 1u);
            if (verify) { if (((out->ground[target / 8] >> (target % 8)) & 1u) != ground) return 0; }
            else ground_set(out, target, (int)ground);
        }
    }
    return 1;
}

static int terrain(Reader *r, uint64_t client, uint32_t world, NavProbeResult *out) {
    NavRoots roots, after;
    SceneHeader headers[NAV_MAX_SCENE_SLOTS];
    RetainedGrid retained[NAV_MAX_GRIDS];
    if (!roots_read(r, client, &roots)) return 0;
    const uint32_t slots = roots.highest == UINT16_MAX ? 0u : (uint32_t)roots.highest + 1u;
    uint32_t active = 0;
    for (uint32_t i = 0; i < slots; ++i) {
        uint64_t address;
        SceneHeader *header = &headers[i];
        if (!add(roots.scenes, (uint64_t)i * SCENE_STRIDE, &address) ||
            !read_at(r, address, header, sizeof *header, 8)) return 0;
        if (header->handle == UINT32_MAX) continue;
        if ((header->handle & 0xFFFFu) != i) return 0;
        ++active;
        if (header->world != world) continue;
        if (header->id == UINT32_MAX || out->grid_count >= NAV_MAX_GRIDS) {
            ++out->unavailable_scene_count; continue;
        }
        RetainedGrid candidate = {.address = address, .slot = i};
        if (!scene_read(r, address, header, &candidate.scene)) return 0;
        const SceneState *s = &candidate.scene;
        /* Runtime grids contain five uint32 fields and are allocated at a
         * 20-byte stride; their target address is only four-byte aligned. */
        if (s->child_count || s->children || !s->grid || s->grid % 4 || s->transform.sno >= roots.max_sno) {
            ++out->unavailable_scene_count; continue;
        }
        uint32_t handle;
        if (!add(roots.sno_table, (uint64_t)s->transform.sno * 4, &candidate.sno_address) ||
            !read_at(r, candidate.sno_address, &handle, 4, 4)) return 0;
        if (handle == UINT32_MAX || (handle & 0x3FFFFu) >= roots.resident_capacity) {
            ++out->unavailable_scene_count; continue;
        }
        if (!add(roots.records, (uint64_t)(handle & 0x3FFFFu) * 16, &candidate.record_address) ||
            !read_at(r, candidate.record_address, &candidate.record, sizeof candidate.record, 8)) return 0;
        if (candidate.record.handle != handle || candidate.record.flags >> 24 != SCENE_GROUP ||
            !candidate.record.asset || candidate.record.asset % 8) {
            ++out->unavailable_scene_count; continue;
        }
        if (!read_at(r, candidate.record.asset, &candidate.resource, sizeof candidate.resource, 4)) return 0;
        if (!resource_valid(&candidate.resource, s->transform.sno)) {
            ++out->unavailable_scene_count; continue;
        }
        if (!read_at(r, s->grid, &candidate.header, sizeof candidate.header, 4) ||
            !read_offset(r, candidate.record.asset, 0x40, &candidate.asset, sizeof candidate.asset, 8)) return 0;
        NavTerrainGrid *grid = &out->grids[out->grid_count];
        if (candidate.header.scene_id != header->id || memcmp(&candidate.header, &candidate.asset, 16) ||
            !grid_shape(s, &candidate.asset, grid) ||
            (uint32_t)grid->columns * grid->rows > NAV_MAX_CELLS - out->cell_count) {
            memset(grid, 0, sizeof *grid); ++out->unavailable_scene_count; continue;
        }
        grid->cell_offset = out->cell_count;
        if (!grid_cells(r, &candidate, grid, out, 0)) return 0;
        retained[out->grid_count++] = candidate;
        out->cell_count += (uint32_t)grid->columns * grid->rows;
    }
    if (active != roots.active_count) return 0;
    /* Recheck holes too: a newly inserted scene must not leave a supposedly
     * complete result missing ground/overrides from that scene. */
    for (uint32_t i = 0; i < slots; ++i) {
        SceneHeader header;
        if (!read_offset(r, roots.scenes, (uint64_t)i * SCENE_STRIDE, &header, sizeof header, 8) ||
            memcmp(&header, &headers[i], sizeof header)) return 0;
    }
    for (uint32_t i = 0; i < out->grid_count; ++i) {
        const RetainedGrid *kept = &retained[i];
        SceneState scene; GridHeader header; ResidentRecord record; ResourceHeader resource;
        NavAsset asset; uint32_t handle;
        if (!scene_read(r, kept->address, &headers[kept->slot], &scene) ||
            memcmp(&scene, &kept->scene, sizeof scene) ||
            !read_at(r, kept->sno_address, &handle, 4, 4) || handle != kept->record.handle ||
            !read_at(r, kept->record_address, &record, sizeof record, 8) || !record_same(&record, &kept->record) ||
            !read_at(r, record.asset, &resource, sizeof resource, 4) ||
            !resource_valid(&resource, scene.transform.sno) || !resource_same(&resource, &kept->resource) ||
            !read_at(r, scene.grid, &header, sizeof header, 4) || memcmp(&header, &kept->header, sizeof header) ||
            !read_offset(r, record.asset, 0x40, &asset, sizeof asset, 8) || memcmp(&asset, &kept->asset, sizeof asset) ||
            !grid_cells(r, kept, &out->grids[i], out, 1)) return 0;
    }
    return roots_read(r, client, &after) && !memcmp(&roots, &after, sizeof roots);
}

void nav_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
               uint32_t expected_world_id, NavProbeResult *result) {
    if (!result) return;
    *result = (NavProbeResult){.reason = "UNAVAILABLE: invalid navigation host/identity"};
    if (!expected || expected_world_id == UINT32_MAX || !host || !host->userdata ||
        !host->read_memory || !host->is_mapped || !host->main_base || host->main_base % 8 ||
        host->main_size < SNO_TABLE_GOT + 8 || host->main_base > UINT64_MAX - host->main_size) return;
    PlayerProbeResult work = {0};
    Reader reader = {host, &work, NAV_MAX_READS, NAV_MAX_BYTES, NULL};
    WorldIdentity first, last;
    const int first_valid = world_identity(&reader, expected, expected_world_id, &first);
    int valid = 0;
    if (first_valid) {
        reader.max_reads -= 64; reader.max_bytes -= 512;
        valid = terrain(&reader, expected->client, expected_world_id, result);
        reader.max_reads = NAV_MAX_READS; reader.max_bytes = NAV_MAX_BYTES;
    }
    const int last_valid = world_identity(&reader, expected, expected_world_id, &last);
    const int identity = first_valid && last_valid && !memcmp(&first, &last, sizeof first);
    if (!identity || !valid) {
        *result = (NavProbeResult){.shared_identity_valid = identity, .reads = work.reads, .bytes = work.bytes,
            .reason = identity ? "UNAVAILABLE: terrain changed, unreadable, or exceeds reader bounds" :
                                "UNAVAILABLE: player/world identity changed or unreadable"};
        return;
    }
    result->shared_identity_valid = 1; result->world_id = expected_world_id;
    result->reads = work.reads; result->bytes = work.bytes;
    result->available = result->grid_count != 0;
    result->partial = result->unavailable_scene_count != 0;
    result->reason = result->available ? (result->partial ?
        "UNVERIFIED RESEARCH: partial terrain; unsupported scenes unavailable" :
        "UNVERIFIED RESEARCH: resident terrain navigation") :
        "UNAVAILABLE: no supported resident terrain scenes";
}

int nav_cell_ground(const NavProbeResult *result, unsigned grid, unsigned column, unsigned row) {
    if (!result || !result->available || result->grid_count > NAV_MAX_GRIDS ||
        result->cell_count > NAV_MAX_CELLS || grid >= result->grid_count) return 0;
    const NavTerrainGrid *g = &result->grids[grid];
    if (column >= g->columns || row >= g->rows || g->cell_offset > result->cell_count) return 0;
    const uint32_t relative = row * (uint32_t)g->columns + column;
    if (relative >= result->cell_count - g->cell_offset) return 0;
    const uint32_t index = g->cell_offset + relative;
    return (result->ground[index / 8] >> (index % 8)) & 1u;
}
