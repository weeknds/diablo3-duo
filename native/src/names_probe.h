// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "skills_probe.h"
#define NAMES_MAX_READS 298u
#define NAMES_MAX_BYTES 13200u
#define NAMES_CLOSING_READS 137u
#define NAMES_CLOSING_BYTES 2760u
#define NAMES_TEXT_CAP 512u
#define NAMES_INTERNAL_CAP 128u
typedef struct SkillName { int available; char text[NAMES_TEXT_CAP]; } SkillName;
typedef struct NamesProbeResult {
    int shared_identity_valid, skills_valid;
    SkillName fields[SKILL_SLOT_COUNT];
    uint32_t reads, bytes; /* attempted mapped spans, including mapping failures */
    const char *reason;
} NamesProbeResult;
void names_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected,
                 const StoredSkillPair accepted[SKILL_SLOT_COUNT], NamesProbeResult *out);
