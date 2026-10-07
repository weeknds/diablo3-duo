// SPDX-License-Identifier: GPL-3.0-or-later
#define main names_suite
#include "test_names.c"
#undef main
#include "details_probe.h"
#define SG UINT64_C(0x7400000000)
#define SP (SG+0x1000)
#define ST (SG+0x2000)
#define SA (SG+0x3000)
#define GROUP (SG+0x5000)
#define OWNER (SG+0x6000)
#define NODES (SG+0x8000)
#define CDR (MAIN+0x1921F98)
static uint32_t bits(float f){uint32_t x;memcpy(&x,&f,4);return x;}
static const uint32_t skeys[4]={0xFFFFF0C9,0xFFFFF0D3,0xFFFFF026,0xFFFFF0B9};
static const uint32_t indexes[4]={0x136,0x12C,0x1D9,0x146};
static uint64_t snode(unsigned field,unsigned i){return NODES+field*0x100+i*16;}
static char shown[6][512],shown_level[64],shown_stats[4][64];static unsigned pubs,ends;
static void begin(void*p){(void)p;pubs=0;}
static void end(void*p){(void)p;CHECK(pubs==18);ends++;}
static void publish(void*p,const char*name,const char*value){(void)p;pubs++;for(const unsigned char*c=(const unsigned char*)value;*c;c++)CHECK(*c>=32&&*c<=126);
 if(!strcmp(name,"details.level"))snprintf(shown_level,sizeof shown_level,"%s",value);
 for(unsigned i=0;i<6;i++){char key[40];snprintf(key,sizeof key,"skills.%u.name",i);if(!strcmp(key,name))snprintf(shown[i],sizeof shown[i],"%s",value);}
 for(unsigned i=0;i<4;i++){char key[40];snprintf(key,sizeof key,"stats.%u.value",i);if(!strcmp(key,name))snprintf(shown_stats[i],sizeof shown_stats[i],"%s",value);}
}
static void integer(void*p,const char*n,int64_t v){(void)p;(void)n;(void)v;CHECK(0);}
static uint64_t tick(void*p){(void)p;return 1;}
static EdenDsmodBool forbidden(void*p,uint64_t a,const void*d,size_t n){(void)p;(void)a;(void)d;(void)n;CHECK(0);return 0;}
static uint64_t guest(void*p,const EdenDsmodGuestCall*c){(void)p;(void)c;CHECK(0);return 0;}
static EdenDsmodHostApi module_fixture(NF*f){
 PlayerProbeIdentity identity;StoredSkillPair pairs[6];EdenDsmodHostApi h=fixture(f,&identity,pairs);
 f->partial=1;f->max_reads=498;f->max_bytes=15270;f->r[3]=(Region){SG,0x10000,calloc(1,0x10000)};f->r[4]=(Region){CDR,8,calloc(1,8)};CHECK(f->r[3].p&&f->r[4].p);
 put(f,PLAYER+0xD68C,42,4);put(f,PLAYER+4,0x56780000,4);put(f,C+0x9A0,SG,8);put(f,SG,SP,8);put(f,SP+0x100,4,4);put(f,SP+0x168,2,4);put(f,SP+0x120,ST,8);put(f,ST,SA,8);put(f,SA,0x56780000,4);put(f,SA+0x168,GROUP,8);put(f,GROUP+4,4,1);put(f,GROUP+0x10,OWNER,8);put(f,OWNER,0x1FF,4);put(f,OWNER+4,4,4);put(f,OWNER+0x10,OWNER+0x1C,8);put(f,OWNER+0x18,0x200,4);
 const float values[4]={1.25f,0.375f,102.5f,1.25f};
 for(unsigned field=0;field<4;field++){put(f,OWNER+0x1C+indexes[field]*8,snode(field,0),8);for(unsigned n=0;n<8;n++){put(f,snode(field,n),n==7?0:snode(field,n+1),8);uint32_t key=n==7?skeys[field]:0x100+n;if(field==1){uint32_t high=(n+1)*0x1000;key=high|(((high>>12)^0x12C)&0x1FF);}put(f,snode(field,n)+8,key,4);put(f,snode(field,n)+12,bits(values[field]),4);}}
 put(f,CDR,0xD3,4);put(f,CDR+4,bits(0.375f),4);
 h.abi_version=1;h.struct_size=sizeof h;h.abi_hash=EDEN_DSMOD_MODULE_ABI_HASH;h.title_id=UINT64_C(0x01001B300B9BE000);h.capabilities=EDEN_DSMOD_CAP_NO_TICK_WHEN_HIDDEN;
 const char*build="2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000";
 for(unsigned i=0;i<32;i++){unsigned v;CHECK(sscanf(build+2*i,"%2x",&v)==1);h.build_id[i]=(uint8_t)v;}
 h.get_tick=tick;h.begin_output=begin;h.end_output=end;h.publish_text=publish;h.publish_i64=integer;h.write_memory=forbidden;h.queue_guest_call=guest;return h;
}
static void module_success(void){NF f;EdenDsmodHostApi h=module_fixture(&f);const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);CHECK(a);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);CHECK(!strcmp(shown[0],"Synthetic skill 0"));CHECK(!strcmp(shown_level,"Level 42")&&!strcmp(shown_stats[1],"37.50%"));CHECK(f.maps==498&&f.bytes==15270);a->destroy(s);release(&f);}
static void module_clear(void){CHECK(!strcmp(shown_level,"Level unavailable"));for(unsigned i=0;i<6;i++)CHECK(!strcmp(shown[i],"Unavailable"));for(unsigned i=0;i<4;i++)CHECK(!strcmp(shown_stats[i],"Unavailable"));}
static int stat_failure(unsigned n){
 if((n>=103&&n<=111)||(n>=140&&n<=148))return 0;
 if((n>=112&&n<=121)||(n>=149&&n<=158))return 1;
 if((n>=122&&n<=130)||(n>=159&&n<=167))return 2;
 if((n>=131&&n<=139)||(n>=168&&n<=176))return 3;
 return -1;
}
static void module_faults(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 for(unsigned mapping=0;mapping<2;mapping++)for(unsigned n=1;n<=498;n++){
  NF f;EdenDsmodHostApi h=module_fixture(&f);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);CHECK(!strcmp(shown[0],"Synthetic skill 0"));reset(&f);if(mapping)f.mapfail=n;else f.fail=n;a->sample(s,&h);
  if(n<=200){int skill=(n>=41&&n<=46)||(n>=60&&n<=65),field=stat_failure(n);
   if(!skill&&field<0)module_clear();
   else{CHECK(!strcmp(shown_level,"Level 42"));for(unsigned i=0;i<6;i++)CHECK((!strcmp(shown[i],"Unavailable"))==skill);for(unsigned i=0;i<4;i++)CHECK((!strcmp(shown_stats[i],"Unavailable"))==((int)i==field));}
  }else{unsigned k=n-200;
   if(k<=13||k>=286)module_clear();
   else{CHECK(!strcmp(shown_level,"Level 42"));for(unsigned i=0;i<4;i++)CHECK(strcmp(shown_stats[i],"Unavailable"));
    if(k<=65||k>=234){for(unsigned i=0;i<6;i++)CHECK(!strcmp(shown[i],"Unavailable"));}
    else{unsigned field=k<=149?(k-66)/14:k<=161?(k-150)/2:5-(k-162)/12;for(unsigned i=0;i<6;i++)CHECK((!strcmp(shown[i],"Unavailable"))==(i==field));}
   }
  }
  CHECK(f.maps<=498&&f.calls<=f.maps&&f.bytes<=15270);a->destroy(s);release(&f);
 }
}
static void module_late(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 for(unsigned failure=0;failure<2;failure++)for(unsigned kind=0;kind<5;kind++){
  NF f;EdenDsmodHostApi h=module_fixture(&f);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);reset(&f);f.mutate=361;if(failure)f.fail=361;
  f.addr=kind==0?C+0x84:kind==1?PLAYER+0x134C:kind==2?MAIN+0x1A63CEC:kind==3?G+0x1148:IP+40;f.value=kind==0?0:kind==1?123:kind==4?RHANDLE+0x40000:2;f.size=4;
  a->sample(s,&h);CHECK(f.seen==1);if(kind==0)module_clear();else{CHECK(!strcmp(shown_level,"Level 42"));for(unsigned i=0;i<6;i++)CHECK(!strcmp(shown[i],"Unavailable"));}
  a->destroy(s);release(&f);
 }
}
static void module_presentation(void){NF f;EdenDsmodHostApi h=module_fixture(&f);const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);void*s=a->create(&h,NULL);CHECK(s);
 memset(loc(&f,TEXT(0),512),'W',511);put(&f,TEXT(0)+511,0,1);a->sample(s,&h);CHECK(!strcmp(shown[0],"Unavailable")&&!strcmp(shown[1],"Synthetic skill 1"));reset(&f);
 put(&f,PLAYER+0x134C,UINT32_MAX,4);put(&f,PLAYER+0x1350,1234,4);a->sample(s,&h);CHECK(!strcmp(shown[0],"Unassigned"));reset(&f);
 put(&f,PLAYER+0x134C,30744,4);put(&f,PLAYER+0x1350,UINT32_MAX,4);a->sample(s,&h);CHECK(!strcmp(shown[0],"Unavailable"));reset(&f);
 h.build_id[31]^=1;a->sample(s,&h);module_clear();CHECK(!f.maps);h.build_id[31]^=1;a->sample(s,&h);module_clear();CHECK(!f.maps);a->destroy(s);release(&f);
}
static void module_gates(void){const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);CHECK(!eden_dsmod_get_module(2,EDEN_DSMOD_MODULE_ABI_HASH));CHECK(!eden_dsmod_get_module(1,0));CHECK(!a->supports_build(NULL)&&!a->supports_build("2607A74F5DF7754C"));
 for(unsigned i=0;i<45;i++){NF f;EdenDsmodHostApi h=module_fixture(&f);if(i<32)h.build_id[i]^=1;if(i==32)h.title_id=0;if(i==33)h.struct_size=8;if(i==34)h.abi_hash=0;if(i==35)h.abi_version=2;if(i==36)h.userdata=NULL;if(i==37)h.capabilities=0;if(i==38)h.read_memory=NULL;if(i==39)h.is_mapped=NULL;if(i==40)h.publish_text=NULL;if(i==41)h.publish_i64=NULL;if(i==42)h.begin_output=NULL;if(i==43)h.end_output=NULL;if(i==44)h.get_tick=NULL;CHECK(!a->create(&h,NULL)&&!f.maps);release(&f);}
}
int main(void){module_success();module_faults();module_late();module_presentation();module_gates();puts("PASS combined names/CDR: all996 read/map faults including inherited200 positions, late transitions, UI clearing/fit, exact498/15270 cap; no writes/calls");return 0;}
