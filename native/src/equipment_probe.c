// SPDX-License-Identifier: GPL-3.0-or-later
#include "equipment_probe.h"
#include "probe_internal.h"

// Exact-build evidence: 0x48A120 reads ACD+0xC8, inventory+0x18, a 32-bucket
// FNV1a table keyed by body slot, node+0x10, then the first cell's ACD handle.
// 0x4888D0 validates that handle against the ACD pool before using the item.
// SLOT_PLAYER_* registrations identify keys 1–13. Decode the Rela entries as
// (offset, info, addend): HEAD's string relocates to 0x11243E0, whose +8 value
// is 1; the following registrations continue through NECK=13.
// This reader does not call those game functions or enumerate other inventories.
typedef struct EquipmentCommon {
    Snapshot player;
    uint64_t globals,pool,pages,page,acd,inventory,buckets;
    uint32_t capacity,shift,acd_id,inventory_type;
} EquipmentCommon;

typedef struct EquipmentNode {
    uint64_t address,next;
    uint32_t key,padding;
} EquipmentNode;

typedef struct EquipmentObservation {
    uint64_t head,value,cells,item_page,item;
    EquipmentNode nodes[8];
    uint32_t count,id,item_id;
} EquipmentObservation;

static int equipment_common(Reader *r, EquipmentCommon *c,
                            const PlayerProbeIdentity *expected) {
    memset(c,0,sizeof *c);
    if(!snapshot(r,&c->player)||memcmp(&c->player,expected,sizeof *expected))return 0;
    const uint32_t low=expected->acd_id&0xFFFFu;
    return pointer(r,expected->client,0x9A0,&c->globals)&&
        pointer(r,c->globals,0,&c->pool)&&
        read_offset(r,c->pool,0x100,&c->capacity,4,4)&&low<c->capacity&&
        read_offset(r,c->pool,0x168,&c->shift,4,4)&&c->shift<=16&&
        pointer(r,c->pool,0x120,&c->pages)&&
        pointer(r,c->pages,(uint64_t)(low>>c->shift)*8,&c->page)&&
        add(c->page,(uint64_t)(low&((UINT32_C(1)<<c->shift)-1))*0x360,&c->acd)&&
        read_at(r,c->acd,&c->acd_id,4,4)&&c->acd_id==expected->acd_id&&
        pointer(r,c->acd,0xC8,&c->inventory)&&
        read_at(r,c->inventory,&c->inventory_type,4,4)&&c->inventory_type==1&&
        read_offset(r,c->inventory,0x18,&c->buckets,8,8)&&c->buckets&&c->buckets%4==0;
}

static uint32_t slot_bucket(uint32_t key) {
    uint32_t hash=UINT32_C(0x811C9DC5);
    for(unsigned i=0;i<4;i++)hash=(hash^((key>>(8*i))&255u))*UINT32_C(0x01000193);
    return hash&31u;
}

static int equipment_slot(Reader *r,const EquipmentCommon *c,unsigned index,
                          EquipmentObservation *o,const char **reason) {
    memset(o,0,sizeof *o);
    const uint32_t key=index+1;
    *reason="UNAVAILABLE: equipment slot read";
    // The exact-build inventory embeds this pointer array at a four-aligned
    // address (confirmed on device); the pointed-to nodes remain eight-aligned.
    if(!read_offset(r,c->buckets,(uint64_t)slot_bucket(key)*8,&o->head,8,4))return 0;
    uint64_t address=o->head;
    for(unsigned i=0;i<8;i++) {
        if(!address){*reason="UNAVAILABLE: equipment slot missing";return 0;}
        for(unsigned j=0;j<i;j++)if(address==o->nodes[j].address){
            *reason="UNAVAILABLE: equipment slot cycle";return 0;
        }
        EquipmentNode *n=&o->nodes[i];n->address=address;
        if(!read_at(r,address,&n->next,16,8))return 0;
        o->count=i+1;
        if(n->key!=key){address=n->next;continue;}
        if(!pointer(r,address,0x10,&o->value)||!pointer(r,o->value,0,&o->cells)||
           !read_at(r,o->cells,&o->id,4,4))return 0;
        if(o->id==UINT32_MAX)return 1; // An observed empty cell, not a missing key.
        *reason="UNAVAILABLE: equipment item identity";
        const uint32_t low=o->id&0xFFFFu;
        return low<c->capacity&&
            pointer(r,c->pages,(uint64_t)(low>>c->shift)*8,&o->item_page)&&
            add(o->item_page,(uint64_t)(low&((UINT32_C(1)<<c->shift)-1))*0x360,&o->item)&&
            read_at(r,o->item,&o->item_id,4,4)&&o->item_id==o->id;
    }
    *reason="UNAVAILABLE: equipment slot node limit";
    return 0;
}

void equipment_probe(const EdenDsmodHostApi *host,
                     const PlayerProbeIdentity *expected,
                     EquipmentProbeResult *result) {
    *result=(EquipmentProbeResult){.reason="UNAVAILABLE: equipment requires player identity"};
    for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)result->slots[i].reason=result->reason;
    if(!host||!expected||!host->userdata||!host->read_memory||!host->is_mapped||
       !host->main_base||host->main_base%8||host->main_size<ROOT_OFFSET+8||
       host->main_base>UINT64_MAX-host->main_size)return;
    PlayerProbeResult work={0};
    Reader reader={host,&work,EQUIPMENT_MAX_READS,EQUIPMENT_MAX_BYTES,NULL};
    EquipmentCommon before={0},after={0};
    EquipmentObservation first[EQUIPMENT_SLOT_COUNT]={0},second[EQUIPMENT_SLOT_COUNT]={0};
    int first_ok[EQUIPMENT_SLOT_COUNT]={0},second_ok[EQUIPMENT_SLOT_COUNT]={0};
    const int common_before=equipment_common(&reader,&before,expected);
    if(common_before) {
        for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)
            first_ok[i]=equipment_slot(&reader,&before,i,&first[i],&result->slots[i].reason);
        for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)
            second_ok[i]=equipment_slot(&reader,&before,i,&second[i],&result->slots[i].reason);
    }
    // A field error never skips the final shared player/inventory ownership check.
    const int common_after=equipment_common(&reader,&after,expected);
    result->reads=work.reads;result->bytes=work.bytes;
    if(!common_before||!common_after||memcmp(&before,&after,sizeof before)) {
        result->reason="UNAVAILABLE: equipment owner changed or unreadable";
        for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++)result->slots[i].reason=result->reason;
        return;
    }
    result->shared_identity_valid=1;result->reason="Equipment slot observations accepted";
    for(unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++) {
        EquipmentSlot *slot=&result->slots[i];
        if(!first_ok[i]||!second_ok[i])continue;
        if(memcmp(&first[i],&second[i],sizeof first[i])){
            slot->reason="UNAVAILABLE: equipment changed during read";continue;
        }
        slot->available=1;slot->occupied=first[i].id!=UINT32_MAX;
        slot->acd_id=slot->occupied?first[i].id:0;
        slot->reason=slot->occupied?"Equipped":"Empty";
    }
}
