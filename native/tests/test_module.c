// SPDX-License-Identifier: GPL-3.0-or-later
// Module integration over synthetic memory. Reader suites independently test offsets/formulas.
#define main names_suite
#include "test_names.c"
#undef main
#include "details_probe.h"
#include "equipment_probe.h"
#include "map_view.h"
#include "dsmod_module_extensions.h"
#define SG UINT64_C(0x7400000000)
#define SP (SG+0x1000)
#define ST (SG+0x2000)
#define SA (SG+0x3000)
#define GROUP (SG+0x5000)
#define OWNER (SG+0x6000)
#define NODES (SG+0x8000)
#define DEFINITIONS (MAIN+0x191EAD8)
#define CDR (MAIN+0x1921F98)
#define MODULE_READ_BOUND (DETAILS_MAX_READS+EQUIPMENT_MAX_READS+MAP_MAX_READS+NAV_MAX_READS+MAP_CONTEXT_MAX_READS+PLAYER_MAX_READS)
#define MODULE_BYTE_BOUND (DETAILS_MAX_BYTES+EQUIPMENT_MAX_BYTES+MAP_MAX_BYTES+NAV_MAX_BYTES+MAP_CONTEXT_MAX_BYTES+PLAYER_MAX_BYTES)
static uint32_t bits(float f){uint32_t x;memcpy(&x,&f,4);return x;}
static const uint32_t skeys[16]={0xFFFFF0C9,0xFFFFF0D3,0xFFFFF026,0xFFFFF0B9,0xFFFFF00E,0xFFFFF00F,0xFFFFF010,0xFFFFF011,0xFFFFF0FC,0xFFFFF0FD,0xFFFFF4B7,0xFFFFF0FF,0xFFFFF100,0xFFFFF0FE,0xFFFFF094,0x000032E3};
static uint64_t snode(unsigned field,unsigned i){return NODES+field*0x100+i*16;}
static uint64_t sbucket(unsigned field){uint32_t key=skeys[field];return OWNER+0x1C+8*((key^(key>>12))&0x1FFu);}
typedef struct Access {uint64_t address;size_t size;} Access;
static Access trace[MODULE_READ_BOUND];
static uint64_t name_lookup_buckets[6];
static uint64_t clock_tick;
static unsigned publications,ends,in_output;
static struct {
 char skills[6][512],runes[6][64],level[64],paragon[64],status[160],stats[STATS_COUNT][64];
 char gear[EQUIPMENT_SLOT_COUNT][64],selected_slot[64],selected_name[512],map_key[80],map_status[160];
 int64_t class_index,map_available;
 unsigned skill_seen[6],rune_seen[6],stat_seen[STATS_COUNT],level_seen,paragon_seen,status_seen,class_seen,gear_seen[EQUIPMENT_SLOT_COUNT],map_seen;
} shown;
static void begin(void*p){(void)p;CHECK(!in_output);in_output=1;publications=0;memset(shown.skill_seen,0,sizeof shown.skill_seen);memset(shown.rune_seen,0,sizeof shown.rune_seen);memset(shown.stat_seen,0,sizeof shown.stat_seen);memset(shown.gear_seen,0,sizeof shown.gear_seen);shown.level_seen=shown.paragon_seen=shown.status_seen=shown.class_seen=shown.map_seen=0;}
static void end(void*p){(void)p;CHECK(in_output&&shown.level_seen==1&&shown.paragon_seen==1&&shown.status_seen==1&&shown.class_seen==1);for(unsigned i=0;i<6;i++)CHECK(shown.skill_seen[i]==1&&shown.rune_seen[i]==1);for(unsigned i=0;i<STATS_COUNT;i++)CHECK(shown.stat_seen[i]==1);CHECK(publications>=4+12+STATS_COUNT);in_output=0;ends++;}
static void publish(void*p,const char*name,const char*value){(void)p;CHECK(in_output);publications++;for(const unsigned char*c=(const unsigned char*)value;*c;c++)CHECK(*c>=32&&*c<=126);
 if(!strcmp(name,"details.level")){shown.level_seen++;snprintf(shown.level,sizeof shown.level,"%s",value);}
 if(!strcmp(name,"details.paragon")){shown.paragon_seen++;snprintf(shown.paragon,sizeof shown.paragon,"%s",value);}
 if(!strcmp(name,"details.status")){shown.status_seen++;snprintf(shown.status,sizeof shown.status,"%s",value);}
 if(!strcmp(name,"map.image"))snprintf(shown.map_key,sizeof shown.map_key,"%s",value);
 if(!strcmp(name,"map.status"))snprintf(shown.map_status,sizeof shown.map_status,"%s",value);
 if(!strcmp(name,"gear.selected_slot"))snprintf(shown.selected_slot,sizeof shown.selected_slot,"%s",value);
 if(!strcmp(name,"gear.selected_name"))snprintf(shown.selected_name,sizeof shown.selected_name,"%s",value);
 for(unsigned i=0;i<6;i++){char key[40];snprintf(key,sizeof key,"skills.%u.name",i);if(!strcmp(key,name)){shown.skill_seen[i]++;snprintf(shown.skills[i],sizeof shown.skills[i],"%s",value);}snprintf(key,sizeof key,"skills.%u.rune",i);if(!strcmp(key,name)){shown.rune_seen[i]++;snprintf(shown.runes[i],sizeof shown.runes[i],"%s",value);}}
 for(unsigned i=0;i<STATS_COUNT;i++){char key[40];snprintf(key,sizeof key,"stats.%u.value",i);if(!strcmp(key,name)){shown.stat_seen[i]++;snprintf(shown.stats[i],sizeof shown.stats[i],"%s",value);}}
 for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++){char key[40];snprintf(key,sizeof key,"gear.%u.name",i);if(!strcmp(key,name)){shown.gear_seen[i]++;snprintf(shown.gear[i],sizeof shown.gear[i],"%s",value);}}
}
static void integer(void*p,const char*n,int64_t v){(void)p;CHECK(in_output);publications++;if(!strcmp(n,"details.class_index")){shown.class_seen++;shown.class_index=v;}if(!strcmp(n,"map.available")){shown.map_seen++;shown.map_available=v;}}
static uint64_t tick(void*p){(void)p;return clock_tick;}
static EdenDsmodBool forbidden(void*p,uint64_t a,const void*d,size_t n){(void)p;(void)a;(void)d;(void)n;CHECK(0);return 0;}
static uint64_t guest(void*p,const EdenDsmodGuestCall*c){(void)p;(void)c;CHECK(0);return 0;}
static EdenDsmodBool module_map(void*p,uint64_t a,uint64_t n){NF*f=p;CHECK(n<=512);f->maps++;f->bytes+=(unsigned)n;CHECK(f->maps<=MODULE_READ_BOUND&&f->bytes<=MODULE_BYTE_BOUND);trace[f->maps-1]=(Access){a,(size_t)n};if(f->mapfail==f->maps)return 0;return loc(f,a,n)!=NULL;}
static EdenDsmodHostApi module_fixture(NF*f){
 PlayerProbeIdentity identity;StoredSkillPair pairs[6];EdenDsmodHostApi h=fixture(f,&identity,pairs);
 for(unsigned field=0;field<6;field++)for(unsigned b=0;b<64;b++){uint64_t head=0;memcpy(&head,loc(f,BUCKETS+8*b,8),8);if(head==node(field,0))name_lookup_buckets[field]=BUCKETS+8*b;}
 f->partial=1;f->max_reads=MODULE_READ_BOUND;f->max_bytes=MODULE_BYTE_BOUND;f->r[3]=(Region){SG,0x10000,calloc(1,0x10000)};f->r[4]=(Region){DEFINITIONS,0x14000,calloc(1,0x14000)};CHECK(f->r[3].p&&f->r[4].p);
 put(f,PLAYER+0xD688,1,4);put(f,PLAYER+0xD68C,42,4);put(f,PLAYER+0xD690,17,4);put(f,PLAYER+4,0x56780000,4);put(f,C+0x9A0,SG,8);put(f,SG,SP,8);put(f,SP+0x100,4,4);put(f,SP+0x168,2,4);put(f,SP+0x120,ST,8);put(f,ST,SA,8);put(f,SA,0x56780000,4);put(f,SA+0x168,GROUP,8);put(f,GROUP+4,4,1);put(f,GROUP+0x10,OWNER,8);put(f,OWNER,0x1FF,4);put(f,OWNER+4,4,4);put(f,OWNER+0x10,OWNER+0x1C,8);put(f,OWNER+0x18,0x200,4);
 const float values[16]={1.25f,0.375f,102.5f,1.25f,13,9,9,11,0.2f,0.3f,0.1f,0.2f,0.6f,0.05f,0,0.175f};
 for(unsigned field=0;field<16;field++){put(f,sbucket(field),snode(field,0),8);unsigned count=field<4?8:1;for(unsigned n=0;n<count;n++){put(f,snode(field,n),n+1==count?0:snode(field,n+1),8);uint32_t key=n+1==count?skeys[field]:0x100+n;if(field==1){uint32_t high=(n+1)*0x1000;key=high|(((high>>12)^0x12C)&0x1FF);}put(f,snode(field,n)+8,key,4);put(f,snode(field,n)+12,field==14?3:bits(values[field]),4);}}
 put(f,CDR,0xD3,4);put(f,CDR+4,bits(0.375f),4);
 h.abi_version=1;h.struct_size=sizeof h;h.abi_hash=EDEN_DSMOD_MODULE_ABI_HASH;h.title_id=UINT64_C(0x01001B300B9BE000);h.capabilities=EDEN_DSMOD_CAP_NO_TICK_WHEN_HIDDEN|EDEN_DSMOD_CAP_EXTENSIONS;
 const char*build="2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000";
 for(unsigned i=0;i<32;i++){unsigned v;CHECK(sscanf(build+2*i,"%2x",&v)==1);h.build_id[i]=(uint8_t)v;}
 clock_tick=1;h.get_tick=tick;h.begin_output=begin;h.end_output=end;h.publish_text=publish;h.publish_i64=integer;h.write_memory=forbidden;h.queue_guest_call=guest;h.is_mapped=module_map;return h;
}
static void check_stats(int unavailable){for(unsigned i=0;i<STATS_COUNT;i++)CHECK((!strcmp(shown.stats[i],"Unavailable"))==unavailable);}
static void check_identity(void){CHECK(!strcmp(shown.level,"Level 42")&&!strcmp(shown.paragon,"Paragon 17")&&shown.class_index==2);}
static void module_clear(void){CHECK(!strcmp(shown.level,"Level unavailable")&&!strcmp(shown.paragon,"Paragon unavailable")&&shown.class_index==0);for(unsigned i=0;i<6;i++)CHECK(!strcmp(shown.skills[i],"Unavailable")&&!shown.runes[i][0]);check_stats(1);CHECK(shown.map_available==0);for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)CHECK(!strcmp(shown.gear[i],"Unavailable"));CHECK(!strcmp(shown.selected_slot,"Equipment inspection")&&!strcmp(shown.selected_name,"Select an equipment slot"));}
static unsigned count_access(uint64_t address,unsigned count){unsigned found=0;for(unsigned i=0;i<count;i++)found+=trace[i].address==address;return found;}
static void module_success(void){
 NF f;EdenDsmodHostApi h=module_fixture(&f);const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);CHECK(a);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);check_identity();check_stats(0);CHECK(!strcmp(shown.skills[0],"Synthetic skill 0")&&!strcmp(shown.stats[1],"37.50%")&&!strcmp(shown.stats[8],"65.00%")&&!strcmp(shown.stats[9],"17.50%"));
 // Missing optional routes do not invalidate the independently verified character data.
 CHECK(shown.map_seen==1&&!shown.map_available);for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)CHECK(shown.gear_seen[i]==1&&!strcmp(shown.gear[i],"Unavailable"));CHECK(count_access(SA+0xC8,f.maps));
 reset(&f);clock_tick=2;a->sample(s,&h);check_identity();check_stats(0);CHECK(!count_access(SA+0xC8,f.maps));
 reset(&f);clock_tick=16;a->sample(s,&h);check_identity();CHECK(count_access(SA+0xC8,f.maps));
 reset(&f);clock_tick=17;put(&f,PLAYER+4,0x56790000,4);put(&f,SA,0x56790000,4);a->sample(s,&h);check_identity();check_stats(0);CHECK(count_access(SA+0xC8,f.maps));
 a->destroy(s);release(&f);
}
// Classify the failed access by its semantic owner, not a historical read index.
enum FailureScope { CLEAR_ALL=-3, CLEAR_SKILLS=-2, KEEP_CORE=-1, STAT_SCOPE=0, NAME_SCOPE=STATS_COUNT };
static int failure_scope(const Access*baseline,unsigned position,unsigned core_count,unsigned closing_start){
 if(position>=closing_start)return CLEAR_ALL;
 if(position>=core_count)return KEEP_CORE;
 const uint64_t address=baseline[position].address;
 // Names also journals the globals pointer as string-table ownership. Only a
 // full player route continues to the client pointer at globals+0x10.
 if(address==MAIN+0x114A840&&(position+1>=core_count||baseline[position+1].address!=G+0x10))return CLEAR_SKILLS;
 const uint64_t ownership[]={MAIN+0x114A840,G+0x10,C+0x84,C+0xBA0,C+0x948,SEL,PLAYER,PLAYER+0xD688,C+0xA98,POOL+0x100,POOL+0x168,POOL+0x120,PAGES,ACTOR,C+0x9A0,SG,SP+0x100,SP+0x168,SP+0x120,ST,SA,SA+0x168,GROUP+4,GROUP+0x10,OWNER+0x10};
 for(unsigned i=0;i<sizeof ownership/sizeof ownership[0];i++)if(address==ownership[i])return CLEAR_ALL;
 for(unsigned i=0;i<16;i++)if(address==sbucket(i)||(address>=snode(i,0)&&address<snode(i,0)+0x100))return i<8?(int)i:i<14?STATS_CRIT_CHANCE:STATS_RESOURCE_COST;
 if(address==CDR)return STATS_CDR;
 for(unsigned i=0;i<6;i++){
  if(address==PLAYER+0x134C+16*i)return CLEAR_SKILLS;
  if(address==name_lookup_buckets[i]||address==DIR+(20+i)*8||address==DESC(0)+i*24||address==ARENA(0)+i*128||address==E+i*24||address==TEXT(i)||(address>=node(i,0)&&address<=node(i,7)))return NAME_SCOPE+(int)i;
 }
 return CLEAR_SKILLS; // Locale, asset registry and string-table ownership serve all names.
}
static void module_faults(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 NF baseline;EdenDsmodHostApi h=module_fixture(&baseline);PlayerProbeResult player;player_probe(&h,&player);CHECK(player.available&&baseline.maps==PLAYER_MAX_READS);Access player_trace[PLAYER_MAX_READS];memcpy(player_trace,trace,sizeof player_trace);reset(&baseline);DetailsProbeResult details;details_probe(&h,&details);CHECK(details.shared_identity_valid);const unsigned core_count=baseline.maps;reset(&baseline);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);const unsigned count=baseline.maps;CHECK(count>core_count+PLAYER_MAX_READS);const unsigned closing_start=count-PLAYER_MAX_READS;CHECK(!memcmp(trace+closing_start,player_trace,sizeof player_trace));Access baseline_trace[MODULE_READ_BOUND];memcpy(baseline_trace,trace,count*sizeof *trace);a->destroy(s);release(&baseline);
 for(unsigned mapping=0;mapping<2;mapping++)for(unsigned n=1;n<=count;n++){
  NF f;h=module_fixture(&f);s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);check_identity();reset(&f);clock_tick=16;if(mapping)f.mapfail=n;else f.fail=n;a->sample(s,&h);
  const int scope=failure_scope(baseline_trace,n-1,core_count,closing_start);
  if(scope==CLEAR_ALL){if(strcmp(shown.level,"Level unavailable"))fprintf(stderr,"fault mapping=%u position=%u address=%llx size=%zu core=%u level=%s\n",mapping,n,(unsigned long long)baseline_trace[n-1].address,baseline_trace[n-1].size,core_count,shown.level);module_clear();}
  else {check_identity();for(unsigned i=0;i<STATS_COUNT;i++)CHECK((!strcmp(shown.stats[i],"Unavailable"))==(scope==(int)i));for(unsigned i=0;i<6;i++)CHECK((!strcmp(shown.skills[i],"Unavailable"))==(scope==CLEAR_SKILLS||scope==NAME_SCOPE+(int)i));}
  CHECK(f.maps<=MODULE_READ_BOUND&&f.calls<=f.maps&&f.bytes<=MODULE_BYTE_BOUND);a->destroy(s);release(&f);
 }
}
static void module_late(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 for(unsigned failure=0;failure<2;failure++)for(unsigned kind=0;kind<5;kind++){
  NF f;EdenDsmodHostApi h=module_fixture(&f);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);
  // Mutate at the last skill-name payload; closing locale/selection/player gates must catch it.
  unsigned trigger=0;for(unsigned i=0;i<f.maps;i++)if(trace[i].address==TEXT(5)&&trace[i].size==512)trigger=i+1;CHECK(trigger);reset(&f);clock_tick=16;f.mutate=trigger;if(failure)f.fail=trigger;
  f.addr=kind==0?C+0x84:kind==1?PLAYER+0x134C:kind==2?MAIN+0x1A63CEC:kind==3?G+0x1148:IP+40;f.value=kind==0?0:kind==1?123:kind==4?RHANDLE+0x40000:2;f.size=4;
  a->sample(s,&h);CHECK(f.seen==1);if(kind==0)module_clear();else{check_identity();check_stats(0);for(unsigned i=0;i<6;i++)CHECK(!strcmp(shown.skills[i],"Unavailable"));}
  a->destroy(s);release(&f);
 }
}
static void module_presentation(void){NF f;EdenDsmodHostApi h=module_fixture(&f);const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);void*s=a->create(&h,NULL);CHECK(s);
 memset(loc(&f,TEXT(0),512),'W',511);put(&f,TEXT(0)+511,0,1);a->sample(s,&h);CHECK(!strcmp(shown.skills[0],"Unavailable")&&!strcmp(shown.skills[1],"Synthetic skill 1"));reset(&f);
 put(&f,PLAYER+0x134C,UINT32_MAX,4);put(&f,PLAYER+0x1350,1234,4);a->sample(s,&h);CHECK(!strcmp(shown.skills[0],"Unassigned"));reset(&f);
 put(&f,PLAYER+0x134C,30744,4);put(&f,PLAYER+0x1350,UINT32_MAX,4);a->sample(s,&h);CHECK(!strcmp(shown.skills[0],"Unavailable"));reset(&f);
 h.build_id[31]^=1;a->sample(s,&h);module_clear();CHECK(!f.maps);h.build_id[31]^=1;a->sample(s,&h);module_clear();CHECK(!f.maps);a->destroy(s);release(&f);
}
static void module_optional_identity(void){
 NF f;EdenDsmodHostApi h=module_fixture(&f);const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);void*s=a->create(&h,NULL);CHECK(s);
 for(unsigned value=0;value<=7;value++){reset(&f);put(&f,PLAYER+0xD688,value,4);a->sample(s,&h);CHECK(!strcmp(shown.level,"Level 42")&&shown.class_index==(value<=6?(int64_t)value+1:0));CHECK(!strcmp(shown.paragon,"Paragon 17"));check_stats(0);}
 reset(&f);put(&f,PLAYER+0xD688,1,4);put(&f,PLAYER+0xD690,0,4);a->sample(s,&h);CHECK(shown.class_index==2&&!strcmp(shown.paragon,"Paragon 0"));check_stats(0);
 reset(&f);put(&f,PLAYER+0xD690,20001,4);a->sample(s,&h);CHECK(shown.class_index==2&&!strcmp(shown.level,"Level 42")&&!strcmp(shown.paragon,"Paragon unavailable"));check_stats(0);a->destroy(s);release(&f);
}
static void image_forbidden(void*p,uint32_t width,uint32_t height,const uint8_t*data,size_t n){(void)p;(void)width;(void)height;(void)data;(void)n;CHECK(0);}
static void transparent_image(void*p,uint32_t width,uint32_t height,const uint8_t*data,size_t n){unsigned*calls=p;(*calls)++;CHECK(width==1&&height==1&&n==4&&data);for(unsigned i=0;i<4;i++)CHECK(data[i]==0);}
static void cleared_image(const EdenDsmodModuleExtensions*e,void*s,const EdenDsmodHostApi*h,const char*key){unsigned calls=0;CHECK(e->load_image(s,h,key,&calls,transparent_image));CHECK(calls==1);}
static void current_image(void*p,uint32_t width,uint32_t height,const uint8_t*data,size_t n){unsigned*calls=p;(*calls)++;CHECK(width==MAP_IMAGE_WIDTH&&height==MAP_IMAGE_HEIGHT&&n==MAP_IMAGE_BYTES&&data&&data[3]==255);}
static void optional_memory(NF*f){
 const uint64_t inventory=SG+0xD000,buckets=SG+0xD100,visible_world=SG+0xB000;
 put(f,SA+0xC8,inventory,8);put(f,inventory,1,4);put(f,inventory+0x18,buckets,8);
 for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++){
  const uint32_t key=i+1;uint32_t h=UINT32_C(0x811C9DC5);for(unsigned j=0;j<4;j++)h=(h^((key>>(j*8))&255u))*UINT32_C(0x01000193);
  const uint64_t entry=SG+0xD400+64*i;put(f,buckets+8*(h&31u),entry,8);put(f,entry+8,key,4);put(f,entry+0x10,entry+24,8);put(f,entry+24,entry+32,8);put(f,entry+32,UINT32_MAX,4);
 }
 put(f,C+0x800,visible_world,8);put(f,visible_world+0x38,77,4);put(f,SA+0xA0,77,4);
}
static struct {uint64_t address;unsigned armed,kind,fail,triggered;} optional_transition;
static EdenDsmodBool optional_transition_read(void*p,uint64_t address,void*out,size_t size){
 NF*f=p;
 if(optional_transition.armed&&address==optional_transition.address){
  optional_transition.armed=0;optional_transition.triggered++;
  if(optional_transition.kind==0)put(f,C+0x84,0,4);
  else {put(f,PLAYER+4,0x56790000,4);put(f,SA,0x56790000,4);}
  if(optional_transition.fail)f->fail=f->maps;
 }
 return readmem(p,address,out,size);
}
static void module_late_optional_transition(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 const EdenDsmodModuleExtensions*e=eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION,EDEN_DSMOD_EXT_HASH);
 for(unsigned where=0;where<3;where++)for(unsigned kind=0;kind<2;kind++)for(unsigned fail=0;fail<2;fail++){
  NF f;EdenDsmodHostApi h=module_fixture(&f);optional_memory(&f);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);check_identity();CHECK(shown.map_available==1);
  for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)CHECK(!strcmp(shown.gear[i],"Empty slot"));
  CHECK(e->on_action(s,"inspect_equipment",6)&&e->on_action(s,"map_pin",0));reset(&f);clock_tick=2;a->sample(s,&h);
  CHECK(!strcmp(shown.selected_slot,"Feet")&&!strcmp(shown.selected_name,"Empty slot")&&strstr(shown.map_status,"Location pinned"));char old_map_key[80];snprintf(old_map_key,sizeof old_map_key,"%s",shown.map_key);
  reset(&f);clock_tick=where==2?62:17;memset(&optional_transition,0,sizeof optional_transition);optional_transition.address=where==2?C+0xB18:where?C+0x800:SA+0xC8;optional_transition.armed=1;optional_transition.kind=kind;optional_transition.fail=fail;h.read_memory=optional_transition_read;
  a->sample(s,&h);CHECK(optional_transition.triggered==1);module_clear();CHECK(strstr(shown.map_status,"unavailable"));cleared_image(e,s,&h,old_map_key);CHECK(!e->on_action(s,"inspect_equipment",0)&&!e->on_action(s,"map_pin",0));
  // Clearing must discard the cached ownership, selection and pin, so a fresh
  // sample restores current data immediately even inside the old refresh period.
  put(&f,C+0x84,1,4);put(&f,PLAYER+4,0x56780000,4);put(&f,SA,0x56780000,4);reset(&f);f.fail=0;clock_tick=where==2?63:18;a->sample(s,&h);check_identity();check_stats(0);CHECK(shown.map_available==1&&count_access(SA+0xC8,f.maps));CHECK(!strstr(shown.map_status,"Location pinned"));CHECK(!strcmp(shown.selected_name,"Select an equipment slot"));a->destroy(s);release(&f);
 }
}
static void module_cached_world_changes(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 const EdenDsmodModuleExtensions*e=eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION,EDEN_DSMOD_EXT_HASH);
 for(unsigned kind=0;kind<4;kind++){
  NF f;EdenDsmodHostApi h=module_fixture(&f);optional_memory(&f);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);CHECK(shown.map_available&&e->on_action(s,"map_pin",0));reset(&f);clock_tick=2;a->sample(s,&h);CHECK(shown.map_available&&strstr(shown.map_status,"Location pinned"));unsigned image_calls=0;CHECK(e->load_image(s,&h,shown.map_key,&image_calls,current_image)&&image_calls==1);char old_key[80];snprintf(old_key,sizeof old_key,"%s",shown.map_key);
  reset(&f);clock_tick=3;
  if(kind==0){put(&f,SA+0xA0,78,4);put(&f,SG+0xB038,78,4);} // New valid world, unchanged player.
  if(kind==1)put(&f,SA+0xA0,UINT32_MAX,4);
  if(kind==2)put(&f,C+0x800,0,8);
  if(kind==3)put(&f,SG+0xB038,78,4); // Incoherent visible-world observation.
  a->sample(s,&h);check_identity();check_stats(0);CHECK(!shown.map_available&&!count_access(SA+0xC8,f.maps));CHECK(strcmp(old_key,shown.map_key));CHECK(!e->on_action(s,"map_pin",0));cleared_image(e,s,&h,old_key);
  put(&f,SA+0xA0,78,4);put(&f,SG+0xB038,78,4);put(&f,C+0x800,SG+0xB000,8);reset(&f);clock_tick=4;a->sample(s,&h);check_identity();check_stats(0);CHECK(shown.map_available&&count_access(SA+0xC8,f.maps)&&!strstr(shown.map_status,"Location pinned"));a->destroy(s);release(&f);
 }
}
static void module_cached_world_failures(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 const EdenDsmodModuleExtensions*e=eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION,EDEN_DSMOD_EXT_HASH);
 NF baseline;EdenDsmodHostApi h=module_fixture(&baseline);optional_memory(&baseline);PlayerProbeResult p;player_probe(&h,&p);CHECK(p.available);reset(&baseline);uint32_t world=UINT32_MAX;CHECK(map_current_world(&h,&p.identity,&world)&&world==77);const unsigned context_count=baseline.maps;CHECK(context_count<=MAP_CONTEXT_MAX_READS&&baseline.bytes<=MAP_CONTEXT_MAX_BYTES);Access context_trace[MAP_CONTEXT_MAX_READS];memcpy(context_trace,trace,context_count*sizeof *trace);
 reset(&baseline);DetailsProbeResult d;details_probe(&h,&d);CHECK(d.shared_identity_valid);const unsigned context_start=baseline.maps;reset(&baseline);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);reset(&baseline);clock_tick=2;a->sample(s,&h);CHECK(baseline.maps==context_start+context_count+PLAYER_MAX_READS);CHECK(!memcmp(trace+context_start,context_trace,context_count*sizeof *trace));a->destroy(s);release(&baseline);
 for(unsigned mapping=0;mapping<2;mapping++)for(unsigned i=0;i<context_count;i++){
  NF f;h=module_fixture(&f);optional_memory(&f);s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);CHECK(shown.map_available&&e->on_action(s,"map_pin",0));reset(&f);clock_tick=2;if(mapping)f.mapfail=context_start+i+1;else f.fail=context_start+i+1;
  a->sample(s,&h);check_identity();check_stats(0);CHECK(!shown.map_available&&!e->on_action(s,"map_clear_pin",0));CHECK(!count_access(SA+0xC8,f.maps));for(unsigned slot=0;slot<EQUIPMENT_SLOT_COUNT;slot++)CHECK(!strcmp(shown.gear[slot],"Empty slot"));
  reset(&f);f.fail=f.mapfail=0;clock_tick=3;a->sample(s,&h);CHECK(shown.map_available&&count_access(SA+0xC8,f.maps)&&!strstr(shown.map_status,"Location pinned"));a->destroy(s);release(&f);
 }
}
static void module_navigation_cadence(void){
 const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);
 const EdenDsmodModuleExtensions*e=eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION,EDEN_DSMOD_EXT_HASH);
 NF f;EdenDsmodHostApi h=module_fixture(&f);optional_memory(&f);void*s=a->create(&h,NULL);CHECK(s);a->sample(s,&h);check_identity();check_stats(0);CHECK(shown.map_available&&count_access(C+0xB18,f.maps));
 // This fixture deliberately has no navigation resources. Its failed terrain
 // lookup remains optional and is retried at 1Hz while maps refresh at 4Hz.
 const struct {uint64_t tick;unsigned map_refresh,navigation_refresh;} samples[]={
  {2,0,0},{15,0,0},{16,1,0},{31,1,0},{46,1,0},{60,0,0},{61,1,1}
 };
 for(unsigned i=0;i<sizeof samples/sizeof samples[0];i++){
  reset(&f);clock_tick=samples[i].tick;a->sample(s,&h);check_identity();check_stats(0);CHECK(shown.map_available);
  CHECK((count_access(SA+0xC8,f.maps)>0)==samples[i].map_refresh);
  CHECK((count_access(C+0xB18,f.maps)>0)==samples[i].navigation_refresh);
 }
 // A different player is never allowed to inherit the old terrain deadline.
 reset(&f);clock_tick=62;put(&f,PLAYER+4,0x56790000,4);put(&f,SA,0x56790000,4);a->sample(s,&h);check_identity();check_stats(0);CHECK(shown.map_available&&count_access(C+0xB18,f.maps));CHECK(e->on_action(s,"map_pin",0));
 // A world transition invalidates the map immediately and triggers fresh
 // terrain on the next sample, even though the one-second deadline is later.
 reset(&f);clock_tick=63;put(&f,SA+0xA0,78,4);put(&f,SG+0xB038,78,4);a->sample(s,&h);check_identity();check_stats(0);CHECK(!shown.map_available&&!count_access(C+0xB18,f.maps));
 reset(&f);clock_tick=64;a->sample(s,&h);check_identity();check_stats(0);CHECK(shown.map_available&&count_access(C+0xB18,f.maps)&&!strstr(shown.map_status,"Location pinned"));a->destroy(s);release(&f);
}
static void module_extensions(void){
 CHECK(!eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION+1,EDEN_DSMOD_EXT_HASH));CHECK(!eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION,0));const EdenDsmodModuleExtensions*e=eden_dsmod_get_extensions(EDEN_DSMOD_EXT_VERSION,EDEN_DSMOD_EXT_HASH);CHECK(e&&e->version==EDEN_DSMOD_EXT_VERSION&&e->struct_size==sizeof *e&&e->abi_hash==EDEN_DSMOD_EXT_HASH);
 NF f;EdenDsmodHostApi h=module_fixture(&f);const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);void*s=a->create(&h,NULL);CHECK(s);e->configure(s,NULL);a->sample(s,&h);CHECK(!e->on_action(s,"unrecognized",0));CHECK(!e->load_image(s,&h,"module:exploration:stale",NULL,image_forbidden));cleared_image(e,s,&h,shown.map_key);
 const char*bad[]={"","file:assets/map.png","module:other:1","module:exploration:","module:exploration:-1","module:exploration:+1","module:exploration:1.0","module:exploration:1x","module:exploration: 1","module:exploration:18446744073709551616","module:exploration:000000000000000000000"};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++)CHECK(!e->load_image(s,&h,bad[i],NULL,image_forbidden));
 reset(&f);for(unsigned i=0;i<4096;i++){char key[80];snprintf(key,sizeof key,"module:exploration:%u",i);cleared_image(e,s,&h,key);}cleared_image(e,s,&h,"module:exploration:18446744073709551615");CHECK(!f.maps&&!f.calls);
 h.build_id[0]^=1;a->sample(s,&h);module_clear();CHECK(!e->on_action(s,"map.pin",0)&&!f.maps);a->destroy(s);release(&f);
}
static void module_gates(void){const EdenDsmodModuleApi*a=eden_dsmod_get_module(1,EDEN_DSMOD_MODULE_ABI_HASH);CHECK(!eden_dsmod_get_module(2,EDEN_DSMOD_MODULE_ABI_HASH));CHECK(!eden_dsmod_get_module(1,0));CHECK(!a->supports_build(NULL)&&!a->supports_build("2607A74F5DF7754C"));CHECK(a->capabilities==(EDEN_DSMOD_CAP_NO_TICK_WHEN_HIDDEN|EDEN_DSMOD_CAP_EXTENSIONS));
 for(unsigned i=0;i<47;i++){NF f;EdenDsmodHostApi h=module_fixture(&f);if(i<32)h.build_id[i]^=1;if(i==32)h.title_id=0;if(i==33)h.struct_size=8;if(i==34)h.abi_hash=0;if(i==35)h.abi_version=2;if(i==36)h.userdata=NULL;if(i==37)h.capabilities=0;if(i==38)h.read_memory=NULL;if(i==39)h.is_mapped=NULL;if(i==40)h.publish_text=NULL;if(i==41)h.publish_i64=NULL;if(i==42)h.begin_output=NULL;if(i==43)h.end_output=NULL;if(i==44)h.get_tick=NULL;if(i==45)h.capabilities=EDEN_DSMOD_CAP_NO_TICK_WHEN_HIDDEN;if(i==46)h.capabilities=EDEN_DSMOD_CAP_EXTENSIONS;CHECK(!a->create(&h,NULL)&&!f.maps);release(&f);}
}
int main(void){module_success();module_faults();module_late();module_presentation();module_optional_identity();module_late_optional_transition();module_cached_world_changes();module_cached_world_failures();module_navigation_cadence();module_extensions();module_gates();puts("PASS module: stats/identity, traced read/map/closing/world faults, late transitions/cache clearing, 1Hz terrain/4Hz map cadence, transparent retired images, extension/build gates; no writes/calls");return 0;}
