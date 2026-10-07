// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic memory only. No fixture value is a game observation.
#define main base_reader_suite
#include "test_player_probe.c"
#undef main
#include "markers_probe.h"
#include <math.h>

#define GM UINT64_C(0x7500000000)
#define DATA UINT64_C(0x7600000000)
#define MM (DATA + 0x0000)
#define VEC (DATA + 0x0200)
#define VALUES (DATA + 0x1000)
#define T0 (DATA + 0x7000)
#define T1 (DATA + 0x7100)
#define BUCKET0 (DATA + 0x7200)
#define BUCKET1 (DATA + 0x7300)
#define OBJ (DATA + 0x8000)
#define OWNER (DATA + 0xB000)
#define LIST (DATA + 0xB100)
#define NODES (DATA + 0xC000)
#define ACD_GLOBALS (DATA + 0x11000)
#define ACD_POOL (DATA + 0x12000)
#define ACD_PAGES (DATA + 0x13000)
#define ACD_PAGE (DATA + 0x14000)
#define ACD (ACD_PAGE + 2 * 0x360)
#define ACD_IDS (DATA + 0x20000)
#define VW (DATA + 0x31000)
#define WORLD UINT32_C(0x11112222)
#define ACD_ID UINT32_C(0x43210002)
#define MAP_ROOT (MAIN + 0x1878FE8)
#define MARKER_ROOT (MAIN + 0x185B590)
#define OPTIONS (MAIN + 0x181AAC4)
#define OPTIONS_VISIBLE (MAIN + 0x181BD80)

static struct { uint64_t address; unsigned seen, on; } trigger;
static void put_float(Fixture *f, uint64_t a, float value) {
    uint32_t bits; memcpy(&bits, &value, 4); put32(f, a, bits);
}

static EdenDsmodBool markers_read(void *data, uint64_t address, void *out, size_t size) {
    Fixture *f = data;
    f->calls++;
    CHECK(size <= 2048);
    if ((f->mutate_on && f->mutate_on == f->calls) ||
        (trigger.address == address && ++trigger.seen == trigger.on))
        put(f, f->change_address, f->change_value, f->change_size);
    if (f->fail_on == f->calls) return 0;
    for (unsigned i = 0; i < 17; ++i) {
        Region *r = &f->regions[i];
        if (address >= r->base && address-r->base <= r->size && size <= r->size-(address-r->base)) {
            memcpy(out, r->bytes+(address-r->base), size); return 1;
        }
    }
    CHECK(0); return 0;
}

static void objective(Fixture *f, unsigned index, uint32_t id, uint32_t texture, float x, float y) {
    uint64_t a = OBJ + index * 0x58;
    put64(f, a, 0); put32(f, a+8, id);
    put_float(f, a+0xC, x); put_float(f, a+0x10, y);
    put32(f, a+0x18, WORLD); put32(f, a+0x1C, texture);
    put32(f, a+0x2C, UINT32_MAX); put32(f, a+0x30, UINT32_MAX);
    if (index) put64(f, a-0x58, a);
    put32(f, T0+4, index+1);
}

static void generic(Fixture *f, unsigned index, uint32_t id, uint32_t texture, float x, float y) {
    uint64_t a = NODES + index * 0x80;
    put32(f, a, id); put32(f, a+4, UINT32_MAX);
    put_float(f, a+8, x); put_float(f, a+0xC, y);
    put32(f, a+0x14, WORLD); put32(f, a+0x2C, texture);
    put64(f, a+0x70, index ? a-0x80 : 0); put64(f, a+0x78, 0);
    if (index) put64(f, a-0x80+0x78, a);
    put64(f, LIST+8, a); put32(f, LIST+0x10, index+1);
}

static EdenDsmodHostApi marker_setup(Fixture *f, MapProbeResult *map) {
    EdenDsmodHostApi host = setup(f);
    free(f->regions[4].bytes);
    f->regions[4] = (Region){PLAYERS,0x40000,calloc(1,0x40000)};
    CHECK(f->regions[4].bytes);
    const uint64_t bases[5] = {MAP_ROOT,MARKER_ROOT,OPTIONS,GM,DATA};
    const size_t sizes[5] = {8,8,0x1400,0x1200,0x40000};
    for (unsigned i = 0; i < 5; ++i) {
        f->regions[8+i] = (Region){bases[i],sizes[i],calloc(1,sizes[i])};
        CHECK(f->regions[8+i].bytes);
    }
    put64(f, MAIN+0x114A840, GM); put64(f, GM+0x10, C);
    put32(f, PLAYER, 0); put32(f, PLAYER+4, ACD_ID); put32(f, PLAYER+8, HANDLE);
    put32(f, PLAYER+0xD68C, 42);
    for (unsigned i = 0; i < 4; ++i) put32(f, PLAYERS+(uint64_t)i*0xEBF8+0xE7D0, UINT32_MAX);
    put64(f, GM+0x1100, VEC); put64(f, VEC, VALUES);
    put64(f, VEC+8, 0x544); put64(f, VEC+0x10, 0x544);
    const uint32_t setting_indices[4] = {0x267,0x268,0x541,0x543};
    for (unsigned i = 0; i < 4; ++i) {
        uint64_t a = VALUES+(uint64_t)setting_indices[i]*16;
        put32(f, a, 2); put32(f, a+4, 44); put32(f, a+8, 100+i);
    }
    put(f, OPTIONS, 1, 1); put32(f, OPTIONS_VISIBLE, 1);
    put64(f, C+0x9A0, ACD_GLOBALS); put64(f, ACD_GLOBALS, ACD_POOL);
    put64(f, ACD_GLOBALS+8, ACD_IDS);
    memset(f->regions[12].bytes+0x20000, 0xFF, 0x10000);
    put32(f, ACD_POOL+0x100, 4); put32(f, ACD_POOL+0x168, 2);
    put64(f, ACD_POOL+0x120, ACD_PAGES); put64(f, ACD_PAGES, ACD_PAGE);
    put32(f, ACD, ACD_ID); put32(f, ACD+0x10, 3); put32(f, ACD+0xA0, WORLD);
    put_float(f, ACD+0x60, 50); put_float(f, ACD+0x64, 50);
    put64(f, C+0x800, VW); put32(f, VW+0x38, WORLD);
    put64(f, MAP_ROOT, MM); put64(f, MM+8, T0); put64(f, MM+0x10, T1);
    put32(f, T0+0x18, 1); put64(f, T0+0x10, BUCKET0); put64(f, BUCKET0, OBJ);
    put32(f, T1+0x18, 1); put64(f, T1+0x10, BUCKET1);
    objective(f, 0, 0x9001, 102, 100, 100);
    objective(f, 1, 0x9002, 103, 200, 100);
    put64(f, MARKER_ROOT, OWNER); put64(f, OWNER+8, LIST); put64(f, LIST, NODES);
    generic(f, 0, 0x8001, 100, 300, 100);
    host.main_size = 0x2000000; host.read_memory = markers_read;
    PlayerProbeResult p; player_probe(&host, &p); CHECK(p.available);
    f->initial_identity = p.identity;
    *map = (MapProbeResult){.available=1,.shared_identity_valid=1,.exploration_available=1,
        .world_id=WORLD,.x=50,.y=50,.tile_count=1};
    map->tiles[0] = (MapExplorationTile){.min_x=0,.min_y=0,.max_x=1000,.max_y=1000,
        .columns=1,.rows=1,.fully_revealed=1};
    f->calls = f->maps = 0; memset(&trigger, 0, sizeof trigger);
    return host;
}

static void bounded(Fixture *f, const MarkersProbeResult *out) {
    CHECK(out->reads <= MARKERS_MAX_READS && out->bytes <= MARKERS_MAX_BYTES);
    CHECK(f->calls == out->reads && f->writes == 0 && f->guest_calls == 0);
    CHECK(out->count <= MARKERS_MAX_ITEMS);
    if (!out->available) {
        CHECK(out->count == 0 && out->world_id == 0);
        for (unsigned i = 0; i < MARKERS_MAX_ITEMS; ++i) CHECK(out->items[i].kind == MARKER_NONE);
    }
}

static void positive(void) {
    Fixture f; MapProbeResult map; EdenDsmodHostApi h = marker_setup(&f, &map); MarkersProbeResult out;
    markers_probe(&h, &f.initial_identity, &map, &out); bounded(&f, &out);
    CHECK(out.available && out.shared_identity_valid && out.world_id == WORLD && out.count == 3);
    CHECK(out.items[0].kind == MARKER_QUEST && out.items[0].x == 100);
    CHECK(out.items[1].kind == MARKER_WAYPOINT && out.items[1].y == 100);
    CHECK(out.items[2].kind == MARKER_PORTAL && out.items[2].id == 0x8001);
    release(&f);
}

static void failures(void) {
    Fixture f; MapProbeResult map; EdenDsmodHostApi h = marker_setup(&f, &map); MarkersProbeResult out;
    markers_probe(&h, &f.initial_identity, &map, &out); unsigned reads = f.calls; release(&f);
    for (unsigned mode = 0; mode < 2; ++mode) for (unsigned i = 1; i <= reads; ++i) {
        h = marker_setup(&f, &map); if (mode) f.map_fail_on = i; else f.fail_on = i;
        markers_probe(&h, &f.initial_identity, &map, &out); bounded(&f, &out);
        CHECK(!out.available); release(&f);
    }
}

static void visibility(void) {
    for (unsigned test = 0; test < 11; ++test) {
        Fixture f; MapProbeResult map; EdenDsmodHostApi h = marker_setup(&f, &map); MarkersProbeResult out;
        unsigned count = 2;
        if (test == 0) { map.tiles[0].fully_revealed=0; count=0; }
        if (test == 1) put32(&f, OBJ+0x18, WORLD+1);
        if (test == 2) put32(&f, OBJ+0x1C, 777);
        if (test == 3) put32(&f, OPTIONS_VISIBLE, 0);
        if (test == 4) put_float(&f, OBJ+0x44, 1);
        if (test == 5) put_float(&f, OBJ+0x48, 1000000);
        if (test == 6) put32(&f, NODES+0x28, 2);
        if (test == 7) put32(&f, NODES+0x24, 3);
        if (test == 8) put32(&f, MM+0x98, 4);
        if (test == 9) { put32(&f, NODES, ACD_ID); }
        if (test == 10) { put32(&f, NODES, 3); put32(&f, ACD_IDS+12, ACD_ID); }
        markers_probe(&h, &f.initial_identity, &map, &out); bounded(&f, &out);
        CHECK(out.available && out.count == count); release(&f);
    }
    Fixture f; MapProbeResult map; EdenDsmodHostApi h = marker_setup(&f, &map); MarkersProbeResult out;
    map.tiles[0].fully_revealed=0; map.tiles[0].cells[0]=0x80;
    markers_probe(&h, &f.initial_identity, &map, &out); CHECK(out.available && out.count==3); release(&f);
}

static void invalid(void) {
    const struct { uint64_t address,value; size_t size; } cases[] = {
        {MM+8,0,8},{T0+4,129,4},{T0+0x18,257,4},{T0,1,4},{T0+4,1,4},{T0+4,3,4},
        {OBJ,OBJ,8},{LIST+0x10,129,4},{NODES+0x78,NODES,8},{NODES+0x70,NODES,8},
        {LIST+8,0,8},{OBJ+0xC,0x7FC00000,4},{NODES+8,0x7F800000,4},
        {OBJ+0x44,0x7F800000,4},{VEC+8,1,8},{VALUES+0x541*16+4,1,4},
        {ACD+0xA0,WORLD+1,4},{VW+0x38,WORLD+1,4},{ACD,ACD_ID+0x10000,4},
    };
    for (unsigned i=0; i<sizeof cases/sizeof cases[0]; ++i) {
        Fixture f; MapProbeResult map; EdenDsmodHostApi h=marker_setup(&f,&map); MarkersProbeResult out;
        put(&f,cases[i].address,cases[i].value,cases[i].size);
        markers_probe(&h,&f.initial_identity,&map,&out); bounded(&f,&out);
        CHECK(!out.available); release(&f);
    }
}

static void mutations(void) {
    const struct { uint64_t trigger,address,value; size_t size; } cases[] = {
        {OBJ,OBJ+0x1C,777,4},{OBJ,OBJ+0x18,WORLD+1,4},{OBJ,OBJ+0xC,0x43FA0000,4},
        {NODES,NODES+0x28,2,4},{T0,T0+4,0,4},{BUCKET0,BUCKET0,0,8},
        {MAP_ROOT,MAP_ROOT,MM+0x100,8},{VEC+8,VEC+8,0x545,8},
        {OPTIONS_VISIBLE,OPTIONS_VISIBLE,0,4},{ACD+0xA0,ACD+0xA0,WORLD+1,4},
    };
    for(unsigned i=0; i<sizeof cases/sizeof cases[0]; ++i) {
        Fixture f; MapProbeResult map; EdenDsmodHostApi h=marker_setup(&f,&map); MarkersProbeResult out;
        trigger.address=cases[i].trigger; trigger.on=2;
        f.change_address=cases[i].address; f.change_value=cases[i].value; f.change_size=cases[i].size;
        markers_probe(&h,&f.initial_identity,&map,&out); bounded(&f,&out);
        CHECK(!out.available); release(&f);
    }
}

static void unsupported(void) {
    Fixture f; MapProbeResult map; EdenDsmodHostApi h=marker_setup(&f,&map); MarkersProbeResult out;
    map.exploration_available=0;
    markers_probe(&h,&f.initial_identity,&map,&out); CHECK(!out.available && !f.calls);
    map.exploration_available=1; h.main_size=0x100;
    markers_probe(&h,&f.initial_identity,&map,&out); CHECK(!out.available && !f.calls);
    markers_probe(NULL,NULL,NULL,NULL); release(&f);
}

int main(void) {
    positive(); failures(); visibility(); invalid(); mutations(); unsupported();
    puts("PASS synthetic marker reader: exact texture classes, explored-only filters, bounded containers, failures clear; no writes");
    return 0;
}
