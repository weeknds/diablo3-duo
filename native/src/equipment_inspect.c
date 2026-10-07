// SPDX-License-Identifier: GPL-3.0-or-later
#include "equipment_inspect.h"
#include "probe_internal.h"

/* Exact-build item base-name route: 0x48A120, 0x6CA860, 0x4F3D30.
 * Resident StringList ownership follows names_probe.c. This on-demand reader
 * copies plain localized base names only; it never calls guest functions. */
#define JOURNAL_CAP 196u
#define CLOSING_READS 220u
#define CLOSING_BYTES 9000u
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
typedef struct InspectionWork {
    const EdenDsmodHostApi *original;
    EdenDsmodHostApi wrapped;
    PlayerProbeResult scratch;
    Reader reader;
    uint32_t attempts, bytes, limit_reads, limit_bytes;
    Meta journal[JOURNAL_CAP]; unsigned count;
    int field, shared_ok, field_ok[1];
    Catalog catalog; Payload payload[1];
} InspectionWork;
static uint32_t u32(const unsigned char *p) { uint32_t v; memcpy(&v,p,4); return v; }
static uint64_t u64(const unsigned char *p) { uint64_t v; memcpy(&v,p,8); return v; }
static int extent(uint64_t base,uint64_t size,uint64_t *end) {
    return base && size && add(base,size,end);
}
static int aligned(uint64_t p,uint32_t a) { return p && p%a==0; }
static EdenDsmodBool budget_map(void *p,uint64_t a,uint64_t n) {
    InspectionWork *w=p;
    if(n>512 || w->attempts>=w->limit_reads || w->bytes>w->limit_bytes || n>w->limit_bytes-w->bytes)return EDEN_DSMOD_FALSE;
    w->attempts++; w->bytes+=(uint32_t)n;
    return w->original->is_mapped(w->original->userdata,a,n);
}
static EdenDsmodBool copy_memory(void *p,uint64_t a,void *out,size_t n) {
    InspectionWork *w=p;return w->original->read_memory(w->original->userdata,a,out,n);
}
static int copy(InspectionWork *w,uint64_t a,void *out,uint32_t n,uint32_t alignment) {
    memset(out,0,n);return read_at(&w->reader,a,out,n,alignment);
}
static Meta *meta(InspectionWork *w,uint64_t a,uint32_t n,uint32_t alignment) {
    if(w->count>=JOURNAL_CAP || n>128)return NULL;
    Meta *m=&w->journal[w->count];*m=(Meta){.address=a,.size=n,.alignment=alignment,.field=w->field};
    if(!copy(w,a,m->data,n,alignment))return NULL;
    w->count++;return m;
}
static void significant(Meta *m,unsigned start,unsigned n) { memset(m->significant+start,1,n); }
static Meta *whole(InspectionWork *w,uint64_t a,uint32_t n,uint32_t alignment) {
    Meta *m=meta(w,a,n,alignment);if(m)significant(m,0,n);return m;
}
static int mptr(InspectionWork *w,uint64_t a,uint64_t *p) {
    Meta *m=whole(w,a,8,8);if(!m)return 0;*p=u64(m->data);return aligned(*p,8);
}
static int mword(InspectionWork *w,uint64_t a,uint32_t *v) {
    Meta *m=whole(w,a,4,4);if(!m)return 0;*v=u32(m->data);return 1;
}
static uint32_t fnv(uint32_t x) {
    uint32_t h=UINT32_C(0x811C9DC5);
    for(unsigned i=0;i<4;i++){h^=(x>>(8*i))&255u;h*=UINT32_C(0x01000193);}return h;
}
static int printable(const unsigned char *p,uint32_t n,uint32_t *length) {
    for(uint32_t i=0;i<n;i++) {
        unsigned b=p[i];if(!b){*length=i;return i!=0;}
        if(b<32 || b>126 || b=='{' || b=='}' || b=='<' || b=='>' || b=='|' || b=='[' || b==']')return 0;
    }
    return 0;
}
static int identity(InspectionWork *w,const PlayerProbeIdentity *expected) {
    Snapshot now;
    return snapshot(&w->reader,&now) && !memcmp(&now,expected,sizeof now);
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
static Meta *descriptor(InspectionWork *w,uint32_t id,uint32_t group,int *owner) {
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
static int shared(InspectionWork *w,const PlayerProbeIdentity *expected) {
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
    if(!aligned(table,8)||length<=0x2C7 || length>capacity || capacity>UINT64_MAX/16 || !extent(table,capacity*16,&end))return 0;
    x=meta(w,table+0x2C7*16,16,8);if(!x)return 0;significant(x,8,4);
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

static int at_ptr(InspectionWork *w,uint64_t base,uint64_t offset,uint64_t *out) {
    uint64_t a;return add(base,offset,&a)&&mptr(w,a,out);
}
static int at_word(InspectionWork *w,uint64_t base,uint64_t offset,uint32_t *out) {
    uint64_t a;return add(base,offset,&a)&&mword(w,a,out);
}
static Meta *at_meta(InspectionWork *w,uint64_t base,uint64_t offset,uint32_t n,uint32_t alignment) {
    uint64_t a;return add(base,offset,&a)?whole(w,a,n,alignment):NULL;
}
static int acd(InspectionWork *w,uint64_t pool,uint32_t id,uint64_t *object) {
    uint32_t capacity,shift,seen;uint64_t pages,page;
    const uint32_t low=id&0xFFFFu;
    return at_word(w,pool,0x100,&capacity)&&low<capacity&&
        at_word(w,pool,0x168,&shift)&&shift<=16&&at_ptr(w,pool,0x120,&pages)&&
        at_ptr(w,pages,(uint64_t)(low>>shift)*8,&page)&&
        add(page,(uint64_t)(low&((UINT32_C(1)<<shift)-1))*0x360,object)&&
        mword(w,*object,&seen)&&seen==id;
}
static int equipped_item(InspectionWork *w,const PlayerProbeIdentity *expected,
                         unsigned slot,uint32_t accepted,uint64_t *item,uint32_t *gbid) {
    const uint32_t key=slot+1; // Native SLOT_PLAYER_HEAD=1 through NECK=13.
    uint64_t globals,pool,player,inventory,buckets,head,value,cells;uint32_t type,id;
    if(!at_ptr(w,expected->client,0x9A0,&globals)||!mptr(w,globals,&pool)||
       !acd(w,pool,expected->acd_id,&player)||!at_ptr(w,player,0xC8,&inventory)||
       !mword(w,inventory,&type)||type!=1)return 0;
    // This inventory bucket array is packed to four-byte alignment on device.
    // Only the array and its entry reads use that alignment, not the nodes.
    Meta *entry=at_meta(w,inventory,0x18,8,8);if(!entry)return 0;
    buckets=u64(entry->data);if(!aligned(buckets,4))return 0;
    entry=at_meta(w,buckets,(uint64_t)(fnv(key)&31)*8,8,4);if(!entry)return 0;
    head=u64(entry->data);if(!aligned(head,8))return 0;
    uint64_t seen[8]={0};
    for(unsigned i=0;i<8;i++) {
        for(unsigned j=0;j<i;j++)if(head==seen[j])return 0;
        seen[i]=head;Meta *m=whole(w,head,16,8);if(!m)return 0;
        if(u32(m->data+8)==key) {
            if(!at_ptr(w,head,0x10,&value)||!mptr(w,value,&cells)||!mword(w,cells,&id)||id!=accepted||
               !acd(w,pool,id,item))return 0;
            m=at_meta(w,*item,0x38,8,4);if(!m||u32(m->data)!=2||u32(m->data+4)==UINT32_MAX)return 0;
            *gbid=u32(m->data+4);return 1;
        }
        head=u64(m->data);if(!head)return 0;
    }
    return 0;
}
// The native item-name getter checks Item_Unidentified (0x19E) before choosing
// its label. 0x69E334 checks the group dirty flag, then the exact dirty-key set;
// only a matching key invokes guest recomputation. Never invoke that fallback.
// Accept zero only from a proven-clean key's cache or proven registered default.
static int identified(InspectionWork *w,uint64_t item) {
    const uint32_t key=0xFFFFF19Eu;uint64_t group,owner,buckets,head=0,seen[8]={0};
    if(!at_ptr(w,item,0x168,&group))return 0;
    Meta *m=at_meta(w,group,4,1,1);if(!m)return 0;
    const unsigned flags=m->data[0];
    if(flags&2) {
        uint64_t dirty,dirty_seen[8]={0};
        m=at_meta(w,group,0x10A8,8,8);if(!m)return 0;
        dirty=u64(m->data);if(!aligned(dirty,4))return 0;
        // 0x69D004–0x69D038 initializes this array at group+0x10B4.
        m=at_meta(w,dirty,(uint64_t)((key^(key>>12))&31u)*8,8,4);
        if(!m)return 0;head=u64(m->data);
        for(unsigned i=0;i<8&&head;i++) {
            for(unsigned j=0;j<i;j++)if(head==dirty_seen[j])return 0;
            dirty_seen[i]=head;m=whole(w,head,12,8);if(!m)return 0;
            if(u32(m->data+8)==key)return 0;
            head=u64(m->data);
        }
        if(head)return 0;
    }
    if(flags&4){if(!at_ptr(w,group,0x10,&owner))return 0;
        m=at_meta(w,owner,0x10,8,8);
    }else m=at_meta(w,group,0x28,8,8);
    if(!m)return 0;buckets=u64(m->data);if(!aligned(buckets,4))return 0;
    // The small cache is also packed: 0x69CEFC–0x69CF00 stores group+0x34.
    m=at_meta(w,buckets,(uint64_t)((key^(key>>12))&(flags&4?511u:255u))*8,8,4);
    if(!m)return 0;head=u64(m->data);
    for(unsigned i=0;i<8&&head;i++) {
        for(unsigned j=0;j<i;j++)if(head==seen[j])return 0;
        seen[i]=head;m=whole(w,head,16,8);if(!m)return 0;
        if(u32(m->data+8)==key)return u32(m->data+12)==0;
        head=u64(m->data);
    }
    if(head)return 0;
    m=at_meta(w,w->original->main_base,UINT64_C(0x191EAD8)+64*(key&0xFFF),8,8);
    return m&&u32(m->data)==(key&0xFFF)&&u32(m->data+4)==0;
}
static int item_key(InspectionWork *w,const PlayerProbeIdentity *expected,uint32_t gbid) {
    uint64_t catalog,tables,buckets,node=0,seen[8]={0},manager_ptr,manager,count_ptr,index_ptr,indices,pool,base,object,data,row,key;
    uint32_t sno=UINT32_MAX,row_index=UINT32_MAX,count,handle,group,slots,bytes;
    Meta *m;
    if(!at_ptr(w,expected->globals,0x2178,&catalog)||!mptr(w,catalog,&tables))return 0;
    m=at_meta(w,tables,2*128,28,8);if(!m)return 0;
    uint32_t mask=u32(m->data),bucket_count=u32(m->data+0x18);
    buckets=u64(m->data+0x10);
    if(!bucket_count||(bucket_count&(bucket_count-1))||bucket_count>65536||mask!=bucket_count-1||!aligned(buckets,8))return 0;
    if(!at_ptr(w,buckets,(uint64_t)(fnv(gbid)&mask)*8,&node))return 0;
    for(unsigned i=0;i<8&&node;i++) {
        for(unsigned j=0;j<i;j++)if(node==seen[j])return 0;
        seen[i]=node;m=whole(w,node,20,8);if(!m)return 0;
        if(u32(m->data+8)==gbid){sno=u32(m->data+12);row_index=u32(m->data+16);break;}
        node=u64(m->data);
    }
    if(sno>0xFFFFFF||row_index==UINT32_MAX)return 0;
    const uint64_t main=w->original->main_base;
    if(!mptr(w,main+0x114B758,&manager_ptr)||!mptr(w,manager_ptr,&manager)||
       !mptr(w,main+0x11572E8,&count_ptr)||!mword(w,count_ptr,&count)||sno>=count||count>0x1000000||
       !mptr(w,main+0x11572F0,&index_ptr)||!mptr(w,index_ptr,&indices)||
       !at_word(w,indices,(uint64_t)sno*4,&handle)||handle==UINT32_MAX||
       !at_ptr(w,manager,0x20,&pool)||!at_word(w,manager,0x50,&group)||group>255)return 0;
    m=at_meta(w,pool,0x100,8,8);if(!m)return 0;slots=u32(m->data);
    if(!slots||slots>0x3FFFE||u32(m->data+4)!=16||(handle&0x3FFFF)>=slots||!at_ptr(w,pool,0x120,&base))return 0;
    m=at_meta(w,base,(uint64_t)(handle&0x3FFFF)*16,16,8);if(!m)return 0;
    object=u64(m->data+8);
    if(u32(m->data)!=handle||m->data[7]!=group||!aligned(object,4))return 0;
    m=meta(w,object,16,4);if(!m)return 0;
    significant(m,0,4);significant(m,8,8); // Compare the same identity/loaded fields as the resident StringList route.
    if(u32(m->data)!=sno||!(u32(m->data+8)&0x40)||(u32(m->data+8)&2)||u32(m->data+12)!=2)return 0;
    // 0x6CA800's group-2 pointer offset is 0x30; 0x6CA180's row size is 800.
    // 0x6CA1E0 reads bytes at +0x2C. Catalog builder 0x6CCF60 reads row GBID +0x10.
    if(!at_word(w,object,0x2C,&bytes)||!bytes||bytes%800||row_index>=bytes/800||
       !at_ptr(w,object,0x30,&data)||!add(data,(uint64_t)row_index*800,&row))return 0;
    m=at_meta(w,row,8,12,8);if(!m||u32(m->data+8)!=gbid)return 0;
    key=u64(m->data);if(!key)return 0;
    Payload *p=&w->payload[0];p->internal_address=key;p->internal_size=128;
    uint32_t length;
    return copy(w,key,p->internal,128,1)&&printable(p->internal,128,&length);
}
static int localized_item(InspectionWork *w) {
    Catalog *c=&w->catalog;Payload *p=&w->payload[0];uint32_t length;
    if(!printable(p->internal,p->internal_size,&length))return 0;
    uint32_t hash=0;for(unsigned i=0;i<length;i++)hash=33*hash+p->internal[i];
    Meta *m=whole(w,c->buckets+(uint64_t)(fnv(hash)&c->mask)*8,8,4);if(!m)return 0;
    uint64_t n=u64(m->data),seen[8]={0};unsigned count=0;
    while(n) {
        if(count>=8||count>=c->node_count||!node_member(c,n))return 0;
        for(unsigned i=0;i<count;i++)if(seen[i]==n)return 0;
        seen[count++]=n;m=meta(w,n,24,8);if(!m)return 0;
        significant(m,0,12);significant(m,16,8);
        if(u32(m->data+8)==hash) {
            uint64_t e=u64(m->data+16);
            if(e<c->entries||e>=c->entries_end||c->entries_end-e<24||(e-c->entries)%24)return 0;
            m=meta(w,e,24,4);if(!m)return 0;significant(m,0,20);
            p->text_address=u64(m->data);p->text_size=u32(m->data+12);
            if(u32(m->data+16)!=hash||!p->text_size||p->text_size>512||
               !copy(w,p->text_address,p->text,p->text_size,1)||!printable(p->text,p->text_size,&length))return 0;
            p->complete=1;return 1;
        }
        n=u64(m->data);
    }
    return 0;
}
static void close_one(InspectionWork *w,Meta *m) {
    unsigned char current[128]={0};int ok=copy(w,m->address,current,m->size,m->alignment);
    if(ok)for(unsigned i=0;i<m->size;i++)if(m->significant[i]&&current[i]!=m->data[i]){ok=0;break;}
    if(!ok){if(m->field==SHARED)w->shared_ok=0;else w->field_ok[0]=0;}
}
void equipment_inspect(const EdenDsmodHostApi *host,const PlayerProbeIdentity *expected,
                       unsigned slot,const EquipmentSlot *accepted,EquipmentInspectResult *out) {
    *out=(EquipmentInspectResult){.reason="UNAVAILABLE: equipment base name"};
    if(!host||!expected||!accepted||slot>=EQUIPMENT_SLOT_COUNT||!accepted->available||!accepted->occupied||
       accepted->acd_id==UINT32_MAX||!host->userdata||!host->read_memory||!host->is_mapped||
       !host->main_base||host->main_base%8||host->main_size<0x1A63CF4||host->main_base>UINT64_MAX-host->main_size)return;
    InspectionWork w={.original=host,.wrapped=*host,.field=SHARED,
        .limit_reads=EQUIPMENT_INSPECT_MAX_READS-CLOSING_READS,.limit_bytes=EQUIPMENT_INSPECT_MAX_BYTES-CLOSING_BYTES};
    w.wrapped.userdata=&w;w.wrapped.is_mapped=budget_map;w.wrapped.read_memory=copy_memory;
    w.reader=(Reader){&w.wrapped,&w.scratch,UINT32_MAX,UINT32_MAX,NULL};
    const int initial=identity(&w,expected);uint64_t item=0;uint32_t gbid=0;
    if(initial) {
        w.shared_ok=equipped_item(&w,expected,slot,accepted->acd_id,&item,&gbid);
        if(w.shared_ok) {
            w.field=0;
            w.field_ok[0]=identified(&w,item)&&shared(&w,expected)&&item_key(&w,expected,gbid)&&localized_item(&w);
            if(w.field_ok[0]) {
                Payload *p=&w.payload[0];unsigned char internal[128]={0},text[512]={0};
                int a=copy(&w,p->internal_address,internal,p->internal_size,1);
                int b=copy(&w,p->text_address,text,p->text_size,1);
                if(!a||!b||memcmp(internal,p->internal,p->internal_size)||memcmp(text,p->text,p->text_size))w.field_ok[0]=0;
            }
        }
    }
    w.limit_reads=EQUIPMENT_INSPECT_MAX_READS;w.limit_bytes=EQUIPMENT_INSPECT_MAX_BYTES;
    for(unsigned i=w.count;i>0;i--)if(!w.journal[i-1].late)close_one(&w,&w.journal[i-1]);
    for(int late=1;late<=2;late++)for(unsigned i=w.count;i>0;i--)if(w.journal[i-1].late==late)close_one(&w,&w.journal[i-1]);
    const int final=identity(&w,expected);
    out->shared_identity_valid=initial&&final;out->item_valid=out->shared_identity_valid&&w.shared_ok;
    if(out->item_valid&&w.field_ok[0]&&w.payload[0].complete) {
        out->base_name_available=1;memcpy(out->base_name,w.payload[0].text,w.payload[0].text_size);
    }
    out->reads=w.attempts;out->bytes=w.bytes;
    out->reason=out->base_name_available?"UNVERIFIED: localized base item name":out->item_valid?"UNAVAILABLE: item base name":"UNAVAILABLE: equipped item identity";
}
