// SPDX-License-Identifier: GPL-3.0-or-later
// All memory addresses, handles and levels here are synthetic fixtures, not game observations.
#include "player_probe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#x);exit(1);}}while(0)
#define MAIN UINT64_C(0x7100000000)
#define G UINT64_C(0x7200000000)
#define C UINT64_C(0x7200001000)
#define SEL UINT64_C(0x7200003000)
#define PLAYERS UINT64_C(0x7200010000)
#define PLAYER (PLAYERS+0x60)
#define POOL UINT64_C(0x7200060000)
#define TABLE UINT64_C(0x7200061000)
#define PAGE UINT64_C(0x7200062000)
#define ACTOR (PAGE+3*0x410)
#define HANDLE UINT32_C(0x12340003)
typedef struct Region { uint64_t base;size_t size;unsigned char *bytes; } Region;
typedef struct Fixture { Region regions[17];unsigned calls,maps,writes,fail_on,map_fail_on,mutate_on; uint64_t change_address,change_value;size_t change_size;uint64_t second_address,second_value;size_t second_size;PlayerProbeIdentity initial_identity; int64_t level,available,hp_available,fraction_available;char text[160],status[160],hp_text[160],hp_status[160],fraction_text[160],diag_stage[160],diag_flags[160],diag_route[4][256],diag_access[160]; unsigned begins,ends,guest_calls; } Fixture;
static void put(Fixture*f,uint64_t a,uint64_t v,size_t n){for(unsigned i=0;i<17;i++){Region*r=&f->regions[i];if(a>=r->base&&a-r->base<=r->size&&n<=r->size-(a-r->base)){memcpy(r->bytes+(a-r->base),&v,n);return;}}CHECK(0);}
static void put32(Fixture*f,uint64_t a,uint32_t v){put(f,a,v,4);}
static void put64(Fixture*f,uint64_t a,uint64_t v){put(f,a,v,8);}
static EdenDsmodBool mapped(void*p,uint64_t a,uint64_t n){Fixture*f=p;f->maps++;if(f->map_fail_on==f->maps)return 0;for(unsigned i=0;i<17;i++){Region*r=&f->regions[i];if(a>=r->base&&a-r->base<=r->size&&n<=r->size-(a-r->base))return 1;}return 0;}
static EdenDsmodBool read_mem(void*p,uint64_t a,void*out,size_t n){Fixture*f=p;f->calls++;CHECK(n<=20);if(f->mutate_on==f->calls){put(f,f->change_address,f->change_value,f->change_size);if(f->second_size)put(f,f->second_address,f->second_value,f->second_size);}if(f->fail_on==f->calls)return 0;for(unsigned i=0;i<17;i++){Region*r=&f->regions[i];if(a>=r->base&&a-r->base<=r->size&&n<=r->size-(a-r->base)){memcpy(out,r->bytes+(a-r->base),n);return 1;}}CHECK(0);return 0;}
static EdenDsmodBool write_mem(void*p,uint64_t a,const void*v,size_t n){(void)a;(void)v;(void)n;((Fixture*)p)->writes++;return 1;}
static EdenDsmodHostApi setup(Fixture*f){
 memset(f,0,sizeof*f);uint64_t bases[8]={MAIN+0x114A840,G,C,SEL,PLAYERS,POOL,TABLE,PAGE};size_t sizes[8]={8,0x20,0xBB0,20,0xEC58,0x170,32,0x1040};
 for(unsigned i=0;i<8;i++){f->regions[i]=(Region){bases[i],sizes[i],calloc(1,sizes[i])};CHECK(f->regions[i].bytes);}
 put64(f,MAIN+0x114A840,G);put64(f,G+0x10,C);put32(f,C+0x84,1);put64(f,C+0xBA0,SEL);put64(f,C+0x948,PLAYERS);put32(f,SEL,0);for(unsigned i=1;i<4;i++)put32(f,SEL+4*i,UINT32_MAX);put32(f,SEL+16,1);
 put32(f,PLAYER,0);put32(f,PLAYER+4,123);put32(f,PLAYER+8,HANDLE);put32(f,PLAYER+0xD68C,42);
 put64(f,C+0xA98,POOL);put32(f,POOL+0x100,4);put32(f,POOL+0x168,2);put64(f,POOL+0x120,TABLE);put64(f,TABLE,PAGE);put32(f,ACTOR,HANDLE);
 EdenDsmodHostApi h={.userdata=f,.main_base=MAIN,.main_size=0x1200000,.is_mapped=mapped,.read_memory=read_mem,.write_memory=write_mem};return h;
}
static void release(Fixture*f){for(unsigned i=0;i<17;i++)free(f->regions[i].bytes);}
static void valid(void){Fixture f;EdenDsmodHostApi h=setup(&f);PlayerProbeResult r;player_probe(&h,&r);CHECK(r.available&&r.level==42);CHECK(r.reads==27&&r.bytes==212);CHECK(f.calls==27&&f.maps==27&&f.writes==0);release(&f);}
static void failures_clear(void){for(unsigned n=1;n<=27;n++){Fixture f;EdenDsmodHostApi h=setup(&f);f.fail_on=n;PlayerProbeResult r={.available=1,.level=42};player_probe(&h,&r);CHECK(!r.available&&r.level==0);CHECK(f.calls==n&&r.reads==n&&r.bytes<=212&&f.writes==0);release(&f);}}
static void invalid_states(void){
 const struct{uint64_t address,value;size_t size;}cases[]={
 {MAIN+0x114A840,0,8},{MAIN+0x114A840,G+1,8},{G+0x10,UINT64_MAX-3,8},{C+0x84,0,4},{C+0x84,UINT32_MAX,4},{C+0xBA0,0,8},{C+0x948,0,8},{C+0x948,UINT64_MAX-7,8},
 {SEL+16,0,4},{SEL+16,2,4},{SEL+4,0,4},{SEL,4,4},{SEL,UINT32_MAX,4},{SEL+4,UINT32_MAX-1,4},
 {PLAYER,1,4},{PLAYER+4,UINT32_MAX,4},{PLAYER+8,UINT32_MAX,4},{C+0xA98,0,8},{POOL+0x100,3,4},{POOL+0x168,17,4},{POOL+0x120,0,8},{TABLE,0,8},{ACTOR,0,4},{PLAYER+0xD68C,0,4},{PLAYER+0xD68C,71,4}};
 for(size_t i=0;i<sizeof cases/sizeof cases[0];i++){Fixture f;EdenDsmodHostApi h=setup(&f);put(&f,cases[i].address,cases[i].value,cases[i].size);PlayerProbeResult r={.available=1,.level=42};player_probe(&h,&r);if(r.available)fprintf(stderr,"case %zu\n",i);CHECK(!r.available&&r.level==0&&r.reads<=27&&r.bytes<=212&&f.writes==0);release(&f);}
}
static void no_stale_snapshot(void){
 const struct{uint64_t address,value;size_t size;}changes[]={{MAIN+0x114A840,0,8},{C+0x84,0,4},{C+0x84,UINT32_MAX,4},{SEL+16,2,4},{PLAYER+4,124,4},{PLAYER+8,UINT32_MAX,4},{POOL+0x100,5,4},{ACTOR,0,4}};
 for(size_t i=0;i<sizeof changes/sizeof changes[0];i++){Fixture f;EdenDsmodHostApi h=setup(&f);f.mutate_on=15;f.change_address=changes[i].address;f.change_value=changes[i].value;f.change_size=changes[i].size;PlayerProbeResult r;player_probe(&h,&r);CHECK(!r.available&&r.level==0&&r.reads<=27);release(&f);}
}
static void invalid_main(void){Fixture f;EdenDsmodHostApi h=setup(&f);PlayerProbeResult r;h.main_size=0x114A847;player_probe(&h,&r);CHECK(!r.available&&f.calls==0);h.main_size=UINT64_MAX;player_probe(&h,&r);CHECK(!r.available&&f.calls==0);release(&f);}
int main(void){valid();failures_clear();invalid_states();no_stale_snapshot();invalid_main();puts("PASS synthetic player reader: bounded route, startup/liveness gates, consistency, failures clear; no writes");return 0;}
