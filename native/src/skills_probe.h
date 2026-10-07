// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"
#define SKILL_SLOT_COUNT 6u
#define SKILLS_MAX_READS 51u
#define SKILLS_MAX_BYTES 408u
typedef struct StoredSkillPair { int32_t power_sno, rune; } StoredSkillPair;
typedef struct SkillsProbeResult {
    int available, shared_identity_valid;
    StoredSkillPair slots[SKILL_SLOT_COUNT];
    StoredSkillPair raw_slots[SKILL_SLOT_COUNT]; /* accepted original empty rune bits */
    uint32_t reads, bytes;
    const char *reason;
} SkillsProbeResult;
void skills_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected, SkillsProbeResult *result);
