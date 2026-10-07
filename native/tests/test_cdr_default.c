// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic runtime registration values are deliberately nonzero; no game value is embedded.
#ifndef CDR_EMBED
#define STATS_EMBED
#include "test_stats_probe.c"
#endif
#define CDR_ENTRY (MAIN+UINT64_C(0x1921F98))
static unsigned cdr_reads;
static EdenDsmodBool cdr_read(void*p,uint64_t a,void*out,size_t n){if(a==CDR_ENTRY){CHECK(n==8);cdr_reads++;}return read_mem(p,a,out,n);}
static EdenDsmodBool cdr_mapped(void*p,uint64_t a,uint64_t n){if(a>=MAIN+0x191EAD8&&a<MAIN+0x195EAD8)CHECK(a==CDR_ENTRY&&n==8);return mapped(p,a,n);}
static uint32_t collision_key(unsigned n){const uint32_t high=(n+1)*0x1000u;return high|(((high>>12)^0x12Cu)&0x1FFu);}
static void cdr_memory(Fixture*f,EdenDsmodHostApi*h,unsigned shared,unsigned count){
 CHECK(count<=8);CHECK(!f->regions[16].bytes);cdr_reads=0;h->read_memory=cdr_read;h->is_mapped=cdr_mapped;
 f->regions[16]=(Region){CDR_ENTRY,8,calloc(1,8)};CHECK(f->regions[16].bytes);
 put32(f,CDR_ENTRY,0xD3);put32(f,CDR_ENTRY+4,bits(0.375f));h->main_size=0x1AA6000;
 const uint64_t head=shared?OWNER+0x1C+8*shared_index[1]:BUCKETS+8*direct_index[1];
 put64(f,head,count?node_address(1,0):0);
 for(unsigned i=0;i<count;i++){uint64_t node=node_address(1,i);put64(f,node,i+1<count?node_address(1,i+1):0);put32(f,node+8,collision_key(i));put32(f,node+12,bits(0.375f));}
}
static EdenDsmodHostApi cdr_setup(Fixture*f,unsigned shared,unsigned count){EdenDsmodHostApi h=stat_setup(f,shared,8);cdr_memory(f,&h,shared,count);return h;}
static void cdr_present(StatsProbeResult*r,float value){CHECK(r->shared_identity_valid);check_all(r,1);CHECK(r->fields[1].raw==value);}
static void cdr_only_clear(StatsProbeResult*r){CHECK(r->shared_identity_valid&&!r->fields[1].available&&!r->fields[1].raw);for(unsigned i=0;i<4;i++)if(i!=1)CHECK(r->fields[i].available);}
static void cdr_absence_modes(void){for(unsigned mode=0;mode<2;mode++)for(unsigned count=0;count<=8;count++){
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,mode,count);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);cdr_present(&r,0.375f);
 const unsigned common=mode?48:46,bytes=mode?346:330;
 CHECK(r.reads==common+2*(29+count)&&r.bytes==bytes+2*(424+16*count));CHECK(f.calls==r.reads&&f.maps==r.reads&&cdr_reads==2&&!f.writes);
 put32(&f,CDR_ENTRY+4,bits(0.0625f));stats_probe(&h,&f.initial_identity,&r);cdr_present(&r,0.0625f);release(&f);
}}
static void cdr_failure_kinds(void){for(unsigned mode=0;mode<2;mode++)for(unsigned kind=0;kind<11;kind++){
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,mode,8);StatsProbeResult r;
 if(kind==0)put64(&f,node_address(1,7),node_address(1,0));
 if(kind==1)put64(&f,node_address(1,7),node_address(1,7)+16); // ninth distinct node beyond policy
 if(kind==2)put64(&f,node_address(1,2),node_address(1,1));
 if(kind==3)put64(&f,node_address(1,2),node_address(1,3)+4);
 if(kind==4)put64(&f,node_address(1,2),UINT64_MAX-7);
 if(kind==5)put32(&f,CDR_ENTRY,0);
 if(kind==6)put32(&f,CDR_ENTRY,0xD2);
 if(kind==7)put32(&f,CDR_ENTRY+4,UINT32_C(0x7FC00000));
 if(kind==8)put32(&f,CDR_ENTRY+4,UINT32_C(0x7F800000));
 if(kind==9)put32(&f,CDR_ENTRY+4,UINT32_C(0xFF800000));
 if(kind==10)put32(&f,CDR_ENTRY+4,bits(FLT_MAX));
 stats_probe(&h,&f.initial_identity,&r);cdr_only_clear(&r);CHECK(cdr_reads==(kind<=4?0u:2u));CHECK(r.reads<=122&&r.bytes<=1450&&!f.writes);release(&f);
}}
static void cdr_no_other_defaults(void){for(unsigned field=0;field<4;field++)if(field!=1){
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,1,0);StatsProbeResult r;put64(&f,OWNER+0x1C+8*shared_index[field],0);stats_probe(&h,&f.initial_identity,&r);
 CHECK(r.shared_identity_valid&&r.fields[1].available&&!r.fields[field].available&&!r.fields[field].raw);release(&f);
}}
static void cdr_extent_and_dirty(void){
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,1,0);StatsProbeResult r;h.main_size=0x1921F9F;stats_probe(&h,&f.initial_identity,&r);cdr_only_clear(&r);
 h.main_size=0x1921FA0;stats_probe(&h,&f.initial_identity,&r);cdr_present(&r,0.375f);
 cdr_reads=0;put(&f,GROUP+4,6,1);stats_probe(&h,&f.initial_identity,&r);CHECK(r.shared_identity_valid);check_all(&r,0);CHECK(r.reads==44&&cdr_reads==0);release(&f);
}
static void cdr_snapshot_changes(void){for(unsigned kind=0;kind<8;kind++){
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,1,8);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);cdr_present(&r,0.375f);f.calls=f.maps=0;
 f.mutate_on=62;f.change_size=4;
 if(kind==0){f.change_address=CDR_ENTRY;f.change_value=0xD2;}
 if(kind==1){f.change_address=CDR_ENTRY+4;f.change_value=bits(0.5f);}
 if(kind==2){put32(&f,CDR_ENTRY+4,0);f.change_address=CDR_ENTRY+4;f.change_value=UINT32_C(0x80000000);}
 if(kind==3){f.change_address=node_address(1,7)+8;f.change_value=keys[1];}
 if(kind==4){f.change_address=node_address(1,7)+12;f.change_value=bits(0.25f);}
 if(kind==5){f.change_address=OWNER+0x1C+8*shared_index[1];f.change_value=0;f.change_size=8;}
 if(kind==6){f.change_address=node_address(1,4);f.change_value=0;f.change_size=8;}
 if(kind==7){f.change_address=CDR_ENTRY+4;f.change_value=UINT32_C(0x7FC00000);}
 stats_probe(&h,&f.initial_identity,&r);cdr_only_clear(&r);release(&f);
}
 // Present -> absent must reject even when cached and default bits agree.
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,1,8);put32(&f,node_address(1,7)+8,keys[1]);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);cdr_present(&r,0.375f);f.calls=f.maps=0;
 f.mutate_on=61;f.change_address=node_address(1,7)+8;f.change_value=collision_key(7);f.change_size=4;
 stats_probe(&h,&f.initial_identity,&r);cdr_only_clear(&r);release(&f);
}
static void cdr_late_shared_changes(void){for(unsigned kind=0;kind<3;kind++){
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,1,8);StatsProbeResult r;stats_probe(&h,&f.initial_identity,&r);cdr_present(&r,0.375f);f.calls=f.maps=0;
 f.mutate_on=81;f.change_size=4;f.change_address=kind==0?C+0x84:kind==1?PLAYER+4:ACD;f.change_value=kind==0?0:ACD_ID+0x10000;
 stats_probe(&h,&f.initial_identity,&r);CHECK(!r.shared_identity_valid);check_all(&r,0);release(&f);
}
 Fixture f;EdenDsmodHostApi h=cdr_setup(&f,1,8);StatsProbeResult r;f.mutate_on=99;f.change_size=1;f.change_address=GROUP+4;f.change_value=6;stats_probe(&h,&f.initial_identity,&r);CHECK(!r.shared_identity_valid);check_all(&r,0);release(&f);
}
static void cdr_suite(void){cdr_absence_modes();cdr_failure_kinds();cdr_no_other_defaults();cdr_extent_and_dirty();cdr_snapshot_changes();cdr_late_shared_changes();puts("PASS CDR-only runtime defaults: typed absence modes, terminal/cap/cycle distinctions, ID/float/range gates, path/default changes and final shared clearing;122/1450 cap");}
#ifndef CDR_EMBED
int main(void){stat_suite();cdr_suite();return 0;}
#endif
