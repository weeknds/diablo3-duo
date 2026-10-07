// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"
#define HEALTH_MAX_READS 69u
#define HEALTH_MAX_BYTES 630u
typedef enum HealthPointerSlot { HP_CLIENT, HP_PLAYER, HP_ACD, HP_GROUP, HP_OWNER, HP_BUCKETS, HP_HEAD, HP_NODE, HP_POINTER_COUNT } HealthPointerSlot;
typedef struct HealthDiagnostics {
    const char *stage, *failure, *mode;
    uint64_t pointers[HP_POINTER_COUNT], access_address, pointer_value;
    uint32_t known_pointers, access_size;
    int access_known, pointer_known, flags_known;
    uint8_t flags;
} HealthDiagnostics;
typedef struct HealthProbeResult {
    int available, fraction_available, shared_identity_valid;
    float current, maximum, fraction;
    uint32_t reads, bytes;
    const char *reason;
    HealthDiagnostics diagnostic;
} HealthProbeResult;
void health_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected, HealthProbeResult *result);
