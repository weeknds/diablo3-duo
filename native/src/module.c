// SPDX-License-Identifier: GPL-3.0-or-later
#include "details_probe.h"
#include "name_layout.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE_ID UINT64_C(0x01001B300B9BE000)
#define REQUIRED_CAPS EDEN_DSMOD_CAP_NO_TICK_WHEN_HIDDEN
static const char expected_build[] = "2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000";
static const uint8_t expected_bytes[32] = {0x26,0x07,0xA7,0x4F,0x5D,0xF7,0x75,0x4C,0xC0,0x35,0x7B,0x5D,0xF7,0xE4,0x96,0x93,0x13,0x55,0xD8,0xCA,0,0,0,0,0,0,0,0,0,0,0,0};
typedef struct ModuleState { int rejected; } ModuleState;

static EdenDsmodBool supports(const char *build) { return build && !strcmp(build, expected_build); }
static int valid_host(const EdenDsmodHostApi *h) {
    // Only the initial eight-byte ABI prefix is read before validating the complete size.
    return h && h->abi_version == EDEN_DSMOD_MODULE_ABI_VERSION && h->struct_size == sizeof *h &&
        h->abi_hash == EDEN_DSMOD_MODULE_ABI_HASH && h->userdata &&
        (h->capabilities & REQUIRED_CAPS) == REQUIRED_CAPS && h->get_tick &&
        h->is_mapped && h->read_memory && h->begin_output && h->end_output &&
        h->publish_i64 && h->publish_text;
}
static int valid_identity(const EdenDsmodHostApi *h) {
    return h->title_id == TITLE_ID && !memcmp(h->build_id, expected_bytes, sizeof expected_bytes);
}
static void *create(const EdenDsmodHostApi *h, const char *config) {
    (void)config;
    return valid_host(h) && valid_identity(h) ? calloc(1, sizeof(ModuleState)) : NULL;
}

static void sample(void *instance, const EdenDsmodHostApi *h) {
    ModuleState *state=instance;
    if(!state)return;
    if(!valid_host(h)){state->rejected=1;return;}
    if(!valid_identity(h))state->rejected=1;
    DetailsProbeResult result={0};
    if(!state->rejected)details_probe(h,&result);
    const int available=result.shared_identity_valid;
    char level[64];
    if(available)snprintf(level,sizeof level,"Level %" PRId32,result.level.level);
    else snprintf(level,sizeof level,"Level unavailable");
    const char *names[SKILL_SLOT_COUNT], *runes[SKILL_SLOT_COUNT];
    char values[STATS_COUNT][28];
    int partial=0;
    for(unsigned i=0;i<SKILL_SLOT_COUNT;i++) {
        const StoredSkillPair *pair=&result.skills.slots[i];
        names[i]="Unavailable";runes[i]="";
        if(available&&result.skills.available) {
            if(pair->power_sno==-1)names[i]="Unassigned";
            else {
                if(result.names.fields[i].available && name_fits(result.names.fields[i].text))
                    names[i]=result.names.fields[i].text;
                else partial=1;
                runes[i]=pair->rune==-1?"No rune selected":"Rune unavailable";
                if(pair->rune!=-1)partial=1;
            }
        } else partial=1;
    }
    for(unsigned i=0;i<STATS_COUNT;i++) {
        const StatValue *v=&result.stats.fields[i];
        // A value that cannot fit the bounded presentation buffer is unavailable,
        // never truncated or silently ellipsized into a different number.
        if(!available||!v->available||!stats_format(i,v->raw,values[i],sizeof values[i])) {
            snprintf(values[i],sizeof values[i],"Unavailable");partial=1;
        }
    }
    const char *status;
    if(state->rejected)status="Companion unavailable. Reload for the supported research build.";
    else if(!available)status="Character data unavailable. Start a game or wait for loading.";
    else if(partial)status="Some details are unavailable in this private preview.";
    else status="Read-only character view.";
    h->begin_output(h->userdata);
    h->publish_text(h->userdata,"details.level",level);
    h->publish_text(h->userdata,"details.status",status);
    for(unsigned i=0;i<SKILL_SLOT_COUNT;i++) {
        char key[40];
        snprintf(key,sizeof key,"skills.%u.name",i);h->publish_text(h->userdata,key,names[i]);
        snprintf(key,sizeof key,"skills.%u.rune",i);h->publish_text(h->userdata,key,runes[i]);
    }
    for(unsigned i=0;i<STATS_COUNT;i++) {
        char key[40];snprintf(key,sizeof key,"stats.%u.value",i);
        h->publish_text(h->userdata,key,values[i]);
    }
    h->end_output(h->userdata);
    // Every row is replaced per sample. No guest pointers or values persist.
}
static void tick(void *instance, const EdenDsmodHostApi *h) { (void)instance; (void)h; }
static void destroy(void *instance) { free(instance); }
static const EdenDsmodModuleApi module = {
    .abi_version=EDEN_DSMOD_MODULE_ABI_VERSION, .struct_size=sizeof(EdenDsmodModuleApi),
    .abi_hash=EDEN_DSMOD_MODULE_ABI_HASH, .title_id=TITLE_ID,
    .name="Diablo III Duo skill names 0.0.10-research", .capabilities=REQUIRED_CAPS,
    .supports_build=supports, .create=create, .destroy=destroy, .sample=sample, .tick=tick
};
#if defined(__GNUC__)
__attribute__((visibility("default")))
#endif
const EdenDsmodModuleApi *eden_dsmod_get_module(uint32_t version, uint64_t hash) {
    return version == EDEN_DSMOD_MODULE_ABI_VERSION && hash == EDEN_DSMOD_MODULE_ABI_HASH ? &module : NULL;
}
