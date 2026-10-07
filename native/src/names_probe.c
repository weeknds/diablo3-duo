// SPDX-License-Identifier: GPL-3.0-or-later
#include "names_probe.h"
#include "probe_internal.h"

/* Exact-build evidence and field masks: see ../README.txt and the independent
 * localized-name audit. Every address and copied string lives only in this call.
 * The journal closes leaves before owners after ALL second text observations. */
#define JOURNAL_CAP 118u
#define SHARED (-1)
typedef struct Meta {
    uint64_t address;
    uint32_t size, alignment;
    int field, late;
    unsigned char data[128], significant[128];
} Meta;
typedef struct Context { uint64_t base, end, names, names_end; } Context;
typedef struct Block { uint64_t address, data, end; uint32_t used; } Block;
typedef struct Catalog {
    uint64_t directory, entries, entries_end, buckets;
    uint32_t directory_count, mask, node_count;
    Context contexts[3]; Block blocks[8]; unsigned block_count;
} Catalog;
typedef struct Payload {
    int complete;
    uint64_t internal_address, text_address;
    uint32_t internal_size, text_size;
    unsigned char internal[128], text[512];
} Payload;
typedef struct NamesWork {
    const EdenDsmodHostApi *original;
    EdenDsmodHostApi wrapped;
    PlayerProbeResult scratch;
    Reader reader;
    uint32_t attempts, bytes, limit_reads, limit_bytes;
    Meta journal[JOURNAL_CAP]; unsigned count;
    int field, shared_ok, field_ok[6];
    Catalog catalog; Payload payload[6];
} NamesWork;
static uint32_t u32(const unsigned char *p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t u64(const unsigned char *p) { uint64_t v; memcpy(&v,p,8); return v; }
static int extent(uint64_t base,uint64_t size,uint64_t *end) {
    return base && size && add(base,size,end);
}
static int aligned(uint64_t p,uint32_t a) { return p && p%a==0; }
static EdenDsmodBool budget_map(void *p,uint64_t a,uint64_t n) {
    NamesWork *w=p;
    if(n>512 || w->attempts>=w->limit_reads || w->bytes>w->limit_bytes || n>w->limit_bytes-w->bytes)return EDEN_DSMOD_FALSE;
    w->attempts++; w->bytes+=(uint32_t)n;
    return w->original->is_mapped(w->original->userdata,a,n);
}
static EdenDsmodBool copy_memory(void *p,uint64_t a,void *out,size_t n) {
    NamesWork *w=p;return w->original->read_memory(w->original->userdata,a,out,n);
}
static int copy(NamesWork *w,uint64_t a,void *out,uint32_t n,uint32_t alignment) {
    memset(out,0,n);return read_at(&w->reader,a,out,n,alignment);
}
static Meta *meta(NamesWork *w,uint64_t a,uint32_t n,uint32_t alignment) {
    if(w->count>=JOURNAL_CAP || n>128)return NULL;
    Meta *m=&w->journal[w->count];*m=(Meta){.address=a,.size=n,.alignment=alignment,.field=w->field};
    if(!copy(w,a,m->data,n,alignment))return NULL;
    w->count++;return m;
}
static void significant(Meta *m,unsigned start,unsigned n) { memset(m->significant+start,1,n); }
static Meta *whole(NamesWork *w,uint64_t a,uint32_t n,uint32_t alignment) {
    Meta *m=meta(w,a,n,alignment);if(m)significant(m,0,n);return m;
}
static int mptr(NamesWork *w,uint64_t a,uint64_t *p) {
    Meta *m=whole(w,a,8,8);if(!m)return 0;*p=u64(m->data);return aligned(*p,8);
}
static int mword(NamesWork *w,uint64_t a,uint32_t *v) {
    Meta *m=whole(w,a,4,4);if(!m)return 0;*v=u32(m->data);return 1;
}
static uint32_t fnv(uint32_t x) {
    uint32_t h=UINT32_C(0x811C9DC5);
    for(unsigned i=0;i<4;i++){h^=(x>>(8*i))&255u;h*=UINT32_C(0x01000193);}return h;
}
static int printable(const unsigned char *p,uint32_t n,uint32_t *length) {
    for(uint32_t i=0;i<n;i++) {
        unsigned b=p[i];if(!b){*length=i;return i!=0;}
        if(b<32 || b>126 || b=='{' || b=='}' || b=='<' || b=='>')return 0;
    }
    return 0;
}
static int identity(NamesWork *w,const PlayerProbeIdentity *expected) {
    Snapshot now;
    return snapshot(&w->reader,&now) && !memcmp(&now,expected,sizeof now);
}
static int pairs(NamesWork *w,uint64_t player,StoredSkillPair out[6]) {
    int valid=1;
    for(unsigned i=0;i<6;i++) {uint64_t a;int ok=add(player,0x134C+16*i,&a) && copy(w,a,&out[i],8,4);if(!ok)valid=0;}
    return valid;
}
static int flat_owner(const Catalog *c,uint64_t descriptor) {
    if(!aligned(descriptor,8))return -1;
    int owner=-1;
    for(unsigned i=0;i<3;i++) {
        const Context *x=&c->contexts[i];
        if(x->base && descriptor>=x->base && descriptor<x->end) {
            if(x->end-descriptor<24 || (descriptor-x->base)%24 || owner!=-1)return -1;
            owner=(int)i;
        }
    }
    return owner;
}
static Meta *descriptor(NamesWork *w,uint32_t id,uint32_t group,int *owner) {
    Catalog *c=&w->catalog;
    if(id>0xFFFFFF || id>=c->directory_count)return NULL;
    uint64_t address;
    if(!add(c->directory,(uint64_t)id*8,&address))return NULL;
    Meta *slot=whole(w,address,8,8);if(!slot)return NULL;
    uint64_t d=u64(slot->data);*owner=flat_owner(c,d);if(*owner<0)return NULL;
    Meta *m=meta(w,d,24,8);if(!m)return NULL;
    significant(m,0,4);if(group==0x1D)significant(m,8,8);
    return u32(m->data)==(id<<8|group)?m:NULL;
}
static int hash_owner(Meta *m,uint32_t stride,uint64_t *buckets,uint32_t *mask) {
    significant(m,0,28);significant(m,0x20,12);significant(m,0x38,4);
    significant(m,0x48,4);significant(m,0x58,8);significant(m,0x70,4);
    const unsigned char *d=m->data;uint64_t embedded,end;
    uint32_t count=u32(d+0x18);
    if(!count || (count&(count-1)) || count>(uint32_t)INT32_MAX/8 ||
       u32(d)!=count-1 || u32(d+0x28)!=count*8 ||
       !add(m->address,0x48,&embedded) || u64(d+8)!=embedded ||
       u32(d+0x38)!=0x600DF00D || u32(d+0x70)!=0x600DF00D || u32(d+0x48)!=stride ||
       !aligned(u64(d+0x10),4) || u64(d+0x10)!=u64(d+0x20) ||
       !extent(u64(d+0x10),(uint64_t)count*8,&end))return 0;
    *buckets=u64(d+0x10);*mask=count-1;return 1;
}
static int shared(NamesWork *w,const PlayerProbeIdentity *expected) {
    const uint64_t main=w->original->main_base;
    Catalog *c=&w->catalog;uint64_t g,m,v,a,p,indices,end;
    Meta *x;
    if(!mptr(w,main+ROOT_OFFSET,&g) || g!=expected->globals || g>UINT64_MAX-0x114C ||
       !mptr(w,main+0x1A216F0,&m) || m>UINT64_MAX-0x7DC0)return 0;
    x=meta(w,m+0x11E0,12,8);if(!x)return 0;significant(x,0,12);
    c->directory=u64(x->data);c->directory_count=u32(x->data+8);
    if(!aligned(c->directory,8) || !extent(c->directory,(uint64_t)c->directory_count*8,&end))return 0;
    x=whole(w,main+0x1A63CEC,8,4);if(!x)return 0;x->late=2;
    x=whole(w,g+0x1148,4,4);if(!x)return 0;x->late=1;if(u32(x->data))return 0;
    if(!mptr(w,g+0x1100,&v))return 0;
    x=whole(w,v,24,8);if(!x)return 0;
    uint64_t table=u64(x->data),length=u64(x->data+8),capacity=u64(x->data+16)&UINT64_C(0x7FFFFFFFFFFFFFFF);
    if(!aligned(table,8)||length<=0x2E5 || length>capacity || capacity>UINT64_MAX/16 || !extent(table,capacity*16,&end))return 0;
    x=meta(w,table+0x2E5*16,16,8);if(!x)return 0;significant(x,8,4);
    uint32_t sno=u32(x->data+8);if(sno>0xFFFFFF)return 0;
    for(unsigned i=0;i<3;i++) {
        uint64_t origin,k,names;uint32_t cap,used;
        if(!add(m,(uint64_t)i*0x598,&origin) || origin>UINT64_MAX-0x5F8 || !mptr(w,origin+0x118,&k))return 0;
        x=meta(w,k,24,8);if(!x)return 0;significant(x,0,8);significant(x,20,4);
        Context *ct=&c->contexts[i];uint32_t slots=u32(x->data+20);ct->base=u64(x->data);
        if(slots) {if(slots>(uint32_t)INT32_MAX/24 || !aligned(ct->base,8) || !extent(ct->base,(uint64_t)slots*24,&ct->end))return 0;}
        else if(ct->base)return 0;
        /* A zero-sized inactive arena may have a null base. */
        x=whole(w,origin+0x438,8,8);if(!x)return 0;names=u64(x->data);
        if(!mword(w,origin+0x2AC,&cap) || !mword(w,origin+0x5F4,&used) || used>cap)return 0;
        if(cap && !extent(names,cap,&end))return 0;
        if(!cap && (names||used))return 0;
        ct->names=names;ct->names_end=names+used;
    }
    for(unsigned i=0;i<3;i++)for(unsigned j=0;j<i;j++) {
        Context *q=&c->contexts[i],*r=&c->contexts[j];
        if(q->base && r->base && q->base<r->end && r->base<q->end)return 0;
    }
    int owner;
    if(!descriptor(w,sno,0x2A,&owner) || !mptr(w,main+0x19DAFB0,&a))return 0;
    x=meta(w,main+0x1955C58,16,8);if(!x)return 0;significant(x,0,4);significant(x,8,8);
    uint32_t index_count=u32(x->data);indices=u64(x->data+8);
    if(sno>=index_count || !aligned(indices,4) || !extent(indices,(uint64_t)index_count*4,&end))return 0;
    x=whole(w,a,8,8);if(!x || u64(x->data)!=main+0x10C7948)return 0;
    if(a>UINT64_MAX-0x80)return 0;
    x=meta(w,a+0x20,12,8);if(!x)return 0;significant(x,0,12);p=u64(x->data);
    uint32_t group,size;
    if(!aligned(p,8) || !mword(w,a+0x50,&group) || group!=0x2A || !mword(w,a+0x7C,&size) || size!=40 || p>UINT64_MAX-0x130)return 0;
    x=meta(w,p+0x100,48,8);if(!x)return 0;significant(x,0,8);significant(x,0x20,8);
    uint32_t slots=u32(x->data);uint64_t base=u64(x->data+0x20);
    if(!slots || slots>0x3FFFE || u32(x->data+4)!=16 || !aligned(base,16) || !extent(base,(uint64_t)slots*16,&end))return 0;
    uint32_t handle;
    if(!mword(w,indices+(uint64_t)sno*4,&handle) || handle==UINT32_MAX || (handle&0x3FFFF)>=slots)return 0;
    x=meta(w,base+(uint64_t)(handle&0x3FFFF)*16,16,8);if(!x)return 0;
    significant(x,0,4);significant(x,7,1);significant(x,8,8);
    uint64_t object=u64(x->data+8);
    if(u32(x->data)!=handle || x->data[7]!=0x2A || !aligned(object,4))return 0;
    x=meta(w,object,40,4);if(!x)return 0;
    significant(x,0,4);significant(x,8,16);significant(x,0x1C,12);
    uint32_t entries=u32(x->data+12),bytes=u32(x->data+0x1C),flags=u32(x->data+8);
    uint64_t h=u64(x->data+0x20);c->entries=u64(x->data+0x10);
    if(u32(x->data)!=sno || !(flags&0x40) || (flags&2) || !entries || bytes%24 || entries!=bytes/24 ||
       !aligned(c->entries,4) || !extent(c->entries,bytes,&c->entries_end) || !aligned(h,4))return 0;
    x=meta(w,h,128,4);if(!x || !hash_owner(x,24,&c->buckets,&c->mask))return 0;
    c->node_count=u32(x->data+4);uint64_t block=u64(x->data+0x58);
    if(!c->node_count || c->node_count>entries || !block)return 0;
    x=meta(w,main+0x19DAFE8,128,4);uint64_t buckets;uint32_t mask;
    if(!x || !hash_owner(x,16,&buckets,&mask))return 0;
    x=whole(w,buckets+(uint64_t)(fnv(sno)&mask)*8,8,4);if(!x || u64(x->data))return 0;
    while(block) {
        if(c->block_count>=8 || !aligned(block,16))return 0;
        for(unsigned i=0;i<c->block_count;i++)if(c->blocks[i].address==block)return 0;
        x=meta(w,block,48,16);if(!x)return 0;
        significant(x,0,16);significant(x,0x18,12);significant(x,0x2C,4);
        uint32_t capacity_b=u32(x->data+0x18),used=u32(x->data+0x20);
        uint64_t data=u64(x->data+8);
        if(!capacity_b || capacity_b>((uint32_t)INT32_MAX-0x50)/24 || u32(x->data+0x1C)!=24 ||
           used>capacity_b || u32(x->data+0x2C)>capacity_b || block>UINT64_MAX-0x50 || data!=block+0x50 ||
           !extent(block,0x50+(uint64_t)capacity_b*24,&end))return 0;
        for(unsigned i=0;i<c->block_count;i++)if(block<c->blocks[i].end && c->blocks[i].address<end)return 0;
        c->blocks[c->block_count++]=(Block){block,data,end,used};block=u64(x->data);
    }
    return 1;
}
static int node_member(const Catalog *c,uint64_t node) {
    if(!aligned(node,8))return 0;
    unsigned owners=0;
    for(unsigned i=0;i<c->block_count;i++){const Block *b=&c->blocks[i];if(node>=b->data && node<b->end && b->end-node>=24 && (node-b->data)%24==0 && (node-b->data)/24<b->used)owners++;}
    return owners==1;
}
static int discover(NamesWork *w,unsigned field,uint32_t power) {
    Catalog *c=&w->catalog;Payload *p=&w->payload[field];int owner;
    Meta *d=descriptor(w,power,0x1D,&owner);if(!d)return 0;
    uint64_t internal=u64(d->data+8);const Context *ct=&c->contexts[owner];
    if(internal<ct->names || internal>=ct->names_end)return 0;
    p->internal_address=internal;p->internal_size=(uint32_t)((ct->names_end-internal)<128?ct->names_end-internal:128);
    uint32_t length;
    if(!copy(w,internal,p->internal,p->internal_size,1) || !printable(p->internal,p->internal_size,&length))return 0;
    char key[133];memcpy(key,p->internal,length);memcpy(key+length,"_name",6);
    uint32_t hash=0;for(unsigned i=0;i<length+5;i++)hash=33*hash+(unsigned char)key[i];
    Meta *m=whole(w,c->buckets+(uint64_t)(fnv(hash)&c->mask)*8,8,4);if(!m)return 0;
    uint64_t node=u64(m->data),seen[8]={0};unsigned count=0;
    while(node) {
        if(count>=8 || count>=c->node_count || !node_member(c,node))return 0;
        for(unsigned i=0;i<count;i++)if(seen[i]==node)return 0;
        seen[count++]=node;
        m=meta(w,node,24,8);if(!m)return 0;significant(m,0,12);significant(m,16,8);
        if(u32(m->data+8)==hash) {
            uint64_t entry=u64(m->data+16);
            if(entry<c->entries || entry>=c->entries_end || c->entries_end-entry<24 || (entry-c->entries)%24)return 0;
            m=meta(w,entry,24,4);if(!m)return 0;significant(m,0,20);
            p->text_address=u64(m->data);p->text_size=u32(m->data+12);
            if(u32(m->data+16)!=hash || !p->text_size || p->text_size>512 ||
               !copy(w,p->text_address,p->text,p->text_size,1) || !printable(p->text,p->text_size,&length))return 0;
            p->complete=1;return 1;
        }
        node=u64(m->data);
    }
    return 0;
}
static void close_one(NamesWork *w,Meta *m) {
    unsigned char current[128]={0};int ok=copy(w,m->address,current,m->size,m->alignment);
    if(ok)for(unsigned i=0;i<m->size;i++)if(m->significant[i] && current[i]!=m->data[i]){ok=0;break;}
    if(!ok){if(m->field==SHARED)w->shared_ok=0;else w->field_ok[m->field]=0;}
}
void names_probe(const EdenDsmodHostApi *host,const PlayerProbeIdentity *expected,
                 const StoredSkillPair accepted[6],NamesProbeResult *out) {
    *out=(NamesProbeResult){.reason="UNAVAILABLE: name lookup"};
    /* Static global spans include override owner and language pair. */
    if(!host || !expected || !accepted || !host->userdata || !host->read_memory || !host->is_mapped ||
       !host->main_base || host->main_base%8 || host->main_size<0x1A63CF4 || host->main_base>UINT64_MAX-host->main_size)return;
    NamesWork w={.original=host,.wrapped=*host,.field=SHARED,
        .limit_reads=NAMES_MAX_READS-NAMES_CLOSING_READS,.limit_bytes=NAMES_MAX_BYTES-NAMES_CLOSING_BYTES};
    w.wrapped.userdata=&w;w.wrapped.is_mapped=budget_map;w.wrapped.read_memory=copy_memory;
    w.reader=(Reader){&w.wrapped,&w.scratch,UINT32_MAX,UINT32_MAX,NULL};
    StoredSkillPair before[6]={{0}},after[6]={{0}};int initial=identity(&w,expected),slots_ok=0;
    if(initial) {
        slots_ok=pairs(&w,expected->player,before) && !memcmp(before,accepted,sizeof before);
        for(unsigned i=0;i<6;i++)if(before[i].power_sno < -1 || (before[i].power_sno!=-1 && (before[i].rune < -1 || before[i].rune>4)))slots_ok=0;
        if(slots_ok) {
            w.shared_ok=shared(&w,expected);
            if(w.shared_ok)for(unsigned i=0;i<6;i++) {
                w.field=(int)i;
                if(before[i].power_sno>=0)w.field_ok[i]=discover(&w,i,(uint32_t)before[i].power_sno);
            }
            /* All second payloads precede all closing metadata. */
            for(unsigned i=0;i<6;i++)if(w.field_ok[i]) {
                Payload *p=&w.payload[i];unsigned char internal[128]={0},text[512]={0};
                int a=copy(&w,p->internal_address,internal,p->internal_size,1);
                int b=copy(&w,p->text_address,text,p->text_size,1);
                if(!a || !b || memcmp(internal,p->internal,p->internal_size) || memcmp(text,p->text,p->text_size))w.field_ok[i]=0;
            }
        }
    }
    /* The reserved suffix cannot be spent by discovery or payload work. No
     * changed route is followed. Read failures do not skip later closing gates. */
    w.limit_reads=NAMES_MAX_READS;w.limit_bytes=NAMES_MAX_BYTES;
    for(unsigned i=w.count;i>0;i--)if(!w.journal[i-1].late)close_one(&w,&w.journal[i-1]);
    for(int late=1;late<=2;late++)for(unsigned i=w.count;i>0;i--)if(w.journal[i-1].late==late)close_one(&w,&w.journal[i-1]);
    int final_pairs=initial?pairs(&w,expected->player,after):0;
    int final_identity=identity(&w,expected);
    out->shared_identity_valid=initial && final_identity;
    out->skills_valid=out->shared_identity_valid && slots_ok && final_pairs && !memcmp(before,after,sizeof before);
    if(out->skills_valid && w.shared_ok)for(unsigned i=0;i<6;i++)if(w.field_ok[i] && w.payload[i].complete) {
        out->fields[i].available=1;memcpy(out->fields[i].text,w.payload[i].text,w.payload[i].text_size);
    }
    out->reads=w.attempts;out->bytes=w.bytes;
    out->reason=out->shared_identity_valid?"UNVERIFIED: bounded cached ASCII names":"UNAVAILABLE: selected player changed";
}
