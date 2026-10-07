// SPDX-License-Identifier: GPL-3.0-or-later
// Every name/address/value in this fixture is synthetic, not a game observation.
#define main names_fixture_suite
#include "test_names.c"
#undef main
#include "equipment_inspect.h"
#define X (HEAP+0x60000)
#define XI_GLOBALS (X+0x100)
#define XI_POOL (X+0x1000)
#define XI_PAGES (X+0x2000)
#define XI_PAGE (X+0x3000)
#define XI_PLAYER (XI_PAGE+0x360)
#define XI_ITEM (XI_PAGE+0x6C0)
#define XI_INVENTORY (X+0x4000)
#define XI_BUCKETS (X+0x5004)
#define XI_NODE (X+0x6000)
#define XI_VALUE (X+0x7000)
#define XI_CELL (X+0x8000)
#define XI_CATALOG (X+0x9000)
#define XI_TABLE (X+0xA000)
#define XI_GBBUCKETS (X+0xB000)
#define XI_GBNODE (X+0xC000)
#define XI_MANAGER_PTR (X+0xD000)
#define XI_MANAGER (X+0xE000)
#define XI_INDEX_COUNT (X+0xF000)
#define XI_INDEX_PTR (X+0xF008)
#define XI_INDICES (X+0x10000)
#define XI_RESPOOL (X+0x11000)
#define XI_RESBASE (X+0x12000)
#define XI_OBJECT (X+0x13000)
#define XI_ROW (X+0x14000)
#define XI_ATTR (X+0x15000)
#define XI_ATTR_BUCKETS (X+0x16004)
#define XI_ATTR_NODE (X+0x17000)
#define XI_DIRTY_BUCKETS (X+0x18004)
#define XI_DIRTY_NODE (X+0x18200)
#define XI_OWNER_ID 0x12340001u
#define XI_ITEM_ID 0x56780002u
#define XI_GBID 0x12345678u
#define XI_RESOURCE_ID 11u
#define XI_RESOURCE_HANDLE 0x12340000u
#define XI_DEFAULT (MAIN+0x191EAD8+64*0x19Eu)

static EdenDsmodHostApi inspection_fixture(NF *f,PlayerProbeIdentity *p,EquipmentSlot *slot) {
    StoredSkillPair pairs[6];EdenDsmodHostApi h=fixture(f,p,pairs);
    f->max_reads=EQUIPMENT_INSPECT_MAX_READS;f->max_bytes=EQUIPMENT_INSPECT_MAX_BYTES;
    // Expand the first globals span and add private synthetic item structures.
    free(f->r[0].p);f->r[0]=(Region){MAIN+0x114A840,0x10000,calloc(1,0x10000)};
    f->r[3]=(Region){X,0x19000,calloc(1,0x19000)};
    f->r[4]=(Region){XI_DEFAULT,8,calloc(1,8)};
    CHECK(f->r[0].p&&f->r[3].p);put(f,MAIN+0x114A840,G,8);
    p->acd_id=XI_OWNER_ID;put(f,PLAYER+4,XI_OWNER_ID,4);
    put(f,C+0x9A0,XI_GLOBALS,8);put(f,XI_GLOBALS,XI_POOL,8);
    put(f,XI_POOL+0x100,4,4);put(f,XI_POOL+0x168,2,4);put(f,XI_POOL+0x120,XI_PAGES,8);put(f,XI_PAGES,XI_PAGE,8);
    put(f,XI_PLAYER,XI_OWNER_ID,4);put(f,XI_PLAYER+0xC8,XI_INVENTORY,8);
    put(f,XI_INVENTORY,1,4);put(f,XI_INVENTORY+0x18,XI_BUCKETS,8);
    // Head is native slot key 1, whose FNV1a bucket is 4.
    put(f,XI_BUCKETS+4*8,XI_NODE,8);put(f,XI_NODE+8,1,4);put(f,XI_NODE+0x10,XI_VALUE,8);
    put(f,XI_VALUE,XI_CELL,8);put(f,XI_CELL,XI_ITEM_ID,4);put(f,XI_ITEM,XI_ITEM_ID,4);
    put(f,XI_ITEM+0x38,2,4);put(f,XI_ITEM+0x3C,XI_GBID,4);
    // Identified flag is an explicitly observed integer zero in a clean cache.
    put(f,XI_ITEM+0x168,XI_ATTR,8);put(f,XI_ATTR+0x28,XI_ATTR_BUCKETS,8);
    const uint32_t unidentified=0xFFFFF19Eu;
    put(f,XI_ATTR_BUCKETS+8*((unidentified^(unidentified>>12))&255),XI_ATTR_NODE,8);
    put(f,XI_ATTR_NODE+8,unidentified,4);
    put(f,XI_DEFAULT,0x19E,4);
    *slot=(EquipmentSlot){.available=1,.occupied=1,.acd_id=XI_ITEM_ID};
    // Items StringList, using the existing independently built localization fixture.
    put(f,VT+0x2C7*16+8,10,4);
    str(f,ARENA(0),"SyntheticPower0_name");str(f,TEXT(0),"Synthetic bronze helm");
    put(f,G+0x2178,XI_CATALOG,8);put(f,XI_CATALOG,XI_TABLE,8);
    owner(f,XI_TABLE+2*128,XI_GBBUCKETS,24);put(f,XI_GBBUCKETS+8*bucket(XI_GBID),XI_GBNODE,8);
    put(f,XI_GBNODE+8,XI_GBID,4);put(f,XI_GBNODE+0xC,XI_RESOURCE_ID,4);
    put(f,MAIN+0x114B758,XI_MANAGER_PTR,8);put(f,XI_MANAGER_PTR,XI_MANAGER,8);
    put(f,MAIN+0x11572E8,XI_INDEX_COUNT,8);put(f,XI_INDEX_COUNT,100,4);
    put(f,MAIN+0x11572F0,XI_INDEX_PTR,8);put(f,XI_INDEX_PTR,XI_INDICES,8);
    put(f,XI_INDICES+XI_RESOURCE_ID*4,XI_RESOURCE_HANDLE,4);
    put(f,XI_MANAGER+0x20,XI_RESPOOL,8);put(f,XI_MANAGER+0x50,20,4);
    put(f,XI_RESPOOL+0x100,1,4);put(f,XI_RESPOOL+0x104,16,4);put(f,XI_RESPOOL+0x120,XI_RESBASE,8);
    put(f,XI_RESBASE,XI_RESOURCE_HANDLE,4);put(f,XI_RESBASE+7,20,1);put(f,XI_RESBASE+8,XI_OBJECT,8);
    put(f,XI_OBJECT,XI_RESOURCE_ID,4);put(f,XI_OBJECT+8,0x40,4);put(f,XI_OBJECT+0xC,2,4);
    put(f,XI_OBJECT+0x2C,800,4);put(f,XI_OBJECT+0x30,XI_ROW,8);
    put(f,XI_ROW+8,ARENA(0),8);put(f,XI_ROW+0x10,XI_GBID,4);
    return h;
}

static void valid_inspection(void) {
    NF f;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
    EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);
    equipment_inspect(&h,&p,0,&slot,&r);
    CHECK(r.shared_identity_valid&&r.item_valid&&r.base_name_available);
    CHECK(!strcmp(r.base_name,"Synthetic bronze helm"));
    CHECK(r.reads<=EQUIPMENT_INSPECT_MAX_READS&&r.bytes<=EQUIPMENT_INSPECT_MAX_BYTES);
    release(&f);
}

static void selected_body_slot_keys(void) {
    // Literal native head, main hand, off hand and neck keys/buckets. Catches
    // inspection selecting an adjacent slot even when its accepted ID exists.
    const struct {unsigned index,key,bucket;} cases[]={{0,1,4},{2,3,6},{3,4,17},{12,13,8}};
    for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++) {
        NF f;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
        EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);
        put(&f,XI_BUCKETS+4*8,0,8);
        put(&f,XI_BUCKETS+cases[i].bucket*8,XI_NODE,8);
        put(&f,XI_NODE+8,cases[i].key,4);
        equipment_inspect(&h,&p,cases[i].index,&slot,&r);
        CHECK(r.shared_identity_valid&&r.item_valid&&r.base_name_available);
        CHECK(!strcmp(r.base_name,"Synthetic bronze helm"));
        release(&f);
    }
}

static void unidentified_and_registered_default(void) {
    for(unsigned kind=0;kind<5;kind++) {
        NF f;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
        EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);
        if(kind==0)put(&f,XI_ATTR_NODE+12,1,4);
        if(kind==1||kind==2)put(&f,XI_ATTR_BUCKETS+8*((0xFFFFF19Eu^(0xFFFFF19Eu>>12))&255),0,8);
        if(kind==2)put(&f,XI_DEFAULT,0x19F,4);
        if(kind==3){put(&f,XI_ATTR_NODE+8,1,4);put(&f,XI_ATTR_NODE,XI_ATTR_NODE,8);}
        if(kind==4)put(&f,XI_ATTR+4,2,1);
        equipment_inspect(&h,&p,0,&slot,&r);
        CHECK(r.shared_identity_valid&&r.item_valid);
        CHECK(r.base_name_available==(kind==1));
        if(kind!=1)CHECK(!r.base_name[0]);
        release(&f);
    }
}

static void dirty_key_fixture(NF *f) {
    // The group flag says some keys are dirty, not that this key is dirty.
    // Native constructor uses four-aligned arrays; key 0xFFFFF19E hashes to 1.
    put(f,XI_ATTR+4,0x0B,1);
    put(f,XI_ATTR+0x10A8,XI_DIRTY_BUCKETS,8);
    put(f,XI_DIRTY_BUCKETS+8,XI_DIRTY_NODE,8);
    put(f,XI_DIRTY_NODE+8,0xFFFFF1BEu,4); // A different key in the same bucket.
}

static void only_the_requested_dirty_key_blocks_inspection(void) {
    for(unsigned kind=0;kind<8;kind++) {
        NF f;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
        EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);dirty_key_fixture(&f);
        if(kind==1)put(&f,XI_DIRTY_BUCKETS+8,0,8);
        if(kind==2)put(&f,XI_DIRTY_NODE+8,0xFFFFF19Eu,4);
        if(kind==3)put(&f,XI_DIRTY_NODE,XI_DIRTY_NODE,8);
        if(kind==4)put(&f,XI_ATTR+0x10A8,XI_DIRTY_BUCKETS+2,8);
        if(kind==5)put(&f,XI_ATTR_NODE+12,1,4);
        if(kind==6)put(&f,XI_ATTR_BUCKETS+97*8,0,8); // Proven-absent cache key uses registered zero.
        if(kind==7)for(unsigned i=0;i<9;i++) {
            put(&f,XI_DIRTY_NODE+i*16,i==8?0:XI_DIRTY_NODE+(i+1)*16,8);
            put(&f,XI_DIRTY_NODE+i*16+8,0xFFFFF1BEu,4);
        }
        equipment_inspect(&h,&p,0,&slot,&r);
        CHECK(r.shared_identity_valid&&r.item_valid);
        const int available=kind==0||kind==1||kind==6;
        CHECK(r.base_name_available==available);
        if(!available)CHECK(!r.base_name[0]);
        CHECK(r.reads<=EQUIPMENT_INSPECT_MAX_READS&&r.bytes<=EQUIPMENT_INSPECT_MAX_BYTES);
        release(&f);
    }
}

static void dirty_key_read_failures_and_late_change(void) {
    NF f;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
    EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);dirty_key_fixture(&f);
    equipment_inspect(&h,&p,0,&slot,&r);CHECK(r.base_name_available);
    unsigned attempts[8]={0},count=0,second_text=0;
    for(unsigned i=0;i<f.maps;i++) {
        if(f.log[i]==XI_ATTR+0x10A8||f.log[i]==XI_DIRTY_BUCKETS+8||f.log[i]==XI_DIRTY_NODE) {
            CHECK(count<8);attempts[count++]=i+1;
        }
        if(f.log[i]==TEXT(0))second_text=i+1;
    }
    CHECK(count==6&&second_text);release(&f);
    for(unsigned mapping=0;mapping<2;mapping++)for(unsigned i=0;i<count;i++) {
        h=inspection_fixture(&f,&p,&slot);dirty_key_fixture(&f);
        if(mapping)f.mapfail=attempts[i];else f.fail=attempts[i];
        equipment_inspect(&h,&p,0,&slot,&r);
        CHECK(!r.base_name_available&&!r.base_name[0]);release(&f);
    }
    h=inspection_fixture(&f,&p,&slot);dirty_key_fixture(&f);
    f.mutate=second_text;f.addr=XI_DIRTY_NODE+8;f.value=0xFFFFF19Eu;f.size=4;
    equipment_inspect(&h,&p,0,&slot,&r);
    CHECK(f.seen==1&&!r.base_name_available&&!r.base_name[0]);release(&f);
}

static void malformed_inspection(void) {
    const struct {uint64_t a,v;unsigned n;} bad[]={
        {XI_CELL,XI_ITEM_ID+0x10000,4},{XI_ITEM,XI_ITEM_ID+0x10000,4},
        {XI_ITEM+0x38,1,4},{XI_PLAYER+0xC8,0,8},{XI_INVENTORY,7,4},
        {XI_TABLE+2*128,62,4},{XI_TABLE+2*128+0x18,3,4},
        {XI_GBNODE+0xC,100,4},{XI_GBNODE+0x10,1,4},{XI_GBNODE+8,0,4},
        {XI_INDICES+XI_RESOURCE_ID*4,UINT32_MAX,4},{XI_RESBASE,XI_RESOURCE_HANDLE+0x40000,4},
        {XI_RESBASE+7,21,1},{XI_OBJECT+8,0x42,4},{XI_OBJECT+0xC,4,4},
        {XI_OBJECT,XI_RESOURCE_ID+1,4},{XI_OBJECT+0x2C,801,4},
        {XI_ROW+0x10,XI_GBID+1,4},{XI_ROW+8,0,8},
        {TEXT(0),'<',1},{TEXT(0),'|',1},{TEXT(0),'[',1},{TEXT(0),0xFF,1},
        {MAIN+0x1A63CEC,0xFFFFFFFF,4},{G+0x1148,1,4},
        {O+8,0x42,4},{RB,RHANDLE+0x40000,4},
        {XI_INVENTORY+0x18,XI_BUCKETS+2,8}
    };
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++) {
        NF f;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
        EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);
        put(&f,bad[i].a,bad[i].v,bad[i].n);
        equipment_inspect(&h,&p,0,&slot,&r);
        // The language pair is an identity gate, not an enum-range assertion.
        if(i==23)CHECK(r.base_name_available);
        else CHECK(!r.base_name_available&&!r.base_name[0]);
        release(&f);
    }
}

static void inspection_failures_and_changes(void) {
    NF base;PlayerProbeIdentity p;EquipmentSlot slot;EquipmentInspectResult r;
    EdenDsmodHostApi bh=inspection_fixture(&base,&p,&slot);
    equipment_inspect(&bh,&p,0,&slot,&r);CHECK(r.base_name_available);
    const unsigned count=base.maps;
    unsigned second_text=0,second_key=0;
    for(unsigned i=0;i<count;i++) {
        if(base.log[i]==TEXT(0))second_text=i+1;
        if(base.log[i]==ARENA(0))second_key=i+1;
    }
    CHECK(second_text&&second_key);release(&base);
    for(unsigned mapping=0;mapping<2;mapping++)for(unsigned n=1;n<=count;n++) {
        NF f;EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);
        if(mapping)f.mapfail=n;else f.fail=n;
        memset(&r,0x7F,sizeof r);equipment_inspect(&h,&p,0,&slot,&r);
        CHECK(!r.base_name_available&&!r.base_name[0]);
        CHECK(r.reads==f.maps&&r.bytes==f.bytes);
        release(&f);
    }
    const struct {uint64_t a,v;unsigned n;} changes[]={
        {XI_CELL,UINT32_MAX,4},{XI_ITEM,XI_ITEM_ID+0x10000,4},{XI_PLAYER+0xC8,0,8},
        {XI_ROW+0x10,XI_GBID+1,4},{XI_GBNODE+0x10,1,4},{XI_OBJECT+8,0x42,4},
        {XI_ATTR+4,2,1},{XI_ATTR_NODE+12,1,4},{C+0x84,0,4},
        {MAIN+0x1A63CEC,2,4},{MAIN+0x1A63CF0,2,4},{G+0x1148,1,4},
        {TEXT(0),'X',1},{ARENA(0),'X',1},{XI_MANAGER_PTR,0,8}
    };
    for(unsigned fail=0;fail<2;fail++)for(unsigned i=0;i<sizeof changes/sizeof changes[0];i++) {
        NF f;EdenDsmodHostApi h=inspection_fixture(&f,&p,&slot);
        f.mutate=i==13?second_key:second_text;f.addr=changes[i].a;f.value=changes[i].v;f.size=changes[i].n;
        if(fail)f.fail=second_text;
        equipment_inspect(&h,&p,0,&slot,&r);
        if(f.seen!=1||r.base_name_available||r.base_name[0])fprintf(stderr,"change case %u failure %u seen %u\n",i,fail,f.seen);
        CHECK(f.seen==1&&!r.base_name_available&&!r.base_name[0]);
        if(i<3)CHECK(!r.item_valid);
        if(i==8)CHECK(!r.shared_identity_valid&&!r.item_valid);
        release(&f);
    }
}

int main(void){
    valid_inspection();selected_body_slot_keys();unidentified_and_registered_default();malformed_inspection();inspection_failures_and_changes();
    only_the_requested_dirty_key_blocks_inspection();dirty_key_read_failures_and_late_change();
    puts("PASS equipment base names: resident rows/localization, hidden items, defaults, malformed metadata, failure sweep and final ownership gates");return 0;
}
