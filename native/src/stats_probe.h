// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"
#include <stddef.h>
enum StatsField {
    STATS_APS, STATS_CDR, STATS_ARMOR, STATS_MOVEMENT,
    STATS_STRENGTH, STATS_DEXTERITY, STATS_INTELLIGENCE, STATS_VITALITY,
    STATS_CRIT_CHANCE, STATS_RESOURCE_COST,
    STATS_COUNT
};
#define STATS_MAX_READS 352u
#define STATS_MAX_BYTES 4826u
typedef struct StatValue { int available; float raw; const char *reason; } StatValue;
typedef struct StatsProbeResult { int shared_identity_valid; StatValue fields[STATS_COUNT]; uint32_t reads,bytes; const char *reason; } StatsProbeResult;
void stats_probe(const EdenDsmodHostApi *host,const PlayerProbeIdentity *expected,StatsProbeResult *result);
int stats_format(unsigned field,float value,char *text,size_t size);
