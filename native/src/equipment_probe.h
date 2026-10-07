// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"

#define EQUIPMENT_SLOT_COUNT 13u
#define EQUIPMENT_MAX_READS 520u
#define EQUIPMENT_MAX_BYTES 7200u

// Stored order is native body-slot keys 1 through 13: head, torso, right hand,
// left hand, hands, waist, feet, shoulders, legs, bracers, right ring,
// left ring, neck. IDs are internal diagnostics, never presentation names.
typedef struct EquipmentSlot {
    int available, occupied;
    uint32_t acd_id;
    const char *reason;
} EquipmentSlot;

typedef struct EquipmentProbeResult {
    int shared_identity_valid;
    uint32_t reads, bytes;
    const char *reason;
    EquipmentSlot slots[EQUIPMENT_SLOT_COUNT];
} EquipmentProbeResult;

void equipment_probe(const EdenDsmodHostApi *host,
                     const PlayerProbeIdentity *expected,
                     EquipmentProbeResult *result);
