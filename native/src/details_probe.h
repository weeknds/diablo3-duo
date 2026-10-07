// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"
#include "skills_probe.h"
#include "stats_probe.h"
#include "names_probe.h"
#define DETAILS_MAX_READS (27u + SKILLS_MAX_READS + STATS_MAX_READS + NAMES_MAX_READS)
#define DETAILS_MAX_BYTES (212u + SKILLS_MAX_BYTES + STATS_MAX_BYTES + NAMES_MAX_BYTES)
typedef struct DetailsProbeResult {
    int shared_identity_valid;
    PlayerProbeResult level;
    SkillsProbeResult skills;
    StatsProbeResult stats;
    NamesProbeResult names;
    uint32_t reads, bytes;
    const char *reason;
} DetailsProbeResult;
void details_probe(const EdenDsmodHostApi *host, DetailsProbeResult *result);
