// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"
#include <stddef.h>
#define STATS_COUNT 4u
#define STATS_MAX_READS 122u
#define STATS_MAX_BYTES 1450u
typedef struct StatValue { int available; float raw; const char *reason; } StatValue;
typedef struct StatsProbeResult { int shared_identity_valid; StatValue fields[STATS_COUNT]; uint32_t reads,bytes; const char *reason; } StatsProbeResult;
void stats_probe(const EdenDsmodHostApi *host,const PlayerProbeIdentity *expected,StatsProbeResult *result);
int stats_format(unsigned field,float value,char *text,size_t size);
