// SPDX-License-Identifier: GPL-3.0-or-later
#include "details_probe.h"
static void clear_all(DetailsProbeResult *r, const char *reason) {
    const uint32_t reads=r->reads, bytes=r->bytes;
    *r=(DetailsProbeResult){.reads=reads,.bytes=bytes,.reason=reason};
}
void details_probe(const EdenDsmodHostApi *host, DetailsProbeResult *result) {
    *result=(DetailsProbeResult){0};
    player_probe(host,&result->level);
    result->reads=result->level.reads;result->bytes=result->level.bytes;
    if(!result->level.available){clear_all(result,result->level.reason);return;}
    skills_probe(host,&result->level.identity,&result->skills);
    result->reads+=result->skills.reads;result->bytes+=result->skills.bytes;
    if(!result->skills.shared_identity_valid){clear_all(result,result->skills.reason);return;}
    stats_probe(host,&result->level.identity,&result->stats);
    result->reads+=result->stats.reads;result->bytes+=result->stats.bytes;
    if(!result->stats.shared_identity_valid){clear_all(result,result->stats.reason);return;}
    if(result->skills.available) {
        names_probe(host,&result->level.identity,result->skills.raw_slots,&result->names);
        result->reads+=result->names.reads;result->bytes+=result->names.bytes;
        if(!result->names.shared_identity_valid){clear_all(result,result->names.reason);return;}
        if(!result->names.skills_valid)result->skills=(SkillsProbeResult){.reason="UNAVAILABLE: stored selections changed"};
    }
    result->shared_identity_valid=1;
    result->reason="UNVERIFIED: consistent selected-player observations";
}
