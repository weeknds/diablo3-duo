// SPDX-License-Identifier: GPL-3.0-or-later
// Entirely synthetic memory and labels. No game asset fixture.
#include "names_probe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#x);exit(1);} } while(0)
#define MAIN UINT64_C(0x7100000000)
#define HEAP UINT64_C(0x7200000000)
#define G (HEAP+0x1000)
#define C (HEAP+0x3000)
#define SEL (HEAP+0x5000)
#define PLAYER (HEAP+0x6060)
#define PLAYERS (HEAP+0x6000)
#define POOL (HEAP+0x16000)
#define PAGES (HEAP+0x17000)
#define ACTOR (HEAP+0x18000)
#define M (HEAP+0x20000)
#define DIR (HEAP+0x28000)
#define V (HEAP+0x29000)
#define VT (HEAP+0x2A000)
#define K(i) (HEAP+0x30000+(i)*0x1000)
#define DESC(i) (HEAP+0x34000+(i)*0x1000)
#define ARENA(i) (HEAP+0x38000+(i)*0x1000)
#define A (HEAP+0x3C000)
#define IP (HEAP+0x3D000)
#define RP (HEAP+0x3E000)
#define RB (HEAP+0x3F000)
#define O (HEAP+0x40000)
#define E (HEAP+0x41000)
#define H (HEAP+0x42004)
#define BUCKETS (HEAP+0x43004)
#define OVBUCKETS (HEAP+0x44004)
#define BLOCK(i) (HEAP+0x45000+(i)*0x1000)
#define TEXT(i) (HEAP+0x50000+(i)*0x1000)
#define OV (MAIN+0x19DAFE8)
#define HANDLE UINT32_C(0x12340000)
#define RHANDLE UINT32_C(0x12000000)
typedef struct Region {uint64_t a;size_t n;unsigned char *p;} Region;
typedef struct NF {
 Region r[5];unsigned partial,max_reads,max_bytes,maps,calls,bytes,fail,mapfail,mutate,seen;uint64_t addr,value;unsigned size;
 uint64_t log[600];unsigned lens[600];
} NF;
static unsigned char *loc(NF*f,uint64_t a,size_t n){for(unsigned i=0;i<5;i++)if(a>=f->r[i].a&&a-f->r[i].a<=f->r[i].n&&n<=f->r[i].n-(a-f->r[i].a))return f->r[i].p+(a-f->r[i].a);return NULL;}
static void put(NF*f,uint64_t a,uint64_t v,unsigned n){unsigned char*p=loc(f,a,n);CHECK(p);memcpy(p,&v,n);}
static void str(NF*f,uint64_t a,const char*s){unsigned char*p=loc(f,a,strlen(s)+1);CHECK(p);memcpy(p,s,strlen(s)+1);}
static EdenDsmodBool map(void*p,uint64_t a,uint64_t n){NF*f=p;CHECK(n<=512);f->maps++;f->bytes+=(unsigned)n;CHECK(f->maps<=f->max_reads&&f->bytes<=f->max_bytes);f->log[f->maps-1]=a;f->lens[f->maps-1]=(unsigned)n;if(f->mapfail==f->maps)return 0;return loc(f,a,n)!=NULL;}
static EdenDsmodBool readmem(void*p,uint64_t a,void*out,size_t n){NF*f=p;f->calls++;if(f->mutate==f->maps){put(f,f->addr,f->value,f->size);f->seen++;}if(f->fail==f->maps){if(f->partial){unsigned char*src=loc(f,a,n);CHECK(src);memcpy(out,src,(n+1)/2);}else memset(out,0xA5,n);return 0;}unsigned char*src=loc(f,a,n);CHECK(src);memcpy(out,src,n);return 1;}
static uint32_t hash(const char*s){uint32_t h=0;while(*s)h=h*33+(unsigned char)*s++;return h;}
static uint32_t bucket(uint32_t h){uint32_t x=0x811C9DC5;for(unsigned i=0;i<4;i++){x^=(h>>(8*i))&255;x*=0x01000193;}return x&63;}
static uint64_t node(unsigned field,unsigned j){unsigned index=field*8+j;return BLOCK(index/8)+0x50+24*(index%8);}
static void owner(NF*f,uint64_t h,uint64_t buckets,unsigned stride){put(f,h,63,4);put(f,h+4,48,4);put(f,h+8,h+0x48,8);put(f,h+0x10,buckets,8);put(f,h+0x18,64,4);put(f,h+0x20,buckets,8);put(f,h+0x28,512,4);put(f,h+0x38,0x600DF00D,4);put(f,h+0x48,stride,4);put(f,h+0x70,0x600DF00D,4);}
static EdenDsmodHostApi fixture(NF*f,PlayerProbeIdentity*id,StoredSkillPair pairs[6]){
 memset(f,0,sizeof *f);f->max_reads=298;f->max_bytes=13200;f->r[0]=(Region){MAIN+0x114A840,8,calloc(1,8)};f->r[1]=(Region){MAIN+0x1955C58,0x110100,calloc(1,0x110100)};f->r[2]=(Region){HEAP,0x57000,calloc(1,0x57000)};
 for(unsigned i=0;i<3;i++)CHECK(f->r[i].p);
 put(f,MAIN+0x114A840,G,8);put(f,G+0x10,C,8);put(f,C+0x84,1,4);put(f,C+0xBA0,SEL,8);put(f,C+0x948,PLAYERS,8);for(unsigned i=1;i<4;i++)put(f,SEL+4*i,UINT32_MAX,4);put(f,SEL+16,1,4);put(f,PLAYER+4,123,4);put(f,PLAYER+8,HANDLE,4);put(f,C+0xA98,POOL,8);put(f,POOL+0x100,4,4);put(f,POOL+0x168,2,4);put(f,POOL+0x120,PAGES,8);put(f,PAGES,ACTOR,8);put(f,ACTOR,HANDLE,4);
 *id=(PlayerProbeIdentity){.globals=G,.client=C,.selector=SEL,.players=PLAYERS,.player=PLAYER,.pool=POOL,.pages=PAGES,.page=ACTOR,.actor=ACTOR,.game_type=1,.slots={0,-1,-1,-1},.count=1,.player_index=0,.acd_id=123,.actor_id=HANDLE,.capacity=4,.shift=2,.actor_identity=HANDLE};
 put(f,MAIN+0x1A216F0,M,8);put(f,M+0x11E0,DIR,8);put(f,M+0x11E8,100,4);put(f,MAIN+0x1A63CEC,1,4);put(f,G+0x1100,V,8);put(f,V,VT,8);put(f,V+8,0x2E6,8);put(f,V+16,0x2E6,8);put(f,VT+0x2E5*16+8,10,4);
 for(unsigned c=0;c<3;c++){put(f,M+c*0x598+0x118,K(c),8);put(f,K(c),DESC(c),8);put(f,K(c)+0x14,16,4);put(f,M+c*0x598+0x438,ARENA(c),8);put(f,M+c*0x598+0x2AC,0x1000,4);put(f,M+c*0x598+0x5F4,0x1000,4);}
 put(f,DIR+10*8,DESC(0)+6*24,8);put(f,DESC(0)+6*24,(10<<8)|0x2A,4);
 put(f,MAIN+0x19DAFB0,A,8);put(f,MAIN+0x1955C58,100,4);put(f,MAIN+0x1955C60,IP,8);put(f,A,MAIN+0x10C7948,8);put(f,A+0x20,RP,8);put(f,A+0x50,0x2A,4);put(f,A+0x7C,40,4);put(f,RP+0x100,1,4);put(f,RP+0x104,16,4);put(f,RP+0x120,RB,8);put(f,IP+10*4,RHANDLE,4);put(f,RB,RHANDLE,4);put(f,RB+4,0x2A000000,4);put(f,RB+8,O,8);put(f,O,10,4);put(f,O+8,0x40,4);put(f,O+12,48,4);put(f,O+0x10,E,8);put(f,O+0x1C,48*24,4);put(f,O+0x20,H,8);
 owner(f,H,BUCKETS,24);owner(f,OV,OVBUCKETS,16);put(f,OV+4,0,4);put(f,H+0x58,BLOCK(0),8);
 for(unsigned i=0;i<8;i++){put(f,BLOCK(i),i==7?0:BLOCK(i+1),8);put(f,BLOCK(i)+8,BLOCK(i)+0x50,8);put(f,BLOCK(i)+0x18,8,4);put(f,BLOCK(i)+0x1C,24,4);put(f,BLOCK(i)+0x20,8,4);}
 unsigned used[64]={0};for(unsigned i=0;i<6;i++){char internal[128],key[133],text[512];unsigned seed=i;do{snprintf(internal,sizeof internal,"SyntheticPower%u",seed++);snprintf(key,sizeof key,"%s_name",internal);}while(used[bucket(hash(key))]);used[bucket(hash(key))]=1;
 pairs[i]=(StoredSkillPair){(int32_t)(20+i),-1};put(f,PLAYER+0x134C+16*i,20+i,4);put(f,PLAYER+0x1350+16*i,UINT32_MAX,4);put(f,DIR+(20+i)*8,DESC(0)+i*24,8);put(f,DESC(0)+i*24,((20+i)<<8)|0x1D,4);put(f,DESC(0)+i*24+8,ARENA(0)+i*128,8);str(f,ARENA(0)+i*128,internal);
 put(f,BUCKETS+8*bucket(hash(key)),node(i,0),8);for(unsigned j=0;j<8;j++){put(f,node(i,j),j==7?0:node(i,j+1),8);put(f,node(i,j)+8,j==7?hash(key):hash(key)^0x80000000,4);put(f,node(i,j)+16,E+i*24,8);}put(f,E+i*24,TEXT(i),8);put(f,E+i*24+12,512,4);put(f,E+i*24+16,hash(key),4);snprintf(text,sizeof text,"Synthetic skill %u",i);str(f,TEXT(i),text);}
 return (EdenDsmodHostApi){.userdata=f,.main_base=MAIN,.main_size=0x1B00000,.is_mapped=map,.read_memory=readmem};
}
static void release(NF*f){for(unsigned i=0;i<5;i++)free(f->r[i].p);}
static void reset(NF*f){f->maps=f->calls=f->bytes=f->seen=0;}
static void valid(void){NF f;PlayerProbeIdentity p;StoredSkillPair pairs[6];EdenDsmodHostApi h=fixture(&f,&p,pairs);NamesProbeResult r;names_probe(&h,&p,pairs,&r);CHECK(r.shared_identity_valid&&r.skills_valid);for(unsigned i=0;i<6;i++){char want[64];snprintf(want,sizeof want,"Synthetic skill %u",i);CHECK(r.fields[i].available&&!strcmp(r.fields[i].text,want));}CHECK(r.reads==298&&r.bytes==13200&&f.maps==298&&f.calls==298&&f.bytes==13200);release(&f);}
static void none(const NamesProbeResult*r){for(unsigned i=0;i<6;i++)CHECK(!r->fields[i].available&&!r->fields[i].text[0]);}
static void failures(void){
 for(unsigned mapping=0;mapping<2;mapping++)for(unsigned n=1;n<=298;n++){
  NF f;PlayerProbeIdentity p;StoredSkillPair a[6];EdenDsmodHostApi h=fixture(&f,&p,a);NamesProbeResult r;
  names_probe(&h,&p,a,&r);CHECK(r.fields[0].available);reset(&f);if(mapping)f.mapfail=n;else f.fail=n;
  names_probe(&h,&p,a,&r);CHECK(r.reads==f.maps&&r.bytes==f.bytes&&r.reads<=298&&r.bytes<=13200);
  if(n<=13||n>=286){CHECK(!r.shared_identity_valid&&!r.skills_valid);none(&r);}
  else if(n<=19||n>=280){CHECK(r.shared_identity_valid&&!r.skills_valid);none(&r);}
  else if(n<=65||n>=234){CHECK(r.shared_identity_valid&&r.skills_valid);none(&r);}
  else {unsigned field=n<=149?(n-66)/14:n<=161?(n-150)/2:5-(n-162)/12;CHECK(r.shared_identity_valid&&r.skills_valid);for(unsigned i=0;i<6;i++)CHECK(r.fields[i].available==(i!=field));}
  release(&f);
 }
}
static void late_changes(void){
 const struct {uint64_t a,v;unsigned n;int scope;} changes[]={
 {MAIN+0x1A63CEC,2,4,0},{MAIN+0x1A63CF0,2,4,0},{G+0x1148,1,4,0},
 {MAIN+0x19DAFB0,A+8,8,0},{IP+40,RHANDLE+0x40000,4,0},{RB,RHANDLE+0x40000,4,0},
 {O+8,0x42,4,0},{OVBUCKETS+8*0,1,8,0},
 {PLAYER+0x134C,99,4,1},{PLAYER+0x1350,0,4,1},{C+0x84,0,4,2},
 {TEXT(5),'X',1,3},{ARENA(0)+5*128,'X',1,3},{H+0x58,BLOCK(1),8,0},
 {V+8,0x2E7,8,0},{M+0x11E0,DIR+8,8,0}};
 for(unsigned failure=0;failure<2;failure++)for(unsigned i=0;i<sizeof changes/sizeof changes[0];i++){
  NF f;PlayerProbeIdentity p;StoredSkillPair a[6];EdenDsmodHostApi h=fixture(&f,&p,a);NamesProbeResult r;
  if(i==7){f.addr=OVBUCKETS+8*bucket(10);}else f.addr=changes[i].a;
  f.value=changes[i].v;f.size=changes[i].n;f.mutate=i==12?160:161;if(failure)f.fail=161;
  names_probe(&h,&p,a,&r);CHECK(f.seen==1);
  if(changes[i].scope==2){CHECK(!r.shared_identity_valid&&!r.skills_valid);none(&r);}
  else if(changes[i].scope==1){CHECK(r.shared_identity_valid&&!r.skills_valid);none(&r);}
  else if(changes[i].scope==0){CHECK(r.shared_identity_valid&&r.skills_valid);none(&r);}
  else{CHECK(r.shared_identity_valid&&r.skills_valid&&!r.fields[5].available);for(unsigned j=0;j<5;j++)CHECK(r.fields[j].available);}
  release(&f);
 }
}
static void malformed(void){
 const struct {uint64_t a,v;unsigned n;int field;} bad[]={
 {DESC(0),0x1E,4,0},{DESC(0)+8,ARENA(1),8,0},{DIR+20*8,DESC(0)+1,8,0},
 {DIR+20*8,DESC(0)+24*16,8,0},{M+0x5F4,0x1001,4,-1},
 {K(1),DESC(0),8,-1},{MAIN+0x1955C58,10,4,-1},{IP+40,UINT32_MAX,4,-1},
 {RB,RHANDLE+0x40000,4,-1},{RB+7,0x1D,1,-1},{O,11,4,-1},{O+8,0,4,-1},{O+8,0x42,4,-1},
 {O+0x1C,49,4,-1},{H+0x38,0xFEEDFACE,4,-1},{H+0x70,0xFEEDFACE,4,-1},
 {H+0x18,3,4,-1},{H+0x28,511,4,-1},{H+8,H+0x50,8,-1},{H+0x48,16,4,-1},
 {BLOCK(0)+8,BLOCK(0)+0x58,8,-1},{BLOCK(0)+0x18,0,4,-1},{BLOCK(0)+0x18,0x5555556,4,-1},
 {BLOCK(0)+0x20,9,4,-1},{BLOCK(0)+0x2C,9,4,-1},{BLOCK(0)+0x1C,16,4,-1},
 {BLOCK(7),BLOCK(0),8,-1},{BLOCK(7),BLOCK(8),8,-1},{BLOCK(1),BLOCK(1),8,-1},
 {E+12,0,4,0},{E+12,513,4,0},{E+16,0,4,0},{node(0,7)+16,E+1,8,0},
 {node(0,7)+16,E+48*24,8,0},{node(0,0),node(0,0),8,0},
 {node(0,7)+8,1,4,0},{node(0,0),node(0,1)+4,8,0},{TEXT(0),0xFF,1,0},
 {TEXT(0),'\n',1,0},{TEXT(0),'<',1,0},{TEXT(0),'>',1,0},{TEXT(0),'{',1,0},{TEXT(0),'}',1,0},
 {ARENA(0),0,1,0},{TEXT(0),0,1,0},{VT+0x2E5*16+8,0xFFFFFFFF,4,-1},
 {V+16,UINT64_MAX,8,-1},{RP+0x104,24,4,-1},{RP+0x100,0x3FFFF,4,-1},
 {A+0x50,0x1D,4,-1},{A+0x7C,39,4,-1},{A,0,8,-1}};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
  NF f;PlayerProbeIdentity p;StoredSkillPair a[6];EdenDsmodHostApi h=fixture(&f,&p,a);NamesProbeResult r;put(&f,bad[i].a,bad[i].v,bad[i].n);
  names_probe(&h,&p,a,&r);CHECK(r.shared_identity_valid&&r.skills_valid);
  if(bad[i].field<0)none(&r);else CHECK(!r.fields[bad[i].field].available);release(&f);
 }
}
static void bounded_strings(void){
 for(unsigned kind=0;kind<7;kind++){
  NF f;PlayerProbeIdentity p;StoredSkillPair a[6];EdenDsmodHostApi h=fixture(&f,&p,a);NamesProbeResult r;
  if(kind==0){memset(loc(&f,TEXT(0),512),'Q',512);put(&f,TEXT(0)+511,0,1);}
  if(kind==1)memset(loc(&f,TEXT(0),512),'Q',512);
  if(kind==2)memset(loc(&f,ARENA(0),128),'Q',128);
  if(kind==3)put(&f,M+0x5F4,4,4);
  if(kind==4)put(&f,OVBUCKETS+8*bucket(10),node(0,0),8);
  if(kind==5){put(&f,PLAYER+0x134C,UINT32_MAX,4);put(&f,PLAYER+0x1350,1234,4);a[0]=(StoredSkillPair){-1,1234};}
  if(kind==6){put(&f,node(0,7)+8,1,4);put(&f,node(0,7),node(1,0),8);}
  names_probe(&h,&p,a,&r);CHECK(r.shared_identity_valid&&r.skills_valid);
  if(kind==0)CHECK(r.fields[0].available&&strlen(r.fields[0].text)==511);else CHECK(!r.fields[0].available);
  if(kind==5){reset(&f);f.mutate=1;f.addr=PLAYER+0x1350;f.value=1235;f.size=4;names_probe(&h,&p,a,&r);CHECK(r.shared_identity_valid&&!r.skills_valid);none(&r);}
  release(&f);
 }
}
static void no_wrapped_offsets(void){
 for(unsigned i=0;i<2;i++){
  NF f;PlayerProbeIdentity p;StoredSkillPair a[6];EdenDsmodHostApi h=fixture(&f,&p,a);NamesProbeResult r;
  put(&f,i?MAIN+0x114A840:MAIN+0x1A216F0,UINT64_MAX-7,8);names_probe(&h,&p,a,&r);
  for(unsigned j=0;j<f.maps;j++)CHECK(f.log[j]>=MAIN);
  release(&f);
 }
}
int main(void){valid();failures();late_changes();malformed();bounded_strings();no_wrapped_offsets();puts("PASS names: 596 read/map failures, closing gates, malformed ownership, strings and exact298/13200 cap");return 0;}
