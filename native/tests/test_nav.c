// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic memory only. No fixture values are observations of the game.
#define main base_reader_suite
#include "test_player_probe.c"
#undef main
#include "nav_probe.h"
#include <math.h>

#define ARENA UINT64_C(0x7500000000)
#define AG (ARENA + 0x0000)
#define AP (ARENA + 0x0100)
#define AT (ARENA + 0x0300)
#define AA (ARENA + 0x0800)
#define ACD (AA + 2 * 0x360)
#define ACD_ID UINT32_C(0x56780002)
#define VW (ARENA + 0x2000)
#define SP (ARENA + 0x3000)
#define MO (ARENA + 0x4000)
#define MANAGER (ARENA + 0x4100)
#define RP (ARENA + 0x4300)
#define RECORDS (ARENA + 0x5000)
#define MAX_OWNER (ARENA + 0x6000)
#define TABLE_OWNER (ARENA + 0x6010)
#define SNO_TABLE (ARENA + 0x7000)
#define SCENES (ARENA + 0x10000)
#define GRID_HEADERS (ARENA + 0x210000)
#define ASSETS (ARENA + 0x220000)
#define FULL (ARENA + 0x250000)
#define PACKED (ARENA + 0x290000)
#define WORLD UINT32_C(0x567890AB)
#define SNO_GOT (MAIN + 0x11572E8)
#define MANAGER_GOT (MAIN + 0x114B898)
#define INSTANCE(i) (SCENES + (uint64_t)(i) * 0x7D8)
#define SCENE(i) (INSTANCE(i) + 8)
#define ASSET(i) (ASSETS + (uint64_t)(i) * 0x100)
#define GRID(i) (GRID_HEADERS + (uint64_t)(i) * 0x20)
#define RECORD(i) (RECORDS + (uint64_t)(i) * 16)
#define SNO(i) ((uint32_t)(i) + 100)
#define SCENE_ID(i) ((uint32_t)(i) + 1000)
#define RESIDENT_HANDLE(i) (UINT32_C(0x40000) + (uint32_t)(i))

static struct { uint64_t address; unsigned seen, on; } trigger;
static void put_float(Fixture *f, uint64_t address, float value) {
    uint32_t bits; memcpy(&bits, &value, 4); put32(f, address, bits);
}
static EdenDsmodBool nav_read(void *p, uint64_t address, void *out, size_t size) {
    Fixture *f = p; f->calls++; CHECK(size <= 256);
    if ((f->mutate_on && f->calls == f->mutate_on) ||
        (trigger.address == address && ++trigger.seen == trigger.on)) {
        put(f, f->change_address, f->change_value, f->change_size);
        if (f->second_size) put(f, f->second_address, f->second_value, f->second_size);
    }
    if (f->fail_on == f->calls) return 0;
    for (unsigned i = 0; i < 17; ++i) {
        Region *r = &f->regions[i];
        if (address >= r->base && address - r->base <= r->size && size <= r->size - (address - r->base)) {
            memcpy(out, r->bytes + address - r->base, size); return 1;
        }
    }
    CHECK(0); return 0;
}

static void scene(Fixture *f, unsigned index, unsigned width, unsigned height, int packed) {
    CHECK(index < 128 && width <= 512 && height <= 512);
    put32(f, INSTANCE(index), 0x120000u + index);
    put32(f, SCENE(index), SCENE_ID(index)); put32(f, SCENE(index) + 4, WORLD);
    put32(f, SCENE(index) + 0xE4, SNO(index));
    put_float(f, SCENE(index) + 0xF4, 1); // Quaternion W.
    put_float(f, SCENE(index) + 0xF8, (float)index * 20);
    put64(f, SCENE(index) + 0x188, GRID(index));
    put32(f, GRID(index), width); put32(f, GRID(index) + 4, height);
    put32(f, GRID(index) + 0xC, 42); put32(f, GRID(index) + 0x10, SCENE_ID(index));
    put32(f, SNO_TABLE + SNO(index) * 4, RESIDENT_HANDLE(index));
    put32(f, RECORD(index), RESIDENT_HANDLE(index)); put32(f, RECORD(index) + 4, 33u << 24);
    put64(f, RECORD(index) + 8, ASSET(index));
    put32(f, ASSET(index), SNO(index)); put32(f, ASSET(index) + 8, 0x40);
    put32(f, ASSET(index) + 0x40, width); put32(f, ASSET(index) + 0x44, height);
    put32(f, ASSET(index) + 0x4C, 42);
    put64(f, ASSET(index) + 0x88, 0); put64(f, ASSET(index) + 0x98, 0);
    if (packed) put64(f, ASSET(index) + 0x98, PACKED + index * 512u);
    else put64(f, ASSET(index) + 0x88, FULL + index * 4096u);
    for (unsigned i = 0; i < width * height; ++i) {
        if (packed) { if (i % 2 == 0) put(f, PACKED + index * 512u + i / 2, 0x10, 1); }
        else put(f, FULL + index * 4096u + i * 4u + 2, (i % 3 == 1) ? 1 : 0, 2);
    }
}

static EdenDsmodHostApi nav_setup(Fixture *f) {
    EdenDsmodHostApi h = setup(f);
    f->regions[8] = (Region){ARENA, 0x300000, calloc(1, 0x300000)};
    f->regions[9] = (Region){MANAGER_GOT, 8, calloc(1, 8)};
    f->regions[10] = (Region){SNO_GOT, 16, calloc(1, 16)};
    CHECK(f->regions[8].bytes && f->regions[9].bytes && f->regions[10].bytes);
    put32(f, PLAYER + 4, ACD_ID); put64(f, C + 0x9A0, AG); put64(f, AG, AP);
    put32(f, AP + 0x100, 4); put32(f, AP + 0x168, 2); put64(f, AP + 0x120, AT); put64(f, AT, AA);
    put32(f, ACD, ACD_ID); put32(f, ACD + 0xA0, WORLD);
    put64(f, C + 0x800, VW); put32(f, VW + 0x38, WORLD);
    put64(f, C + 0xB18, SP); put32(f, SP + 0x100, 1024);
    put(f, SP + 0x108, 2, 2); put32(f, SP + 0x10C, 2); put64(f, SP + 0x120, SCENES);
    for (unsigned i = 0; i < 1024; ++i) put32(f, INSTANCE(i), UINT32_MAX);
    put64(f, MANAGER_GOT, MO); put64(f, MO, MANAGER); put32(f, MANAGER + 0x50, 33);
    put64(f, MANAGER + 0x20, RP); put32(f, RP + 0x100, 128); put64(f, RP + 0x120, RECORDS);
    put64(f, SNO_GOT, MAX_OWNER); put32(f, MAX_OWNER, 1024);
    put64(f, SNO_GOT + 8, TABLE_OWNER); put64(f, TABLE_OWNER, SNO_TABLE);
    scene(f, 0, 4, 3, 1); scene(f, 2, 5, 2, 0);
    h.read_memory = nav_read; h.main_size = 0x2000000;
    PlayerProbeResult p; player_probe(&h, &p); CHECK(p.available); f->initial_identity = p.identity;
    f->calls = f->maps = 0; memset(&trigger, 0, sizeof trigger); return h;
}

static void bounded(const Fixture *f, const NavProbeResult *r) {
    CHECK(r->reads <= NAV_MAX_READS && r->bytes <= NAV_MAX_BYTES);
    CHECK(f->calls == r->reads && f->maps <= NAV_MAX_READS + 2 && !f->writes);
    CHECK(r->grid_count <= NAV_MAX_GRIDS && r->cell_count <= NAV_MAX_CELLS);
    if (!r->available) CHECK(!r->grid_count && !r->cell_count);
}

static void decoders_and_transform(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    /* Rotate the second scene 90 degrees. Its world corner must move on the
     * corresponding perpendicular axes, with no dependency on terrain height. */
    put_float(&f, SCENE(2) + 0xF0, sqrtf(0.5f)); put_float(&f, SCENE(2) + 0xF4, sqrtf(0.5f));
    put_float(&f, SCENE(2) + 0xFC, 17); put_float(&f, SCENE(2) + 0x100, 123);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(r.available && r.shared_identity_valid && !r.partial && r.grid_count == 2 && r.cell_count == 22);
    CHECK(r.grids[0].scene_id == SCENE_ID(0) && r.grids[1].scene_id == SCENE_ID(2));
    for (unsigned i = 0; i < 12; ++i) CHECK(nav_cell_ground(&r, 0, i % 4, i / 4) == (int)(i % 2));
    for (unsigned i = 0; i < 10; ++i) CHECK(nav_cell_ground(&r, 1, i % 5, i / 5) == (int)(i % 3 == 1));
    CHECK(r.grids[1].origin_x == 40 && r.grids[1].origin_y == 17);
    CHECK(fabsf(r.grids[1].axis_x_x) < 0.0001f && fabsf(r.grids[1].axis_x_y - 2.5f) < 0.0001f);
    CHECK(fabsf(r.grids[1].axis_y_x + 2.5f) < 0.0001f && fabsf(r.grids[1].axis_y_y) < 0.0001f);
    CHECK(!nav_cell_ground(NULL, 0, 0, 0) && !nav_cell_ground(&r, 2, 0, 0) && !nav_cell_ground(&r, 0, 4, 0));
    release(&f);
}

static void cropped_defaults_and_odd_nibbles(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    scene(&f, 0, 5, 5, 1);
    put32(&f, ASSET(0) + 0x50, 1); put32(&f, ASSET(0) + 0x54, 1);
    put32(&f, ASSET(0) + 0x58, 1); put32(&f, ASSET(0) + 0x5C, 1);
    put32(&f, ASSET(0) + 0x6C, 1);
    for (unsigned i = 0; i < 5; ++i) put(&f, PACKED + i, 0xA1, 1); // Ground, blocked, repeated; unrelated flags set.
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r); CHECK(r.available && !r.partial);
    for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 5; ++x) {
        const int expected = x == 0 || x == 4 || y == 0 || y == 4 ? 1 : (((y - 1) * 3 + x - 1) % 2 == 0);
        CHECK(nav_cell_ground(&r, 0, x, y) == expected);
    }
    /* Null full storage is the native default-flags path. */
    f.calls = f.maps = 0; put64(&f, ASSET(2) + 0x88, 0); put32(&f, ASSET(2) + 0x6C, 1);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    for (unsigned y = 0; y < 2; ++y) for (unsigned x = 0; x < 5; ++x) CHECK(nav_cell_ground(&r, 1, x, y));
    release(&f);
}

static void four_byte_aligned_runtime_grid(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    /* A valid 20-byte runtime header need not have eight-byte alignment. */
    const uint64_t grid = GRID(0) + 4;
    put64(&f, SCENE(0) + 0x188, grid);
    put32(&f, grid, 4); put32(&f, grid + 4, 3); put32(&f, grid + 8, 0);
    put32(&f, grid + 0xC, 42); put32(&f, grid + 0x10, SCENE_ID(0));
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(r.available && r.shared_identity_valid && !r.partial &&
          r.grid_count == 2 && r.cell_count == 22 && !r.unavailable_scene_count);
    CHECK(r.grids[0].scene_id == SCENE_ID(0));
    for (unsigned i = 0; i < 12; ++i) CHECK(nav_cell_ground(&r, 0, i % 4, i / 4) == (int)(i % 2));
    release(&f);
}

static void every_read_failure(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    nav_probe(&h, &f.initial_identity, WORLD, &r); const unsigned calls = f.calls; release(&f);
    for (unsigned mode = 0; mode < 2; ++mode) for (unsigned n = 1; n <= calls; ++n) {
        h = nav_setup(&f); if (mode) f.map_fail_on = n; else f.fail_on = n;
        memset(&r, 0xA5, sizeof r); nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
        CHECK(!r.available && !r.grid_count && !r.cell_count); release(&f);
    }
}

static void invalid_roots_and_identity(void) {
    const struct { uint64_t address, value; size_t size; } cases[] = {
        {C + 0xB18, 0, 8}, {SP + 0x100, 65535, 4}, {SP + 0x108, 1024, 2},
        {SP + 0x10C, 4, 4}, {SP + 0x120, SCENES + 4, 8}, {MANAGER_GOT, MO + 4, 8},
        {MO, 0, 8}, {MANAGER + 0x50, 44, 4}, {RP + 0x100, 0x40001, 4},
        {SNO_GOT, 0, 8}, {MAX_OWNER, 0x1000001, 4}, {TABLE_OWNER, 0, 8},
        {ACD + 0xA0, WORLD + 1, 4}, {VW + 0x38, WORLD + 1, 4}, {ACD, ACD_ID + 0x10000, 4},
        {INSTANCE(0), 0x120001, 4}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
        put(&f, cases[i].address, cases[i].value, cases[i].size);
        nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r); CHECK(!r.available); release(&f);
    }
}

static void unsupported_scenes_are_absent(void) {
    const struct { uint64_t address, value; size_t size; } cases[] = {
        {SCENE(0) + 0x1D0, 1, 4}, {SCENE(0) + 0x1C0, ARENA, 8},
        {SCENE(0) + 0x188, 0, 8}, {SCENE(0) + 0x188, GRID(0) + 2, 8},
        {SCENE(0) + 0xE4, UINT32_MAX, 4}, {SNO_TABLE + SNO(0) * 4, UINT32_MAX, 4},
        {SNO_TABLE + SNO(0) * 4, 0x40080, 4}, {RECORD(0), 0x80000, 4},
        {RECORD(0) + 4, 44u << 24, 4}, {RECORD(0) + 8, 0, 8},
        {GRID(0) + 0x10, 9999, 4}, {GRID(0) + 0xC, 41, 4},
        {SCENE(0) + 0xF8, 0x7FC00000, 4}, {SCENE(0) + 0xE8, 0x3F000000, 4},
        {SCENE(0) + 0xF4, 0, 4}, {ASSET(0) + 0x50, UINT32_MAX, 4},
        {ASSET(0) + 0x58, 5, 4}, {ASSET(0) + 0x54, 4, 4}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
        put(&f, cases[i].address, cases[i].value, cases[i].size);
        nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
        CHECK(r.available && r.partial && r.grid_count == 1 && r.cell_count == 10);
        CHECK(r.unavailable_scene_count == 1 && r.grids[0].scene_id == SCENE_ID(2)); release(&f);
    }
}

static void mutations_clear(void) {
    const struct { uint64_t address, value; size_t size; } cases[] = {
        {PLAYER + 4, ACD_ID + 0x10000, 4}, {ACD + 0xA0, WORLD + 1, 4},
        {C + 0x84, 0, 4}, {SP + 0x10C, 3, 4}, {SP + 0x120, SCENES + 8, 8},
        {INSTANCE(0), 0x130000, 4}, {INSTANCE(1), 0x120001, 4}, {SCENE(0) + 4, WORLD + 1, 4},
        {SCENE(0) + 0xE4, SNO(2), 4}, {SCENE(0) + 0xF8, 0x3F800000, 4},
        {SCENE(0) + 0x188, GRID(2), 8}, {SCENE(0) + 0x1D0, 1, 4},
        {SNO_TABLE + SNO(0) * 4, 0x80000, 4}, {RECORD(0), 0x80000, 4},
        {RECORD(0) + 8, ASSET(2), 8}, {GRID(0) + 0xC, 77, 4},
        {ASSET(0) + 0x98, PACKED + 1, 8}, {PACKED, 0x11, 1}, {FULL + 2 * 4096 + 2, 1, 2},
        {MANAGER_GOT, MO + 8, 8}, {MAX_OWNER, 2048, 4}, {TABLE_OWNER, SNO_TABLE + 8, 8}
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
        trigger.address = INSTANCE(0); trigger.on = 2;
        f.change_address = cases[i].address; f.change_value = cases[i].value; f.change_size = cases[i].size;
        nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r); CHECK(trigger.seen);
        CHECK(!r.available && !r.grid_count && !r.cell_count); release(&f);
    }
    /* Only the resident last-used tick changes: ownership is still the same. */
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    trigger.address = INSTANCE(0); trigger.on = 2;
    f.change_address = RECORD(0) + 4; f.change_value = (33u << 24) | 123; f.change_size = 4;
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r); CHECK(r.available && !r.partial); release(&f);
}

static void maximum_budget_and_empty_world(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    scene(&f, 0, 256, 256, 0); put32(&f, INSTANCE(2), UINT32_MAX);
    put32(&f, SP + 0x10C, 1); put(&f, SP + 0x108, 1023, 2);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(r.available && !r.partial && r.grid_count == 1 && r.cell_count == NAV_MAX_CELLS && r.bytes > 524288);
    CHECK(nav_cell_ground(&r, 0, 1, 0) && !nav_cell_ground(&r, 0, 0, 0));
    f.calls = f.maps = 0; put32(&f, SCENE(0) + 4, WORLD + 1);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(!r.available && r.shared_identity_valid && !r.partial && !r.cell_count);
    f.calls = f.maps = 0; put32(&f, INSTANCE(0), UINT32_MAX); put32(&f, SP + 0x10C, 0);
    put(&f, SP + 0x108, UINT16_MAX, 2);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r); CHECK(!r.available && r.shared_identity_valid);
    release(&f);
}

static void resource_header_ownership(void) {
    const struct { uint64_t address; uint32_t value; } rejected[] = {
        {ASSET(0), SNO(0) + 1}, // Correct resident handle must not excuse a different resource SNO.
        {ASSET(0) + 8, 0}, // Published resident entry, but loading has not finished.
        {ASSET(0) + 8, 0x42} // A loaded fallback is not the requested scene resource.
    };
    for (unsigned mutation = 0; mutation < 2; ++mutation) {
        for (unsigned i = 0; i < sizeof rejected / sizeof rejected[0]; ++i) {
            Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
            if (mutation) {
                /* Change the header after its first valid read, while the payload
                 * is being copied. Revalidation must discard the entire sample. */
                trigger.address = PACKED; trigger.on = 1;
                f.change_address = rejected[i].address; f.change_value = rejected[i].value; f.change_size = 4;
            } else put32(&f, rejected[i].address, rejected[i].value);
            nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
            if (mutation) CHECK(!r.available && !r.grid_count && !r.cell_count && trigger.seen);
            else CHECK(r.available && r.partial && r.grid_count == 1 &&
                       r.unavailable_scene_count == 1 && r.grids[0].scene_id == SCENE_ID(2));
            release(&f);
        }
    }
    /* Ordinary guest resource references are volatile, so they are deliberately
     * excluded from the header comparison; no guest reference is acquired here. */
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    trigger.address = PACKED; trigger.on = 1;
    f.change_address = ASSET(0) + 4; f.change_value = 99; f.change_size = 4;
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(r.available && !r.partial && r.grid_count == 2 && trigger.seen); release(&f);
}

static void grid_and_cell_limits(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    for (unsigned i = 0; i < 65; ++i) scene(&f, i, 4, 3, 1);
    put(&f, SP + 0x108, 64, 2); put32(&f, SP + 0x10C, 65);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(r.available && r.partial && r.grid_count == 64 && r.unavailable_scene_count == 1);
    release(&f);
    h = nav_setup(&f); scene(&f, 0, 256, 256, 1);
    nav_probe(&h, &f.initial_identity, WORLD, &r); bounded(&f, &r);
    CHECK(r.available && r.partial && r.grid_count == 1 && r.cell_count == NAV_MAX_CELLS && r.unavailable_scene_count == 1);
    release(&f);
}

static void invalid_hosts_clear(void) {
    Fixture f; EdenDsmodHostApi h = nav_setup(&f); NavProbeResult r;
    memset(&r, 0xA5, sizeof r); nav_probe(NULL, &f.initial_identity, WORLD, &r); CHECK(!r.available && !r.reads);
    nav_probe(&h, NULL, WORLD, &r); CHECK(!r.available && !r.reads);
    nav_probe(&h, &f.initial_identity, UINT32_MAX, &r); CHECK(!r.available && !r.reads);
    h.main_size = UINT64_MAX; nav_probe(&h, &f.initial_identity, WORLD, &r); CHECK(!r.available && !r.reads);
    release(&f);
}

int main(void) {
    decoders_and_transform(); cropped_defaults_and_odd_nibbles(); four_byte_aligned_runtime_grid(); every_read_failure();
    invalid_roots_and_identity(); unsupported_scenes_are_absent(); mutations_clear();
    maximum_budget_and_empty_world(); resource_header_ownership(); grid_and_cell_limits(); invalid_hosts_clear();
    puts("PASS navigation synthetic decoder, ownership, mutation, failure and bounds suite"); return 0;
}
