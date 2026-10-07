// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic memory only. Fixture values are never observations of the game.
#define main base_reader_suite
#include "test_player_probe.c"
#undef main
#include "map_probe.h"
#include <math.h>

#define MG UINT64_C(0x7300000000)
#define MP UINT64_C(0x7300001000)
#define MT UINT64_C(0x7300002000)
#define MA UINT64_C(0x7300003000)
#define ACD (MA + 2 * 0x360)
#define ACD_ID UINT32_C(0x76540002)
#define MAP_ROOT (MAIN + 0x1878FE8)
#define MM UINT64_C(0x7400000000)
#define MN UINT64_C(0x7400001000)
#define MC UINT64_C(0x7400010000)
#define VW UINT64_C(0x7400020000)
#define WORLD UINT32_C(0x12345678)

static uint32_t float_bits(float v) { uint32_t b; memcpy(&b, &v, 4); return b; }
static void put_float(Fixture *f, uint64_t a, float v) { put32(f, a, float_bits(v)); }
static struct { uint64_t address; unsigned seen, on; } trigger;

static EdenDsmodBool map_read(void *p, uint64_t a, void *out, size_t n) {
    Fixture *f = p;
    f->calls++;
    CHECK(n <= MAP_TILE_BYTES);
    if ((f->mutate_on && f->mutate_on == f->calls) ||
        (trigger.address == a && ++trigger.seen == trigger.on)) {
        put(f, f->change_address, f->change_value, f->change_size);
        if (f->second_size) put(f, f->second_address, f->second_value, f->second_size);
    }
    if (f->fail_on == f->calls) return 0;
    for (unsigned i = 0; i < 17; ++i) {
        Region *r = &f->regions[i];
        if (a >= r->base && a-r->base <= r->size && n <= r->size-(a-r->base)) {
            memcpy(out, r->bytes+(a-r->base), n); return 1;
        }
    }
    CHECK(0); return 0;
}

static void node(Fixture *f, unsigned index, uint32_t world, int full) {
    const uint64_t a = MN + index * 0x58;
    put32(f, a, index+100); put32(f, a+4, index+200); put32(f, a+8, world); put32(f, a+12, 300);
    put_float(f, a+0x10, (float)index*100); put_float(f, a+0x14, 0);
    put_float(f, a+0x18, (float)index*100+100); put_float(f, a+0x1C, 50);
    put32(f, a+0x20, UINT32_MAX); put64(f, a+0x28, full ? 0 : MC+index*256);
    put32(f, a+0x30, 16); put32(f, a+0x34, 32); put32(f, a+0x3C, (uint32_t)full);
    put64(f, a+0x48, index ? a-0x58 : 0); put64(f, a+0x50, 0);
    if (index) put64(f, a-0x58+0x50, a);
    put64(f, MM+0x28, a); put32(f, MM+0x30, index+1);
    if (!full) for (unsigned i = 0; i < 128; ++i) put(f, MC+index*256+i, 0x1B, 1);
}

static EdenDsmodHostApi map_setup(Fixture *f) {
    EdenDsmodHostApi h = setup(f);
    uint64_t bases[9] = {MG, MP, MT, MA, MAP_ROOT, MM, MN, MC, VW};
    size_t sizes[9] = {8, 0x170, 32, 0xD80, 8, 0x40, 128*0x58, 128*256, 0x40};
    for (unsigned i=0; i<9; ++i) {
        f->regions[i+8]=(Region){bases[i],sizes[i],calloc(1,sizes[i])}; CHECK(f->regions[i+8].bytes);
    }
    put32(f, PLAYER+4, ACD_ID); put64(f, C+0x9A0, MG); put64(f, MG, MP);
    put32(f, MP+0x100, 4); put32(f, MP+0x168, 2); put64(f, MP+0x120, MT); put64(f, MT, MA);
    put32(f, ACD, ACD_ID); put32(f, ACD+0xA0, WORLD);
    put_float(f, ACD+0x60, 25); put_float(f, ACD+0x64, 12.5f); put_float(f, ACD+0x68, -4);
    put64(f, C+0x800, VW); put32(f, VW+0x38, WORLD);
    put64(f, MAP_ROOT, MM); put64(f, MM+0x20, MN);
    node(f, 0, WORLD, 0); node(f, 1, WORLD, 1);
    h.main_size=0x2000000; h.read_memory=map_read;
    PlayerProbeResult p; player_probe(&h, &p); CHECK(p.available); f->initial_identity=p.identity;
    f->calls=f->maps=0; memset(&trigger,0,sizeof trigger); return h;
}

static void bounded(const Fixture *f, const MapProbeResult *r) {
    CHECK(r->reads <= MAP_MAX_READS && r->bytes <= MAP_MAX_BYTES);
    CHECK(f->calls == r->reads && f->maps <= MAP_MAX_READS && !f->writes);
    CHECK(!r->exploration_available || r->available);
    if (!r->available) CHECK(!r->exploration_available && !r->tile_count && !r->world_id && r->x==0);
    if (!r->exploration_available) CHECK(!r->tile_count);
}

static void valid_map(void) {
    Fixture f; EdenDsmodHostApi h=map_setup(&f); MapProbeResult r;
    map_probe(&h,&f.initial_identity,&r); bounded(&f,&r);
    CHECK(r.available && r.shared_identity_valid && r.exploration_available && r.tile_count==2);
    CHECK(r.world_id==WORLD && r.x==25 && r.y==12.5f && r.z==-4);
    CHECK(r.tiles[0].columns==32 && r.tiles[0].rows==16 && r.tiles[0].min_x==0 && r.tiles[0].max_y==50);
    /* 00,01,10,11 must reproduce transparent,clear,half,clear native shades. */
    const unsigned expected[4]={0,2,1,2};
    for(unsigned row=0;row<16;row++) CHECK(map_cell_visibility(&r.tiles[0],0,row)==expected[row%4]);
    CHECK(r.tiles[1].fully_revealed && map_cell_visibility(&r.tiles[1],0,0)==2);
    CHECK(!map_cell_visibility(NULL,0,0) && !map_cell_visibility(&r.tiles[0],32,0));
    release(&f);
}

static void every_read_failure(void) {
    Fixture f; EdenDsmodHostApi h=map_setup(&f); MapProbeResult r;
    map_probe(&h,&f.initial_identity,&r); const unsigned calls=f.calls; release(&f);
    for(unsigned mode=0;mode<2;mode++) for(unsigned n=1;n<=calls;n++) {
        h=map_setup(&f); if(mode)f.map_fail_on=n;else f.fail_on=n;
        memset(&r,0xA5,sizeof r); map_probe(&h,&f.initial_identity,&r); bounded(&f,&r);
        CHECK(!r.exploration_available); release(&f);
    }
}

static void invalid_exploration_keeps_position(void) {
    const struct{uint64_t a,v;size_t n;}cases[]={
        {MAP_ROOT,0,8},{MAP_ROOT,MM+4,8},{MM+0x30,129,4},{MM+0x28,0,8},
        {MN+0x50,MN,8},{MN+0x48,MN,8},{MN+0x58+0x48,0,8},{MN+0x30,33,4},
        {MN+0x30,0,4},{MN+0x34,0,4},{MN+0x3C,2,4},{MN+0x28,0,8},
        {MN+0x28,UINT64_MAX,8},{MN+0x10,0x7FC00000,4},{MN+0x18,0,4},
        {MN+0x1C,0x7F800000,4}
    };
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        Fixture f; EdenDsmodHostApi h=map_setup(&f); put(&f,cases[i].a,cases[i].v,cases[i].n);
        MapProbeResult r; map_probe(&h,&f.initial_identity,&r); bounded(&f,&r);
        CHECK(r.available && r.shared_identity_valid && !r.exploration_available); release(&f);
    }
}

static void invalid_identity_or_position(void) {
    const struct{uint64_t a,v;size_t n;}cases[]={
        {C+0x9A0,0,8},{MG,MP+4,8},{MP+0x100,2,4},{MP+0x168,17,4},
        {MP+0x120,0,8},{MT,UINT64_MAX-7,8},{ACD,ACD_ID+0x10000,4},
        {C+0x800,0,8},{ACD+0xA0,UINT32_MAX,4},{VW+0x38,WORLD+1,4},
        {ACD+0x60,0x7FC00000,4},{ACD+0x64,0x7F800000,4},{ACD+0x68,0x7F7FFFFF,4}
    };
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        Fixture f; EdenDsmodHostApi h=map_setup(&f); put(&f,cases[i].a,cases[i].v,cases[i].n);
        MapProbeResult r; map_probe(&h,&f.initial_identity,&r); bounded(&f,&r); CHECK(!r.available); release(&f);
    }
}

static void changing_world_or_player(void) {
    for(unsigned kind=0;kind<4;kind++) {
        Fixture f; EdenDsmodHostApi h=map_setup(&f);
        trigger.address=ACD+0x60;trigger.on=1;
        f.change_address=kind==0?PLAYER+4:kind==1?ACD:kind==2?ACD+0xA0:C+0x84;
        f.change_value=kind<2?ACD_ID+0x10000:kind==2?WORLD+1:0;f.change_size=4;
        if(kind==2){f.second_address=VW+0x38;f.second_value=WORLD+1;f.second_size=4;}
        MapProbeResult r;map_probe(&h,&f.initial_identity,&r);bounded(&f,&r);
        CHECK(!r.available && !r.shared_identity_valid && trigger.seen);release(&f);
    }
}

static void changing_map(void) {
    const struct{uint64_t a,v;size_t n;}cases[]={
        {MN+4,999,4},{MN+8,WORLD+1,4},{MN+0x10,0x3F800000,4},
        {MN+0x28,MC+256,8},{MN+0x30,8,4},{MN+0x3C,1,4},{MN+0x50,0,8},
        {MAP_ROOT,MM+8,8},{MM+0x30,1,4},{MC,0,1}
    };
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        Fixture f;EdenDsmodHostApi h=map_setup(&f);trigger.address=MN;trigger.on=2;
        f.change_address=cases[i].a;f.change_value=cases[i].v;f.change_size=cases[i].n;
        MapProbeResult r;map_probe(&h,&f.initial_identity,&r);bounded(&f,&r);
        CHECK(r.available && !r.exploration_available);release(&f);
    }
    /* Last-used timestamps and rendering handles do not define map ownership. */
    Fixture f;EdenDsmodHostApi h=map_setup(&f);trigger.address=MN;trigger.on=2;
    f.change_address=MN+0x38;f.change_value=44;f.change_size=4;
    MapProbeResult r;map_probe(&h,&f.initial_identity,&r);CHECK(r.exploration_available);bounded(&f,&r);release(&f);
}

static void bounded_large_map(void) {
    Fixture f;EdenDsmodHostApi h=map_setup(&f);MapProbeResult r;
    for(unsigned i=0;i<128;i++)node(&f,i,i<64?WORLD:WORLD+1,0);
    /* Full-size masks exercise the true worst-case payload, including second pass. */
    for(unsigned i=0;i<64;i++)put32(&f,MN+i*0x58+0x30,32);
    map_probe(&h,&f.initial_identity,&r);bounded(&f,&r);
    CHECK(r.available && r.exploration_available && r.tile_count==64 && r.bytes>50000);
    f.calls=f.maps=0;put32(&f,MN+64*0x58+8,WORLD);
    map_probe(&h,&f.initial_identity,&r);bounded(&f,&r);CHECK(r.available&&!r.exploration_available);
    release(&f);
}

static void invalid_hosts_clear(void) {
    Fixture f;EdenDsmodHostApi h=map_setup(&f);MapProbeResult r;
    memset(&r,0xA5,sizeof r);map_probe(NULL,&f.initial_identity,&r);CHECK(!r.available&&!r.tile_count&&!r.reads);
    map_probe(&h,NULL,&r);CHECK(!r.available&&!r.reads);
    h.main_size=UINT64_MAX;map_probe(&h,&f.initial_identity,&r);CHECK(!r.available&&!r.reads);release(&f);
}

int main(void) {
    base_reader_suite();valid_map();every_read_failure();invalid_exploration_keeps_position();
    invalid_identity_or_position();changing_world_or_player();changing_map();bounded_large_map();invalid_hosts_clear();
    puts("PASS map synthetic bounds, packed visibility, field isolation, transitions and read-failure suite");return 0;
}
