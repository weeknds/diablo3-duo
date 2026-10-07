// SPDX-License-Identifier: GPL-3.0-or-later
#include "stats_probe.h"
#include "probe_internal.h"
#include <math.h>
#include <stdio.h>

// Exact-build facts: research-analysis/character-stats/ATTRIBUTE-DETAILS-20261006.txt
// and ATTRIBUTE-DETAILS-INDEPENDENT-AUDIT-20261006.txt. CDR default only per
// CDR-NATIVE-DEFAULT-FEASIBILITY-20261006.txt; no guest calls or literal fallback.
static const uint32_t stat_keys[STATS_COUNT]={UINT32_C(0xFFFFF0C9),UINT32_C(0xFFFFF0D3),UINT32_C(0xFFFFF026),UINT32_C(0xFFFFF0B9)};
typedef struct CommonSnapshot {
    Snapshot route;
    uint64_t acd_globals,pool,pages,page,acd,group,owner,buckets;
    uint32_t capacity,shift,acd_identity;
    uint8_t flags;
} CommonSnapshot;
typedef struct CacheNode { uint64_t address,next; uint32_t key,value; } CacheNode;
typedef enum CacheOutcome { CACHE_UNAVAILABLE=0, CACHE_FOUND, CACHE_PROVEN_ABSENT } CacheOutcome;
typedef struct FieldSnapshot { uint64_t head; CacheNode nodes[8]; uint32_t count,bits,definition_id; } FieldSnapshot;
typedef struct StatPass { CommonSnapshot common; FieldSnapshot fields[STATS_COUNT]; CacheOutcome outcome[STATS_COUNT]; const char *reason[STATS_COUNT]; } StatPass;

static int common_snapshot(Reader*r,CommonSnapshot*s,const PlayerProbeIdentity*expected){
    memset(s,0,sizeof *s);
    if(!snapshot(r,&s->route)||memcmp(&s->route,expected,sizeof *expected))return 0;
    const uint32_t low=s->route.acd_id&0xFFFFu;
    if(!pointer(r,s->route.client,0x9A0,&s->acd_globals)||!pointer(r,s->acd_globals,0,&s->pool)||
       !read_offset(r,s->pool,0x100,&s->capacity,4,4)||low>=s->capacity||
       !read_offset(r,s->pool,0x168,&s->shift,4,4)||s->shift>16||!pointer(r,s->pool,0x120,&s->pages)||
       !pointer(r,s->pages,(uint64_t)(low>>s->shift)*8,&s->page)||
       !add(s->page,(uint64_t)(low&((UINT32_C(1)<<s->shift)-1))*0x360,&s->acd)||
       !read_at(r,s->acd,&s->acd_identity,4,4)||s->acd_identity!=s->route.acd_id||
       !pointer(r,s->acd,0x168,&s->group)||!read_offset(r,s->group,4,&s->flags,1,1))return 0;
    // Dirty state affects every stat, but a second matching identity pass may retain level.
    if(s->flags&2u)return 1;
    if(s->flags&4u){
        if(!pointer(r,s->group,0x10,&s->owner)||!read_offset(r,s->owner,0x10,&s->buckets,8,8)||
           !s->buckets||s->buckets%4)return 0;
        // Only this packed shared bucket target is four-aligned; all node pointers stay eight-aligned.
    }else if(!pointer(r,s->group,0x28,&s->buckets))return 0;
    return 1;
}
static CacheOutcome field_snapshot(Reader*r,const CommonSnapshot*c,unsigned field,FieldSnapshot*s,const char**reason){
    memset(s,0,sizeof *s); // Include padding in the two bitwise-compared snapshots.
    *reason="UNAVAILABLE: cache read";
    const uint32_t key=stat_keys[field],hash=key^(key>>12),index=hash&(c->flags&4u?0x1FFu:0xFFu);
    if(!read_offset(r,c->buckets,(uint64_t)index*8,&s->head,8,c->flags&4u?4:8))return CACHE_UNAVAILABLE;
    uint64_t address=s->head;
    for(unsigned i=0;i<8;i++){
        if(!address){*reason="UNAVAILABLE: cache key missing";return CACHE_PROVEN_ABSENT;}
        for(unsigned j=0;j<i;j++)if(address==s->nodes[j].address){*reason="UNAVAILABLE: cache cycle";return CACHE_UNAVAILABLE;}
        CacheNode*n=&s->nodes[i];n->address=address;
        if(!read_at(r,address,&n->next,16,8))return CACHE_UNAVAILABLE;
        s->count=i+1;
        if(n->key==key){s->bits=n->value;return CACHE_FOUND;}
        address=n->next;
    }
    // Only an observed terminal null proves absence. A ninth link is not absence.
    if(!address){*reason="UNAVAILABLE: cache key missing";return CACHE_PROVEN_ABSENT;}
    *reason="UNAVAILABLE: cache node limit";return CACHE_UNAVAILABLE;
}
static int cdr_default_snapshot(Reader*r,FieldSnapshot*s,const char**reason){
    // Exact CDR registration, not a generic key-to-default facility.
    const uint64_t offset=UINT64_C(0x191EAD8)+UINT64_C(64)*UINT64_C(0xD3);
    struct { uint32_t id,bits; } pair={0};
    _Static_assert(sizeof pair==8,"Registration observation must be exactly eight bytes");
    *reason="UNAVAILABLE: CDR default range/read";
    if(r->host->main_size<offset+8 || r->host->main_base>UINT64_MAX-r->host->main_size ||
       !read_offset(r,r->host->main_base,offset,&pair,8,8))return 0;
    *reason="UNAVAILABLE: CDR registration identity";
    if(pair.id!=UINT32_C(0xD3))return 0;
    s->definition_id=pair.id;s->bits=pair.bits;
    // Raw bits, including signed zero/NaN payloads, are compared before formatting.
    return 1;
}
static void acquire_fields(Reader*r,StatPass*p,const CommonSnapshot*common){
    for(unsigned i=0;i<STATS_COUNT;i++){
        if(common->flags&2u){p->reason[i]="UNAVAILABLE: cache dirty";continue;}
        p->outcome[i]=field_snapshot(r,common,i,&p->fields[i],&p->reason[i]);
        if(i==1 && p->outcome[i]==CACHE_PROVEN_ABSENT &&
           !cdr_default_snapshot(r,&p->fields[i],&p->reason[i]))p->outcome[i]=CACHE_UNAVAILABLE;
    }
}
static int usable(CacheOutcome outcome,unsigned field){
    return outcome==CACHE_FOUND || (field==1 && outcome==CACHE_PROVEN_ABSENT);
}

int stats_format(unsigned field,float value,char*text,size_t size){
    if(!text||!size)return 0;
    text[0]=0;
    if(field>=STATS_COUNT||!isfinite(value))return 0;
    int written=-1;
    if(field==0)written=snprintf(text,size,"%.2f",(double)value);
    else if(field==1){
        const float percent=value*100.0f;
        if(!isfinite(percent))return 0;
        written=snprintf(text,size,"%.2f%%",(double)percent);
    }else if(field==2){
        // Implement the audited ordinary-range nearest-even result explicitly.
        // Larger native conversion branches and alternate guest FP modes are unverified.
        if(fabsf(value)>8388607.0f)return 0;
        int32_t rounded=(int32_t)floorf(value);
        const float fraction=value-(float)rounded;
        if(fraction>0.5f||(fraction==0.5f&&rounded%2))rounded++;
        written=snprintf(text,size,"%d",(int)rounded);
    }else{
        const float bonus=value-1.0f,percent=bonus*100.0f;
        if(!isfinite(bonus)||!isfinite(percent))return 0;
        unsigned precision=0;float part=bonus;
        for(;precision<2;precision++){
            // The native formatter uses signed fraction before percent scaling.
            // Avoid undefined int conversion for arbitrary finite input; reject unsupported range.
            if(part<(float)INT32_MIN||part>=2147483648.0f)return 0;
            const float fraction=part-truncf(part);
            if(fraction<=1.0e-6f)break;
            part*=10.0f;
        }
        written=snprintf(text,size,"%s%.*f%%",bonus>=0.0f?"+":"",(int)precision,(double)percent);
    }
    if(written<0||(size_t)written>=size){text[0]=0;return 0;}
    return 1;
}

void stats_probe(const EdenDsmodHostApi*host,const PlayerProbeIdentity*expected,StatsProbeResult*result){
    *result=(StatsProbeResult){.reason="UNAVAILABLE: invalid host/main region"};
    for(unsigned i=0;i<STATS_COUNT;i++)result->fields[i].reason="UNAVAILABLE: shared identity required";
    if(!expected||!host||!host->userdata||!host->read_memory||!host->is_mapped||!host->main_base||
       host->main_base%8||host->main_size<ROOT_OFFSET+8||host->main_base>UINT64_MAX-host->main_size)return;
    PlayerProbeResult work={0};Reader reader={host,&work,STATS_MAX_READS,STATS_MAX_BYTES,NULL};
    StatPass before={0},after={0};
    const int first=common_snapshot(&reader,&before.common,expected);
    if(first){
        acquire_fields(&reader,&before,&before.common);
        acquire_fields(&reader,&after,&before.common);
    }
    // Bracket both field snapshots, including failed reads, with the complete common snapshot.
    // A missing/dirty/unreadable final field must never skip this shared identity recheck.
    const int second=common_snapshot(&reader,&after.common,expected);
    result->reads=work.reads;result->bytes=work.bytes;
    if(!first||!second||memcmp(&before.common,&after.common,sizeof before.common)){
        result->reason="UNAVAILABLE: shared player/ACD/cache identity changed or unreadable";return;
    }
    result->shared_identity_valid=1;
    result->reason="UNVERIFIED RESEARCH: independent cached menu-stat candidates";
    for(unsigned i=0;i<STATS_COUNT;i++){
        if(!usable(before.outcome[i],i)){result->fields[i].reason=before.reason[i];continue;}
        if(!usable(after.outcome[i],i)){result->fields[i].reason=after.reason[i];continue;}
        if(before.outcome[i]!=after.outcome[i] || memcmp(&before.fields[i],&after.fields[i],sizeof before.fields[i])){
            result->fields[i].reason="UNAVAILABLE: stat cache snapshot changed";continue;
        }
        float value;memcpy(&value,&before.fields[i].bits,4);
        char text[128];
        if(!stats_format(i,value,text,sizeof text)){
            result->fields[i].reason="UNAVAILABLE: nonfinite or unsupported formatting range";continue;
        }
        result->fields[i]=(StatValue){.available=1,.raw=value,.reason=before.outcome[i]==CACHE_PROVEN_ABSENT?"UNVERIFIED CDR registered-default candidate":"UNVERIFIED cached menu-stat candidate"};
    }
}
