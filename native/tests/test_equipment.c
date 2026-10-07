// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic memory fixtures; no values below are game observations.
#define main equipment_base_suite
#include "test_player_probe.c"
#undef main
#include "equipment_probe.h"

#define EA_GLOBALS UINT64_C(0x7400000000)
#define EA_POOL UINT64_C(0x7400001000)
#define EA_PAGES UINT64_C(0x7400002000)
#define EA_PAGE UINT64_C(0x7400010000)
#define EA_PLAYER (EA_PAGE + 2u * 0x360u)
#define EA_INVENTORY UINT64_C(0x7400020000)
// The device trace places this packed array at inventory +0x24 (four-aligned).
// Its entries are eight-byte pointers, but their addresses need not be eight-aligned.
#define EA_BUCKETS UINT64_C(0x7400030004)
#define EA_NODES UINT64_C(0x7400040000)
#define EA_VALUES UINT64_C(0x7400050000)
#define EA_CELLS UINT64_C(0x7400060000)
#define EA_OWNER_ID UINT32_C(0x56780002)
#define EA_ITEM_ID UINT32_C(0xABCD0003)
#define EA_OTHER_ITEM_ID UINT32_C(0xBCDE0000)
// Native enum values independently decoded from the exact-build Rela records.
// These constants must not be derived from the reader's key calculation.
static const unsigned slot_keys[13] = {1,2,3,4,5,6,7,8,9,10,11,12,13};
// Hand-checked FNV1a bucket indices for these little-endian keys.
static const unsigned slot_buckets[13] = {4,23,6,17,0,19,2,29,12,31,14,25,8};

static EdenDsmodHostApi equipment_setup(Fixture *f) {
    EdenDsmodHostApi h = setup(f);
    const uint64_t bases[] = {EA_GLOBALS,EA_POOL,EA_PAGES,EA_PAGE,EA_INVENTORY,EA_BUCKETS,EA_NODES,EA_VALUES,EA_CELLS};
    const size_t sizes[] = {8,0x170,8,0x1000,0x20,0x100,0x1A00,0x100,0x100};
    for (unsigned i=0;i<9;i++) {
        f->regions[i+8]=(Region){bases[i],sizes[i],calloc(1,sizes[i])};
        CHECK(f->regions[i+8].bytes);
    }
    put32(f,PLAYER+4,EA_OWNER_ID);
    put64(f,C+0x9A0,EA_GLOBALS);put64(f,EA_GLOBALS,EA_POOL);
    put32(f,EA_POOL+0x100,4);put32(f,EA_POOL+0x168,2);
    put64(f,EA_POOL+0x120,EA_PAGES);put64(f,EA_PAGES,EA_PAGE);
    put32(f,EA_PLAYER,EA_OWNER_ID);put64(f,EA_PLAYER+0xC8,EA_INVENTORY);
    put32(f,EA_INVENTORY,1);put64(f,EA_INVENTORY+0x18,EA_BUCKETS);
    for (unsigned i=0;i<13;i++) {
        const uint64_t n=EA_NODES+0x200*i;
        put64(f,EA_BUCKETS+8*slot_buckets[i],n);
        put32(f,n+8,slot_keys[i]);put64(f,n+0x10,EA_VALUES+8*i);
        put64(f,EA_VALUES+8*i,EA_CELLS+8*i);
        put32(f,EA_CELLS+8*i,UINT32_MAX);
    }
    // Head occupied, other twelve slots empty.
    put32(f,EA_CELLS,EA_ITEM_ID);put32(f,EA_PAGE+3*0x360,EA_ITEM_ID);
    PlayerProbeResult p;player_probe(&h,&p);CHECK(p.available);
    f->initial_identity=p.identity;f->calls=f->maps=0;
    return h;
}

// Catches accepting stale item generations, reading the wrong body slot, or
// turning a genuinely empty cell into unavailable.
static void occupied_and_empty(void) {
    Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(r.shared_identity_valid);
    CHECK(r.slots[0].available&&r.slots[0].occupied&&r.slots[0].acd_id==EA_ITEM_ID);
    for(unsigned i=1;i<13;i++) CHECK(r.slots[i].available&&!r.slots[i].occupied);
    CHECK(r.reads<=EQUIPMENT_MAX_READS&&r.bytes<=EQUIPMENT_MAX_BYTES&&!f.writes);
    release(&f);
}

// Native keys 3 and 4 are the hands. The former +2 reader misreported this
// synthetic pair as torso/main hand and silently skipped the head key.
static void hand_slots_do_not_shift_into_torso(void) {
    Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
    put32(&f,EA_CELLS,UINT32_MAX);
    put32(&f,EA_CELLS+2*8,EA_ITEM_ID);
    put32(&f,EA_CELLS+3*8,EA_OTHER_ITEM_ID);
    put32(&f,EA_PAGE,EA_OTHER_ITEM_ID);
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(r.shared_identity_valid);
    for(unsigned i=0;i<13;i++) {
        CHECK(r.slots[i].available);
        CHECK(r.slots[i].occupied==(i==2||i==3));
    }
    CHECK(r.slots[2].acd_id==EA_ITEM_ID);
    CHECK(r.slots[3].acd_id==EA_OTHER_ITEM_ID);
    CHECK(!f.writes);
    release(&f);
}

static void item_generation_and_missing_slots(void) {
    Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
    put32(&f,EA_PAGE+3*0x360,EA_ITEM_ID+0x10000u);
    put64(&f,EA_BUCKETS+8*slot_buckets[1],0);
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(r.shared_identity_valid);
    CHECK(!r.slots[0].available&&!r.slots[0].occupied&&!r.slots[0].acd_id);
    CHECK(!r.slots[1].available&&!r.slots[1].occupied);
    CHECK(r.slots[2].available&&!r.slots[2].occupied);
    release(&f);
}

static void invalid_bucket_alignment(void) {
    Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
    put64(&f,EA_INVENTORY+0x18,EA_BUCKETS+2);
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(!r.shared_identity_valid);
    for(unsigned i=0;i<13;i++)CHECK(!r.slots[i].available&&!r.slots[i].occupied);
    release(&f);
}

static void bounded_chains(void) {
    Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
    // A malformed chain must not hang or become an empty equipment slot.
    put32(&f,EA_NODES+8,100);put64(&f,EA_NODES,EA_NODES);
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(r.shared_identity_valid&&!r.slots[0].available);
    CHECK(strstr(r.slots[0].reason,"cycle"));
    // Longest admitted chains in every slot still leave budget for the final
    // ownership snapshot. Keys deliberately collide; no guest hash helper runs.
    for(unsigned i=0;i<13;i++) {
        const uint64_t base=EA_NODES+0x200*i;
        for(unsigned j=0;j<8;j++) {
            const uint64_t n=base+0x20*j;
            put64(&f,n,j==7?0:n+0x20);put32(&f,n+8,j==7?slot_keys[i]:100u+j);
        }
        put64(&f,base+0xE0+0x10,EA_VALUES+8*i);
    }
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(r.shared_identity_valid);
    for(unsigned i=0;i<13;i++)CHECK(r.slots[i].available);
    CHECK(r.reads<=EQUIPMENT_MAX_READS&&r.bytes<=EQUIPMENT_MAX_BYTES);
    put32(&f,EA_NODES+0xE0+8,200);
    equipment_probe(&h,&f.initial_identity,&r);
    CHECK(!r.slots[0].available&&strstr(r.slots[0].reason,"node limit"));
    release(&f);
}

typedef struct EquipmentFault {
    Fixture memory;
    uint64_t trigger,mutated_address;
    uint64_t new_value;
    size_t new_size;
    unsigned occurrence,seen;
    int fail;
} EquipmentFault;

static EdenDsmodBool equipment_fault_read(void *p,uint64_t a,void *out,size_t n) {
    EquipmentFault *f=p;
    if(a==f->trigger&&++f->seen==f->occurrence) {
        if(f->new_size)put(&f->memory,f->mutated_address,f->new_value,f->new_size);
        if(f->fail)return EDEN_DSMOD_FALSE;
    }
    return read_mem(p,a,out,n);
}

static void changes_and_field_failure(void) {
    EquipmentFault f={0};EdenDsmodHostApi h=equipment_setup(&f.memory);
    h.read_memory=equipment_fault_read;
    f.trigger=EA_CELLS;f.occurrence=2;
    f.mutated_address=EA_CELLS;f.new_value=UINT32_MAX;f.new_size=4;
    EquipmentProbeResult r;
    equipment_probe(&h,&f.memory.initial_identity,&r);
    CHECK(r.shared_identity_valid&&!r.slots[0].available&&!r.slots[0].occupied);
    CHECK(strstr(r.slots[0].reason,"changed"));
    release(&f.memory);

    f=(EquipmentFault){0};h=equipment_setup(&f.memory);h.read_memory=equipment_fault_read;
    f.trigger=EA_CELLS;f.occurrence=1;f.fail=1;
    f.mutated_address=EA_PLAYER+0xC8;f.new_value=0;f.new_size=8;
    memset(&r,0x7F,sizeof r);
    equipment_probe(&h,&f.memory.initial_identity,&r);
    CHECK(!r.shared_identity_valid);
    for(unsigned i=0;i<13;i++)CHECK(!r.slots[i].available&&!r.slots[i].occupied&&!r.slots[i].acd_id);
    CHECK(strstr(r.reason,"owner changed"));
    release(&f.memory);
}

static void failure_sweep(void) {
    Fixture base;EdenDsmodHostApi bh=equipment_setup(&base);EquipmentProbeResult good;
    equipment_probe(&bh,&base.initial_identity,&good);
    const unsigned count=base.calls;release(&base);
    for(unsigned failure=1;failure<=count;failure++) {
        Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
        f.fail_on=failure;equipment_probe(&h,&f.initial_identity,&r);
        unsigned unavailable=0;for(unsigned i=0;i<13;i++)unavailable+=!r.slots[i].available;
        CHECK(!r.shared_identity_valid||unavailable);
        CHECK(r.reads<=EQUIPMENT_MAX_READS&&r.bytes<=EQUIPMENT_MAX_BYTES&&!f.writes);
        release(&f);
    }
    for(unsigned failure=1;failure<=count;failure++) {
        Fixture f;EdenDsmodHostApi h=equipment_setup(&f);EquipmentProbeResult r;
        f.map_fail_on=failure;equipment_probe(&h,&f.initial_identity,&r);
        unsigned unavailable=0;for(unsigned i=0;i<13;i++)unavailable+=!r.slots[i].available;
        CHECK(!r.shared_identity_valid||unavailable);
        CHECK(r.reads<=EQUIPMENT_MAX_READS&&r.bytes<=EQUIPMENT_MAX_BYTES&&!f.writes);
        release(&f);
    }
}

int main(void) {
    occupied_and_empty();hand_slots_do_not_shift_into_torso();
    item_generation_and_missing_slots();invalid_bucket_alignment();bounded_chains();
    changes_and_field_failure();failure_sweep();
    puts("PASS equipment: occupancy, generation, missing keys, bounded chains, changes, failure sweep; no writes");
    return 0;
}
