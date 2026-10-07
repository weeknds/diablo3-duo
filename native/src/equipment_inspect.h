// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "equipment_probe.h"

#define EQUIPMENT_INSPECT_TEXT_CAP 512u
#define EQUIPMENT_INSPECT_MAX_READS 600u
#define EQUIPMENT_INSPECT_MAX_BYTES 30000u

typedef struct EquipmentInspectResult {
    int shared_identity_valid, item_valid, base_name_available;
    char base_name[EQUIPMENT_INSPECT_TEXT_CAP];
    uint32_t reads, bytes;
    const char *reason;
} EquipmentInspectResult;

// A localized base-item label, not a full affixed/rare item name. Intended for
// the currently inspected equipped slot; never reveals an unidentified item.
void equipment_inspect(const EdenDsmodHostApi *host,
                       const PlayerProbeIdentity *expected,
                       unsigned slot, const EquipmentSlot *accepted,
                       EquipmentInspectResult *out);
