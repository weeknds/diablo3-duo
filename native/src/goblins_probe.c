// SPDX-License-Identifier: GPL-3.0-or-later
#include "goblins_probe.h"
#include "probe_internal.h"
#include <math.h>
#include <stdlib.h>

/* Exact-build routes: 0x3EE340 inserts native minimap actors, 0x3E8C1C
 * renders them, and 0x4769E0 resolves their generic handles. 0x2C2CC8 uses
 * 0x518FF0's monster-family value 11 to select the TreasureGoblin icon.
 * The resource managers are constructed at 0x6B6400 and 0x6E88D0.
 * No numeric actor SNO list and no native function invocation is used here. */
#define META_CAP 4096u
#define BLOCK_CAP 16u
#define CLASS_CAP 128u
typedef struct Meta {uint64_t a,mask;uint32_t n,alignment;unsigned char data[64];} Meta;
typedef struct Block {uint64_t base,end;uint32_t used;} Block;
typedef struct Pool {uint64_t pages;uint32_t capacity,shift;} Pool;
typedef struct ResourcePool {uint64_t records;uint32_t capacity,group;} ResourcePool;
typedef struct Classification {uint32_t sno;int goblin;} Classification;
typedef struct Work {
    Reader reader;PlayerProbeResult counters;Meta journal[META_CAP];unsigned count;
    Block blocks[BLOCK_CAP];unsigned block_count;
    Pool acd_pool;uint64_t ids,sno_indices;uint32_t sno_count;
    ResourcePool actors,monsters;
    Classification classes[CLASS_CAP];unsigned class_count;
} Work;
static uint32_t u32(const unsigned char *p){uint32_t x;memcpy(&x,p,4);return x;}
static uint64_t u64(const unsigned char *p){uint64_t x;memcpy(&x,p,8);return x;}
static int aligned(uint64_t a,unsigned alignment){return a&&a%alignment==0;}
static uint64_t bytes_mask(unsigned first,unsigned n){return (n==64?UINT64_MAX:((UINT64_C(1)<<n)-1))<<first;}
static Meta *watch(Work*w,uint64_t base,uint64_t offset,uint32_t n,uint32_t alignment,uint64_t mask){
    uint64_t a;
    if(n>64||w->count>=META_CAP||w->counters.reads>=GOBLINS_MAX_READS/2-64||
       w->counters.bytes>=GOBLINS_MAX_BYTES/2-2048||!add(base,offset,&a))return NULL;
    Meta *m=&w->journal[w->count];*m=(Meta){.a=a,.mask=mask,.n=n,.alignment=alignment};
    if(!read_at(&w->reader,a,m->data,n,alignment))return NULL;
    w->count++;return m;
}
static Meta *whole(Work*w,uint64_t a,uint64_t offset,uint32_t n,uint32_t alignment){return watch(w,a,offset,n,alignment,bytes_mask(0,n));}
static int ptr(Work*w,uint64_t a,uint64_t offset,uint64_t*p){Meta*m=whole(w,a,offset,8,8);if(!m)return 0;*p=u64(m->data);return aligned(*p,8);}
static int word(Work*w,uint64_t a,uint64_t offset,uint32_t*p){Meta*m=whole(w,a,offset,4,4);if(!m)return 0;*p=u32(m->data);return 1;}
static int close_journal(Work*w){
    unsigned char now[64];
    for(unsigned i=w->count;i;i--){Meta*m=&w->journal[i-1];if(!read_at(&w->reader,m->a,now,m->n,m->alignment))return 0;
        for(unsigned b=0;b<m->n;b++)if((m->mask&(UINT64_C(1)<<b))&&m->data[b]!=now[b])return 0;}
    return 1;
}
static int pool_read(Work*w,uint64_t p,Pool*out){return word(w,p,0x100,&out->capacity)&&out->capacity>0&&out->capacity<=65536&&
    word(w,p,0x168,&out->shift)&&out->shift<=16&&ptr(w,p,0x120,&out->pages);}
static int pool_object(Work*w,const Pool*p,uint32_t id,uint32_t stride,uint64_t*object){
    const uint32_t low=id&0xFFFFu;uint64_t page;
    return id!=UINT32_MAX&&low<p->capacity&&ptr(w,p->pages,(uint64_t)(low>>p->shift)*8,&page)&&
        add(page,(uint64_t)(low&((UINT32_C(1)<<p->shift)-1))*stride,object);
}
static int generic_valid(uint32_t id){return id!=UINT32_MAX&&((id>>2)&0x3FFFu)<=0xEE6u;}
static int shared_world(Work*w,const PlayerProbeIdentity*e,uint32_t world){
    Snapshot s;uint64_t g,p,acd,visible;uint32_t id,current,generic,mapped;
    if(!snapshot(&w->reader,&s)||memcmp(&s,e,sizeof s)||!ptr(w,e->client,0x9A0,&g)||!ptr(w,g,0,&p)||
       !ptr(w,g,8,&w->ids)||!pool_read(w,p,&w->acd_pool)||!pool_object(w,&w->acd_pool,e->acd_id,0x360,&acd)||
       !word(w,acd,0,&id)||id!=e->acd_id||!word(w,acd,0xA0,&current)||current!=world||
       !ptr(w,e->client,0x800,&visible)||!word(w,visible,0x38,&current)||current!=world)return 0;
    /* Calibrate the client generic-handle mapping against the selected player.
     * This rejects other native contexts rather than guessing a TLS mode. */
    return word(w,acd,0x10,&generic)&&generic_valid(generic)&&
        word(w,w->ids,(uint64_t)(generic&0xFFFFu)*4,&mapped)&&mapped==e->acd_id;
}
static int resource_pool(Work*w,uint64_t got,uint32_t group,uint32_t size,uint64_t vtable,ResourcePool*out){
    uint64_t owner,manager,pool,vt;uint32_t actual,stride;
    return ptr(w,w->reader.host->main_base,got,&owner)&&ptr(w,owner,0,&manager)&&
        ptr(w,manager,0,&vt)&&vt==w->reader.host->main_base+vtable&&
        word(w,manager,0x50,&out->group)&&out->group==group&&word(w,manager,0x7C,&actual)&&actual==size&&
        ptr(w,manager,0x20,&pool)&&word(w,pool,0x100,&out->capacity)&&out->capacity>0&&out->capacity<=0x3FFFE&&
        word(w,pool,0x104,&stride)&&stride==16&&ptr(w,pool,0x120,&out->records);
}
static int resource_roots(Work*w){
    uint64_t owner,table;
    return ptr(w,w->reader.host->main_base,0x11572E8,&owner)&&word(w,owner,0,&w->sno_count)&&
        w->sno_count>0&&w->sno_count<=0x1000000&&ptr(w,w->reader.host->main_base,0x11572F0,&table)&&
        ptr(w,table,0,&w->sno_indices)&&
        resource_pool(w,0x114B778,1,0x1C0,0x10C5550,&w->actors)&&
        resource_pool(w,0x1150CA8,25,0x558,0x10C6DB8,&w->monsters);
}
static int resource(Work*w,const ResourcePool*p,uint32_t sno,uint64_t*object){
    uint32_t handle;Meta*m;
    if(sno>=w->sno_count||!word(w,w->sno_indices,(uint64_t)sno*4,&handle)||handle==UINT32_MAX||
       (handle&0x3FFFFu)>=p->capacity)return 0;
    m=watch(w,p->records,(uint64_t)(handle&0x3FFFFu)*16,16,8,bytes_mask(0,4)|bytes_mask(7,9));
    if(!m||u32(m->data)!=handle||m->data[7]!=p->group||!aligned(*object=u64(m->data+8),4))return 0;
    m=watch(w,*object,0,12,4,bytes_mask(0,4)|bytes_mask(8,4));
    return m&&u32(m->data)==sno&&(u32(m->data+8)&0x40u)&&!(u32(m->data+8)&2u);
}
static int classify(Work*w,uint32_t sno,int*goblin){
    for(unsigned i=0;i<w->class_count;i++)if(w->classes[i].sno==sno){*goblin=w->classes[i].goblin;return 1;}
    uint64_t actor,monster;uint32_t type,monster_sno,family;
    if(w->class_count>=CLASS_CAP||!resource(w,&w->actors,sno,&actor)||!word(w,actor,0x10,&type)||type!=1||
       !word(w,actor,0x74,&monster_sno)||!resource(w,&w->monsters,monster_sno,&monster)||!word(w,monster,0x1C,&family))return 0;
    *goblin=family==11;w->classes[w->class_count++]=(Classification){sno,*goblin};return 1;
}
static int allocator_blocks(Work*w,uint64_t allocator){
    uint32_t stride,count;uint64_t block;
    if(!word(w,allocator,0,&stride)||stride!=24||!word(w,allocator,8,&count)||count>BLOCK_CAP)return 0;
    Meta*m=whole(w,allocator,0x10,8,8);if(!m)return 0;block=u64(m->data);
    while(block){
        if(w->block_count>=count||!aligned(block,16))return 0;
        for(unsigned i=0;i<w->block_count;i++)if(block+0x50==w->blocks[i].base)return 0;
        m=watch(w,block,0,64,16,bytes_mask(0,16)|bytes_mask(0x18,12)|bytes_mask(0x2C,4)|bytes_mask(0x38,4));
        if(!m)return 0;
        uint32_t capacity=u32(m->data+0x18),used=u32(m->data+0x20),free_count=u32(m->data+0x2C);uint64_t data=u64(m->data+8),end;
        if(!capacity||capacity>65536||u32(m->data+0x1C)!=24||used>capacity||free_count>capacity||
           u32(m->data+0x38)!=0x600DF00D||block>UINT64_MAX-0x50||data!=block+0x50||
           !add(data,(uint64_t)capacity*24,&end))return 0;
        for(unsigned i=0;i<w->block_count;i++)if(data<w->blocks[i].end&&w->blocks[i].base<end)return 0;
        w->blocks[w->block_count++]=(Block){data,end,used};block=u64(m->data);
    }
    return w->block_count==count;
}
static int node_owned(const Work*w,uint64_t node){
    if(!aligned(node,8))return 0;unsigned owners=0;
    for(unsigned i=0;i<w->block_count;i++){const Block*b=&w->blocks[i];if(node>=b->base&&node<b->end&&b->end-node>=24&&
       (node-b->base)%24==0&&(node-b->base)/24<b->used)owners++;}
    return owners==1;
}
static int coordinate(float f){return isfinite(f)&&fabsf(f)<=1000000.0f;}
static int explored(const MapProbeResult*map,float x,float y){
    if(!coordinate(x)||!coordinate(y))return 0;
    for(unsigned i=0;i<map->tile_count;i++){
        const MapExplorationTile*t=&map->tiles[i];
        if(!coordinate(t->min_x)||!coordinate(t->max_x)||!coordinate(t->min_y)||!coordinate(t->max_y)||
           t->min_x>=t->max_x||t->min_y>=t->max_y||!t->columns||!t->rows||t->fully_revealed>1||
           (uint64_t)t->columns*t->rows>MAP_TILE_BYTES*4u)continue;
        if(x<t->min_x||x>=t->max_x||y<t->min_y||y>=t->max_y)continue;
        unsigned col=(unsigned)((double)(x-t->min_x)/(t->max_x-t->min_x)*t->columns);
        unsigned row=(unsigned)((double)(y-t->min_y)/(t->max_y-t->min_y)*t->rows);
        if(map_cell_visibility(t,col,row))return 1;
    }
    return 0;
}
static int visible_key(Work*w,uint64_t acd,int32_t player,int*visible){
    uint64_t group,owner,buckets,head,seen[8]={0};const uint32_t key=0x1EBu|((uint32_t)player<<12);Meta*m;
    if(!ptr(w,acd,0x168,&group))return 0;
    m=whole(w,group,4,1,1);if(!m||(m->data[0]&2))return 0;
    const unsigned flags=m->data[0];
    if(flags&4){if(!ptr(w,group,0x10,&owner))return 0;m=whole(w,owner,0x10,8,8);}
    else m=whole(w,group,0x28,8,8);
    if(!m||!aligned(buckets=u64(m->data),4))return 0;
    m=whole(w,buckets,(uint64_t)((key^(key>>12))&(flags&4?511u:255u))*8,8,4);if(!m)return 0;head=u64(m->data);
    for(unsigned i=0;i<8&&head;i++){
        for(unsigned j=0;j<i;j++)if(seen[j]==head)return 0;
        seen[i]=head;m=whole(w,head,0,16,8);if(!m)return 0;
        if(u32(m->data+8)==key){const uint32_t v=u32(m->data+12);*visible=v>0&&v<=INT32_MAX;return 1;}
        head=u64(m->data);
    }
    /* Missing cache visibility is unknown, never an invented visible default. */
    return 0;
}
static int actor(Work*w,const PlayerProbeIdentity*e,const MapProbeResult*map,uint32_t generic,MarkersProbeResult*out){
    uint32_t acd_id,world,state,type,gizmo;uint64_t acd,client;Meta*m;
    if(!generic_valid(generic)||!word(w,w->ids,(uint64_t)(generic&0xFFFFu)*4,&acd_id)||
       !pool_object(w,&w->acd_pool,acd_id,0x360,&acd))return 0;
    m=watch(w,acd,0,28,4,bytes_mask(0,4)|bytes_mask(0x10,12));
    if(!m||u32(m->data)!=acd_id||u32(m->data+0x10)!=generic)return 0;
    const uint32_t client_id=u32(m->data+0x14),sno=u32(m->data+0x18);
    if(!word(w,acd,0xA0,&world)||!word(w,acd,0x48,&state))return 0;
    if(world!=map->world_id||state)return 1;
    if(!word(w,acd,0x14C,&type))return 0;if(type!=1)return 1;
    int goblin;if(!classify(w,sno,&goblin))return 0;if(!goblin)return 1;
    if(!word(w,acd,0x148,&gizmo)||gizmo!=UINT32_MAX)return 0;
    m=whole(w,acd,0x160,2,2);if(!m)return 0;const uint32_t flags=m->data[0]|((uint32_t)m->data[1]<<8);
    if((flags&0x80u)||((flags>>3)&7u)!=7u)return 1;
    Pool client_pool={e->pages,e->capacity,e->shift};
    if(!pool_object(w,&client_pool,client_id,0x410,&client))return 0;
    m=watch(w,client,0,16,4,bytes_mask(0,4)|bytes_mask(8,8));
    if(!m||u32(m->data)!=client_id||u32(m->data+8)!=acd_id||u32(m->data+12)!=sno)return 0;
    m=whole(w,client,0x1CC,1,1);if(!m)return 0;if(m->data[0]&4)return 1;
    int visible;if(!visible_key(w,acd,e->player_index,&visible))return 0;if(!visible)return 1;
    float xy[2],presentation[2];
    if(!read_offset(&w->reader,acd,0x60,xy,sizeof xy,4)||!read_offset(&w->reader,client,0x30,presentation,sizeof presentation,4)||
       !coordinate(xy[0])||!coordinate(xy[1])||!coordinate(presentation[0])||!coordinate(presentation[1]))return 0;
    if(!explored(map,xy[0],xy[1])||!explored(map,presentation[0],presentation[1]))return 1;
    if(out->count>=MARKERS_MAX_ITEMS)return 0;
    out->items[out->count++]=(Marker){generic,MARKER_GOBLIN,presentation[0],presentation[1]};return 1;
}
static int collect(Work*w,const PlayerProbeIdentity*e,const MapProbeResult*map,MarkersProbeResult*out){
    uint64_t manager,list,head,tail,allocator,previous=0,seen[GOBLINS_MAX_NODES];Meta*m;
    if(!shared_world(w,e,map->world_id)||!ptr(w,w->reader.host->main_base,0x1878FE8,&manager)||!ptr(w,manager,0,&list))return 0;
    m=watch(w,list,0,40,8,bytes_mask(0,20)|bytes_mask(24,12));if(!m)return 0;
    head=u64(m->data);tail=u64(m->data+8);uint32_t count=u32(m->data+16);allocator=u64(m->data+24);
    if(count>GOBLINS_MAX_NODES||u32(m->data+32)!=1||(!count&&(head||tail))||(count&&(!head||!tail)))return 0;
    if(!count)return 1;
    if(!aligned(allocator,8)||!allocator_blocks(w,allocator)||!resource_roots(w))return 0;
    for(unsigned i=0;i<count;i++){
        if(!node_owned(w,head))return 0;
        for(unsigned j=0;j<i;j++)if(seen[j]==head)return 0;
        seen[i]=head;m=watch(w,head,0,24,8,bytes_mask(0,4)|bytes_mask(8,16));if(!m||u64(m->data+8)!=previous)return 0;
        const uint64_t next=u64(m->data+16);const uint32_t id=u32(m->data);
        if(!actor(w,e,map,id,out))return 0;
        previous=head;head=next;
    }
    return !head&&previous==tail;
}
void goblins_probe(const EdenDsmodHostApi*host,const PlayerProbeIdentity*expected,const MapProbeResult*map,MarkersProbeResult*result){
    if(!result)return;
    *result=(MarkersProbeResult){.reason="UNAVAILABLE: goblin map context"};
    if(!host||!expected||!map||!host->userdata||!host->read_memory||!host->is_mapped||!host->main_base||host->main_base%8||
       host->main_size<0x1878FF0||host->main_base>UINT64_MAX-host->main_size||!map->available||!map->shared_identity_valid||
       !map->exploration_available||map->world_id==UINT32_MAX||map->tile_count>MAP_MAX_TILES)return;
    Work*w=calloc(1,sizeof *w);if(!w){result->reason="UNAVAILABLE: goblin reader allocation";return;}
    w->reader=(Reader){host,&w->counters,GOBLINS_MAX_READS,GOBLINS_MAX_BYTES,NULL};MarkersProbeResult batch={0};Snapshot after;
    const int ok=collect(w,expected,map,&batch)&&close_journal(w)&&snapshot(&w->reader,&after)&&!memcmp(&after,expected,sizeof after);
    result->reads=w->counters.reads;result->bytes=w->counters.bytes;
    if(ok){batch.available=batch.shared_identity_valid=1;batch.world_id=map->world_id;batch.reads=result->reads;batch.bytes=result->bytes;
        batch.reason="UNVERIFIED RESEARCH: visible explored goblin actors";*result=batch;
    }else result->reason="UNAVAILABLE: goblin ownership, visibility, resource or bounded read";
    free(w);
}
