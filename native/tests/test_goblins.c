// SPDX-License-Identifier: GPL-3.0-or-later
// Entirely synthetic memory. These fixtures are not observations of the game.
#define main base_reader_suite
#include "test_player_probe.c"
#undef main
#include "goblins_probe.h"
#include <math.h>

#define MEM UINT64_C(0x7500000000)
#define MG (MEM+0x0000)
#define AP (MEM+0x0100)
#define AT (MEM+0x0300)
#define AC (MEM+0x1000)
#define ACD (AC+2*0x360)
#define ENEMY (AC+1*0x360)
#define MAP (MEM+0x3000)
#define LIST (MEM+0x3100)
#define ALLOC (MEM+0x3200)
#define BLOCK (MEM+0x3300)
#define NODE (BLOCK+0x50)
#define VW (MEM+0x4000)
#define IDS (MEM+0x5000)
#define RI (MEM+0x15000)
#define RM_A (MEM+0x16000)
#define RM_M (MEM+0x17000)
#define RP_A (MEM+0x18000)
#define RP_M (MEM+0x19000)
#define RR_A (MEM+0x1A000)
#define RR_M (MEM+0x1B000)
#define RA (MEM+0x1C000)
#define RM (MEM+0x1D000)
#define GROUP (MEM+0x1E000)
#define BUCKETS (MEM+0x20000)
#define KEYNODE (MEM+0x21000)
#define CLIENT_ENEMY (PAGE+1*0x410)
#define WORLD UINT32_C(0xAABBCCDD)
#define ACD_ID UINT32_C(0x76540002)
#define ENEMY_ID UINT32_C(0x87650001)
#define CLIENT_ID UINT32_C(0x789A0001)
#define GENERIC_ID UINT32_C(0x34560005)

static struct {uint64_t address; unsigned seen,on;} change;
static void putf(Fixture *f,uint64_t a,float v){uint32_t b;memcpy(&b,&v,4);put32(f,a,b);}
static EdenDsmodBool goblin_read(void *p,uint64_t a,void *out,size_t n){
    Fixture *f=p;f->calls++;CHECK(n<=64);
    if(change.address==a && ++change.seen==change.on)put(f,f->change_address,f->change_value,f->change_size);
    if(f->fail_on==f->calls)return 0;
    for(unsigned i=0;i<17;i++){Region *r=&f->regions[i];if(a>=r->base&&a-r->base<=r->size&&n<=r->size-(a-r->base)){memcpy(out,r->bytes+(a-r->base),n);return 1;}}
    CHECK(0);return 0;
}
static void manager(Fixture*f,uint64_t a,uint64_t pool,uint64_t records,uint32_t group,uint32_t size,uint64_t vt){
    put64(f,a,MAIN+vt);put64(f,a+0x20,pool);put32(f,a+0x50,group);put32(f,a+0x7C,size);
    put32(f,pool+0x100,4);put32(f,pool+0x104,16);put64(f,pool+0x120,records);
}
static EdenDsmodHostApi goblin_setup(Fixture *f,MapProbeResult *m){
    EdenDsmodHostApi h=setup(f);
    const uint64_t bases[]={MEM,MAIN+0x1878FE8,MAIN+0x114B778,MAIN+0x1150CA8,MAIN+0x11572E8};
    const size_t sizes[]={0x23000,8,8,8,16};
    for(unsigned i=0;i<5;i++){f->regions[8+i]=(Region){bases[i],sizes[i],calloc(1,sizes[i])};CHECK(f->regions[8+i].bytes);}
    put32(f,PLAYER+4,ACD_ID);put64(f,C+0x9A0,MG);put64(f,MG,AP);put64(f,MG+8,IDS);
    put32(f,AP+0x100,4);put32(f,AP+0x168,2);put64(f,AP+0x120,AT);put64(f,AT,AC);
    put32(f,ACD,ACD_ID);put32(f,ACD+0x10,0x11110006);put32(f,ACD+0xA0,WORLD);put32(f,IDS+6*4,ACD_ID);
    put64(f,C+0x800,VW);put32(f,VW+0x38,WORLD);
    put64(f,MAIN+0x1878FE8,MAP);put64(f,MAP,LIST);put64(f,LIST,NODE);put64(f,LIST+8,NODE);put32(f,LIST+0x10,1);put64(f,LIST+0x18,ALLOC);put32(f,LIST+0x20,1);
    put32(f,ALLOC,24);put32(f,ALLOC+8,1);put64(f,ALLOC+0x10,BLOCK);
    put64(f,BLOCK+8,NODE);put32(f,BLOCK+0x18,8);put32(f,BLOCK+0x1C,24);put32(f,BLOCK+0x20,1);put32(f,BLOCK+0x2C,7);put32(f,BLOCK+0x38,0x600DF00D);
    put32(f,NODE,GENERIC_ID);put32(f,IDS+5*4,ENEMY_ID);
    put32(f,ENEMY,ENEMY_ID);put32(f,ENEMY+0x10,GENERIC_ID);put32(f,ENEMY+0x14,CLIENT_ID);put32(f,ENEMY+0x18,100);
    put32(f,ENEMY+0xA0,WORLD);put32(f,ENEMY+0x148,UINT32_MAX);put32(f,ENEMY+0x14C,1);put(f,ENEMY+0x160,0x38,2);put64(f,ENEMY+0x168,GROUP);
    putf(f,ENEMY+0x60,25);putf(f,ENEMY+0x64,30);
    put32(f,CLIENT_ENEMY,CLIENT_ID);put32(f,CLIENT_ENEMY+8,ENEMY_ID);put32(f,CLIENT_ENEMY+0xC,100);putf(f,CLIENT_ENEMY+0x30,26);putf(f,CLIENT_ENEMY+0x34,31);
    put64(f,MAIN+0x114B778,MEM+0x40);put64(f,MEM+0x40,RM_A);
    put64(f,MAIN+0x1150CA8,MEM+0x48);put64(f,MEM+0x48,RM_M);
    put64(f,MAIN+0x11572E8,MEM+0x30);put32(f,MEM+0x30,256);put64(f,MAIN+0x11572F0,MEM+0x38);put64(f,MEM+0x38,RI);
    manager(f,RM_A,RP_A,RR_A,1,0x1C0,0x10C5550);manager(f,RM_M,RP_M,RR_M,25,0x558,0x10C6DB8);
    put32(f,RI+100*4,0x40001);put32(f,RI+200*4,0x80001);
    put32(f,RR_A+16,0x40001);put(f,RR_A+16+7,1,1);put64(f,RR_A+16+8,RA);
    put32(f,RR_M+16,0x80001);put(f,RR_M+16+7,25,1);put64(f,RR_M+16+8,RM);
    put32(f,RA,100);put32(f,RA+8,0x40);put32(f,RA+0x10,1);put32(f,RA+0x74,200);
    put32(f,RM,200);put32(f,RM+8,0x40);put32(f,RM+0x1C,11);
    put64(f,GROUP+0x28,BUCKETS);put64(f,BUCKETS+0xEB*8,KEYNODE);put32(f,KEYNODE+8,0x1EB);put32(f,KEYNODE+12,1);
    h.main_size=0x2000000;h.read_memory=goblin_read;
    PlayerProbeResult p;player_probe(&h,&p);CHECK(p.available);f->initial_identity=p.identity;
    *m=(MapProbeResult){.available=1,.shared_identity_valid=1,.exploration_available=1,.world_id=WORLD,.tile_count=1};
    m->tiles[0]=(MapExplorationTile){.min_x=0,.min_y=0,.max_x=100,.max_y=100,.columns=10,.rows=10,.fully_revealed=1};
    f->calls=f->maps=0;memset(&change,0,sizeof change);return h;
}
static void bounded(Fixture*f,MarkersProbeResult*r){CHECK(r->reads==f->calls&&r->reads<=GOBLINS_MAX_READS&&r->bytes<=GOBLINS_MAX_BYTES&&!f->writes);if(!r->available)CHECK(!r->count&&!r->world_id&&!r->shared_identity_valid);}
static void visible_goblin_only(void){
    Fixture f;MapProbeResult m;EdenDsmodHostApi h=goblin_setup(&f,&m);MarkersProbeResult r;
    goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);
    CHECK(r.available&&r.shared_identity_valid&&r.count==1&&r.world_id==WORLD);
    CHECK(r.items[0].id==GENERIC_ID&&r.items[0].kind==MARKER_GOBLIN&&r.items[0].x==26&&r.items[0].y==31);release(&f);
}
static void never_show_hidden_wrong_world_or_non_goblin(void){
    const struct{uint64_t a,v;size_t n;} cases[]={
        {ENEMY+0xA0,WORLD+1,4},{ENEMY+0x48,1,4},{ENEMY+0x160,0xB8,2},{ENEMY+0x160,0,2},{CLIENT_ENEMY+0x1CC,4,1},{KEYNODE+12,0,4},{GROUP+4,2,1},{RM+0x1C,12,4},{ENEMY+0x14C,2,4}
    };
    for(unsigned i=0;i<sizeof cases/sizeof *cases;i++){Fixture f;MapProbeResult m;EdenDsmodHostApi h=goblin_setup(&f,&m);put(&f,cases[i].a,cases[i].v,cases[i].n);MarkersProbeResult r;goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(!r.count);release(&f);}
    Fixture f;MapProbeResult m;EdenDsmodHostApi h=goblin_setup(&f,&m);m.tiles[0].fully_revealed=0;MarkersProbeResult r;goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(!r.count);release(&f);
}
static void reject_corrupt_owners_and_resources(void){
    const struct{uint64_t a,v;size_t n;} cases[]={
        {LIST+0x10,GOBLINS_MAX_NODES+1,4},{NODE+0x10,NODE,8},{NODE+8,NODE,8},{LIST+8,NODE+24,8},{ALLOC,16,4},{BLOCK+0x1C,16,4},{BLOCK+0x38,0,4},{IDS+5*4,ENEMY_ID+0x10000,4},{ENEMY+0x10,GENERIC_ID+1,4},{CLIENT_ENEMY+8,ACD_ID,4},{RM_A+0x50,25,4},{RA+8,0,4},{RM+8,0x42,4},{RI+200*4,UINT32_MAX,4},{RR_M+16,0x80002,4},{RM,201,4},{CLIENT_ENEMY+0x30,0x7FC00000,4}
    };
    for(unsigned i=0;i<sizeof cases/sizeof *cases;i++){Fixture f;MapProbeResult m;EdenDsmodHostApi h=goblin_setup(&f,&m);put(&f,cases[i].a,cases[i].v,cases[i].n);MarkersProbeResult r;goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(!r.available);release(&f);}
}
static void mutations_and_failed_reads_clear(void){
    Fixture f;MapProbeResult m;EdenDsmodHostApi h=goblin_setup(&f,&m);MarkersProbeResult r;goblins_probe(&h,&f.initial_identity,&m,&r);unsigned reads=f.calls;release(&f);
    for(unsigned mode=0;mode<2;mode++)for(unsigned i=1;i<=reads;i++){h=goblin_setup(&f,&m);if(mode)f.map_fail_on=i;else f.fail_on=i;goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(!r.available);release(&f);}
    const struct{uint64_t a,v;size_t n;} changes[]={ {PLAYER+4,ACD_ID+1,4},{ENEMY,ENEMY_ID+0x10000,4},{ENEMY+0xA0,WORLD+1,4},{CLIENT_ENEMY+8,ACD_ID,4},{RA+0x74,201,4},{RM+0x1C,12,4},{KEYNODE+12,0,4},{NODE+0x10,NODE,8},{LIST+0x10,0,4},{MAIN+0x1878FE8,MAP+8,8} };
    for(unsigned i=0;i<sizeof changes/sizeof *changes;i++){h=goblin_setup(&f,&m);change.address=CLIENT_ENEMY+0x30;change.on=1;f.change_address=changes[i].a;f.change_value=changes[i].v;f.change_size=changes[i].n;goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(change.seen&&!r.available);release(&f);}
}
static void exploration_empty_and_volatile_fields(void){
    Fixture f;MapProbeResult m;MarkersProbeResult r;EdenDsmodHostApi h=goblin_setup(&f,&m);
    m.tiles[0].fully_revealed=0;m.tiles[0].cells[5]=3; // (column 2,row 3) is pair 23.
    goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(r.available&&r.count==1);release(&f);
    h=goblin_setup(&f,&m);put32(&f,LIST+0x10,0);put64(&f,LIST,0);put64(&f,LIST+8,0);put64(&f,MAIN+0x114B778,0);
    goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(r.available&&!r.count);release(&f);
    const uint64_t volatile_fields[]={RA+4,RR_A+16+4};
    for(unsigned i=0;i<2;i++){h=goblin_setup(&f,&m);change.address=CLIENT_ENEMY+0x30;change.on=1;f.change_address=volatile_fields[i];f.change_value=7;f.change_size=3;
        goblins_probe(&h,&f.initial_identity,&m,&r);bounded(&f,&r);CHECK(r.available&&r.count==1);release(&f);}
}
int main(void){visible_goblin_only();never_show_hidden_wrong_world_or_non_goblin();reject_corrupt_owners_and_resources();mutations_and_failed_reads_clear();exploration_empty_and_volatile_fields();puts("PASS synthetic goblin reader: identity, classification, visibility, exploration, bounded failures; no writes");return 0;}
