// SPDX-License-Identifier: GPL-3.0-or-later
#include "skills_probe.h"
#include "probe_internal.h"

_Static_assert(sizeof(StoredSkillPair) == 8, "Stored pairs contain exactly two 32-bit words");

static int same_identity(Reader *r, const PlayerProbeIdentity *expected) {
    Snapshot current;
    if (!snapshot(r, &current)) return 0;
    if (memcmp(&current, expected, sizeof current)) {
        r->out->reason = "UNAVAILABLE: selected player identity changed";
        return 0;
    }
    return 1;
}

static int read_pairs(Reader *r, uint64_t player, StoredSkillPair pairs[SKILL_SLOT_COUNT]) {
    for (uint32_t i = 0; i < SKILL_SLOT_COUNT; ++i) {
        // Current-build six-slot getter: Player+0x134C/+0x1350, stride0x10.
        // Host copies eight bytes into an aligned local pair; guest address is 4-aligned.
        if (!read_offset(r, player, UINT64_C(0x134C) + UINT64_C(0x10) * i, &pairs[i], 8, 4)) return 0;
    }
    return 1;
}

void skills_probe(const EdenDsmodHostApi *host, const PlayerProbeIdentity *expected, SkillsProbeResult *result) {
    *result = (SkillsProbeResult){.reason="UNAVAILABLE: invalid host/main region"};
    if (!expected || !host || !host->userdata || !host->read_memory || !host->is_mapped ||
        !host->main_base || host->main_base % 8 || host->main_size < ROOT_OFFSET + 8 ||
        host->main_base > UINT64_MAX - host->main_size) return;
    PlayerProbeResult work = {0};
    Reader reader = {host, &work, SKILLS_MAX_READS, SKILLS_MAX_BYTES, NULL};
    StoredSkillPair before[SKILL_SLOT_COUNT] = {0}, after[SKILL_SLOT_COUNT] = {0};
    const char *skill_failure = "UNAVAILABLE: stored skill pair read";
    int complete = 0;
    if (!same_identity(&reader, expected)) goto done;
    if (!read_pairs(&reader, expected->player, before)) goto final_identity;
    if (!same_identity(&reader, expected)) goto done;
    if (!read_pairs(&reader, expected->player, after)) goto final_identity;
    complete = 1;
final_identity:
    // Also recheck ownership after a skill-only failure before preserving level.
    if (!same_identity(&reader, expected)) goto done;
    result->shared_identity_valid = 1;
    if (!complete) { work.reason = skill_failure; goto done; }
    if (memcmp(before, after, sizeof before)) {
        work.reason = "UNAVAILABLE: stored skill snapshot changed";
        goto done;
    }
    for (uint32_t i = 0; i < SKILL_SLOT_COUNT; ++i) {
        if (before[i].power_sno < -1 || (before[i].power_sno != -1 && (before[i].rune < -1 || before[i].rune > 4))) {
            work.reason = "UNAVAILABLE: stored skill ID/rune range";
            goto done;
        }
    }
    for (uint32_t i = 0; i < SKILL_SLOT_COUNT; ++i) {
        result->raw_slots[i] = before[i];
        result->slots[i] = before[i];
        if (before[i].power_sno == -1) result->slots[i].rune = -1;
    }
    result->available = 1;
    work.reason = "UNVERIFIED: numbered stored selections; IDs not named";
done:
    result->reads = work.reads; result->bytes = work.bytes; result->reason = work.reason;
}
