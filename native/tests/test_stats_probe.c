// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic guest memory only. No numbers here are game observations.
#define main base_reader_suite
#include "test_player_probe.c"
#undef main
#include "stats_probe.h"
#include <math.h>
#include <float.h>
#define ACD_GLOBALS UINT64_C(0x7300000000)
#define ACD_POOL UINT64_C(0x7300001000)
#define ACD_TABLE UINT64_C(0x7300002000)
#define ACD_PAGE UINT64_C(0x7300003000)
#define ACD (ACD_PAGE+2*0x360)
#define GROUP UINT64_C(0x7300005000)
#define BUCKETS UINT64_C(0x7300006000)
#define OWNER UINT64_C(0x7300008000)
#define NODES UINT64_C(0x730000A000)
#define ACD_ID UINT32_C(0x56780002)
static const uint32_t keys[4]={0xFFFFF0C9,0xFFFFF0D3,0xFFFFF026,0xFFFFF0B9};
static const uint32_t direct_index[4]={0x36,0x2C,0xD9,0x46};
static const uint32_t shared_index[4]={0x136,0x12C,0x1D9,0x146};
static const float values[4]={1.25f,0.125f,102.5f,1.25f};
static uint32_t bits(float v){uint32_t b;memcpy(&b,&v,4);return b;}
static uint64_t node_address(unsigned field,unsigned node){return NODES+0x100*field+16*node;}
static void stat_memory(Fixture*f,int shared,unsigned count){
 uint64_t bases[8]={ACD_GLOBALS,ACD_POOL,ACD_TABLE,ACD_PAGE,GROUP,BUCKETS,OWNER,NODES};size_t sizes[8]={8,0x170,32,0xD80,0x30,0x800,0x1028,0x400};
 for(unsigned i=0;i<8;i++){f->regions[i+8]=(Region){bases[i],sizes[i],calloc(1,sizes[i])};CHECK(f->regions[i+8].bytes);}
 put32(f,PLAYER+4,ACD_ID);put64(f,C+0x9A0,ACD_GLOBALS);put64(f,ACD_GLOBALS,ACD_POOL);
 put32(f,ACD_POOL+0x100,4);put32(f,ACD_POOL+0x168,2);put64(f,ACD_POOL+0x120,ACD_TABLE);put64(f,ACD_TABLE,ACD_PAGE);put32(f,ACD,ACD_ID);
 put64(f,ACD+0x168,GROUP);put(f,GROUP+4,shared?4:0,1);put64(f,GROUP+0x28,BUCKETS);put64(f,GROUP+0x10,OWNER);
 put64(f,OWNER+0x10,OWNER+0x1C);put32(f,OWNER,0x1FF);put32(f,OWNER+4,4);put32(f,OWNER+0x18,0x200);
 for(unsigned field=0;field<4;field++){
  put64(f,BUCKETS+8*direct_index[field],node_address(field,0));put64(f,OWNER+0x1C+8*shared_index[field],node_address(field,0));
  for(unsigned n=0;n<count;n++){uint64_t a=node_address(field,n);put64(f,a,n+1<count?node_address(field,n+1):0);put32(f,a+8,n+1<count?0x100+n:keys[field]);put32(f,a+12,bits(values[field]));}
 }
}
static EdenDsmodHostApi stat_setup(Fixture*f,int shared,unsigned count){EdenDsmodHostApi h=setup(f);stat_memory(f,shared,count);PlayerProbeResult p;player_probe(&h,&p);CHECK(p.available);f->initial_identity=p.identity;f->calls=f->maps=0;return h;}
static void check_all(const StatsProbeResult*r,int wanted){for(unsigned i=0;i<4;i++)CHECK(r->fields[i].available==wanted);}
// Catches wrong keys/masks, weakened packed-bucket alignment, missing second pass, and payload budget drift.
static void stats_valid(void){for(int mode=0;mode<2;mode++)for(unsigned count=1;count<=8;count+=7){
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,mode,count);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);CHECK(r.shared_identity_valid);check_all(&r,1);
 for(unsigned i=0;i<4;i++)CHECK(r.fields[i].raw==values[i]);
 const unsigned want_reads=(mode?56:54)+8*count, want_bytes=(mode?410:394)+128*count;
 CHECK(r.reads==want_reads&&r.bytes==want_bytes&&f.calls==r.reads&&f.maps==r.reads);CHECK(r.reads<=120&&r.bytes<=1434&&f.writes==0);release(&f);
}}
// Catches returning early after a field error and accidentally retaining old data or losing unrelated stats.
static void field_independence(void){for(unsigned field=0;field<4;field++)for(unsigned kind=0;kind<6;kind++){
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);uint64_t a=node_address(field,0);
 if(kind==0)put64(&f,OWNER+0x1C+8*shared_index[field],0);
 if(kind==1)put32(&f,a+8,12345);
 if(kind==2)put32(&f,a+12,0x7FC00000);
 if(kind==3){put32(&f,a+8,12345);put64(&f,a,a);}
 if(kind==4)put64(&f,OWNER+0x1C+8*shared_index[field],a+4);
 if(kind==5)put64(&f,OWNER+0x1C+8*shared_index[field],UINT64_MAX-7);
 StatsProbeResult r;memset(&r,0xA5,sizeof r);stats_probe(&h,&f.initial_identity,&r);CHECK(r.shared_identity_valid);
 for(unsigned i=0;i<4;i++){CHECK(r.fields[i].available==(i!=field));if(i==field)CHECK(r.fields[i].raw==0&&strstr(r.fields[i].reason,"UNAVAILABLE"));}
 CHECK(f.calls>32&&f.writes==0);release(&f);
}}
// Catches every guest read/map failure being ignored. Successful fields must never use a failed access.
static void failure_positions(void){for(unsigned mode=0;mode<2;mode++)for(unsigned n=1;n<=120;n++){
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,8);if(mode)f.map_fail_on=n;else f.fail_on=n;
 StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);
 // Before the injected failure, all reads succeeded, so its owning position is exact.
 const int shared=n<=24||n>=97;
 CHECK(r.shared_identity_valid==!shared);
 if(shared)check_all(&r,0);
 else{const unsigned field=((n-25)%36)/9;for(unsigned i=0;i<4;i++)CHECK(r.fields[i].available==(i!=field));}
 CHECK(r.reads<=120&&r.bytes<=1434&&f.writes==0);release(&f);
}}
static void cache_limits_dirty(void){
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,9);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);CHECK(r.shared_identity_valid);check_all(&r,0);CHECK(r.reads==120&&r.bytes==1434);release(&f);
 for(unsigned mode=0;mode<2;mode++){h=stat_setup(&f,mode,1);put(&f,GROUP+4,mode?6:2,1);stats_probe(&h,&f.initial_identity,&r);CHECK(r.shared_identity_valid);check_all(&r,0);CHECK(r.reads==44&&r.bytes==314);for(unsigned i=0;i<4;i++)CHECK(strstr(r.fields[i].reason,"dirty"));release(&f);}
}
// Catches dropped ACD generation/range/alignment guards and accepting invalid hosts.
static void invalid_shared_states(void){
 const struct{uint64_t address,value;size_t size;}cases[]={
 {C+0x9A0,0,8},{C+0x9A0,ACD_GLOBALS+4,8},{ACD_GLOBALS,UINT64_MAX-7,8},
 {ACD_POOL+0x100,2,4},{ACD_POOL+0x168,17,4},{ACD_POOL+0x120,0,8},{ACD_TABLE,UINT64_MAX-7,8},
 {ACD,ACD_ID+0x10000,4},{ACD+0x168,0,8},{ACD+0x168,GROUP+4,8},{GROUP+0x10,0,8},
 {OWNER+0x10,0,8},{OWNER+0x10,OWNER+0x1E,8}};
 for(unsigned c=0;c<sizeof cases/sizeof cases[0];c++){Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);put(&f,cases[c].address,cases[c].value,cases[c].size);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);CHECK(!r.shared_identity_valid);check_all(&r,0);CHECK(r.reads<=120&&r.bytes<=1434&&!f.writes);release(&f);}
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);StatsProbeResult r;
 stats_probe(NULL,&f.initial_identity,&r);CHECK(!r.shared_identity_valid&&!f.calls);stats_probe(&h,NULL,&r);CHECK(!r.shared_identity_valid&&!f.calls);
 h.main_size=UINT64_MAX;stats_probe(&h,&f.initial_identity,&r);CHECK(!r.shared_identity_valid&&!f.calls);release(&f);
}
// Catches the previous early-failure hazard: a missing field must not skip final actor/player identity checks.
static void field_failure_then_identity_change(void){for(unsigned kind=0;kind<3;kind++){
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);put64(&f,OWNER+0x1C+8*shared_index[0],0);
 f.mutate_on=32;f.change_address=kind==0?C+0x84:kind==1?PLAYER+4:ACD;f.change_value=kind==0?0:ACD_ID+0x10000;f.change_size=4;
 StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);CHECK(!r.shared_identity_valid);check_all(&r,0);release(&f);
}}
static void snapshot_changes(void){
 const struct{uint64_t address,value;size_t size;int shared;}cases[]={
 {C+0x84,0,4,1},{ACD_POOL+0x100,5,4,1},{ACD,ACD_ID+0x10000,4,1},{ACD+0x168,GROUP+8,8,1},{GROUP+4,6,1,1},{OWNER+0x10,BUCKETS,8,1},
 {NODES+12,0x3F800000,4,0},{NODES+8,42,4,0},{NODES,NODES+16,8,0},{OWNER+0x1C+8*0x136,NODES+16,8,0}};
 for(unsigned c=0;c<sizeof cases/sizeof cases[0];c++){Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);f.mutate_on=33;f.change_address=cases[c].address;f.change_value=cases[c].value;f.change_size=cases[c].size;StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);if(cases[c].shared){CHECK(!r.shared_identity_valid);check_all(&r,0);}else{CHECK(r.shared_identity_valid&&!r.fields[0].available);for(unsigned i=1;i<4;i++)CHECK(r.fields[i].available);}release(&f);}
}
// Address-driven regression: a late field failure must be followed by shared validation,
// independent of traversal order or how many earlier reads were needed.
static struct {uint64_t target;unsigned seen,kind,triggered,identity_failed;} late;
static EdenDsmodBool late_failure_read(void*p,uint64_t a,void*out,size_t n){
 Fixture*f=p;
 if(a==late.target&&++late.seen==2){
  late.triggered=1;f->fail_on=f->calls+1;
  if(late.kind<3)put32(f,late.kind==0?C+0x84:late.kind==1?PLAYER+4:ACD,late.kind==0?0:ACD_ID+0x10000);
 }else if(late.triggered&&late.kind==3&&a==MAIN+0x114A840){f->fail_on=f->calls+1;late.identity_failed=1;}
 return read_mem(p,a,out,n);
}
static void late_field_failure_then_identity(void){for(unsigned field=0;field<4;field++)for(unsigned where=0;where<2;where++)for(unsigned kind=0;kind<4;kind++){
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);memset(&late,0,sizeof late);late.target=where?node_address(field,0):OWNER+0x1C+8*shared_index[field];late.kind=kind;h.read_memory=late_failure_read;
 StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);CHECK(late.triggered);CHECK(!r.shared_identity_valid);check_all(&r,0);if(kind==3)CHECK(late.identity_failed);CHECK(r.reads<=120&&r.bytes<=1434&&!f.writes);release(&f);
}}
// Hand-derived expected text catches fraction-versus-percent mistakes, ties-away rounding,
// percent-before-precision, fake zeros, and undefined/out-of-range integer conversion.
static void formatting(void){
 const struct{unsigned field;float input;const char*want;}cases[]={
 {0,1.25f,"1.25"},{1,0.125f,"12.50%"},{1,0.0f,"0.00%"},{2,102.5f,"102"},{2,103.5f,"104"},{2,-2.5f,"-2"},{2,0,"0"},
 {3,1.25f,"+25.00%"},{3,1.5f,"+50.0%"},{3,1.0f,"+0%"},{3,0.875f,"-12%"}};
 char text[128];for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++){CHECK(stats_format(cases[i].field,cases[i].input,text,sizeof text));CHECK(!strcmp(text,cases[i].want));}
 CHECK(!stats_format(0,NAN,text,sizeof text));CHECK(!stats_format(1,FLT_MAX,text,sizeof text));CHECK(!stats_format(2,8388608.0f,text,sizeof text));CHECK(!stats_format(3,FLT_MAX,text,sizeof text));CHECK(!stats_format(4,1,text,sizeof text));CHECK(!stats_format(0,1,text,1));
 Fixture f;EdenDsmodHostApi h=stat_setup(&f,1,1);put32(&f,node_address(1,0)+12,bits(FLT_MAX));StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);CHECK(r.shared_identity_valid&&!r.fields[1].available);CHECK(r.fields[0].available&&r.fields[2].available&&r.fields[3].available);release(&f);
}
static int stat_suite(void){base_reader_suite();late_field_failure_then_identity();stats_valid();field_independence();failure_positions();cache_limits_dirty();invalid_shared_states();field_failure_then_identity_change();snapshot_changes();formatting();puts("PASS stats synthetic route/cache/field isolation/formatting suite");return 0;}
#ifndef STATS_EMBED
int main(void){return stat_suite();}
#endif
