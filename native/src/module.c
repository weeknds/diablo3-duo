// SPDX-License-Identifier: GPL-3.0-or-later
#include "details_probe.h"
#include "name_layout.h"
#include "equipment_probe.h"
#include "equipment_inspect.h"
#include "map_view.h"
#include "dsmod_module_extensions.h"
#include <pthread.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE_ID UINT64_C(0x01001B300B9BE000)
#define REQUIRED_CAPS (EDEN_DSMOD_CAP_NO_TICK_WHEN_HIDDEN | EDEN_DSMOD_CAP_EXTENSIONS)
static const char expected_build[] = "2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000";
static const uint8_t expected_bytes[32] = {0x26,0x07,0xA7,0x4F,0x5D,0xF7,0x75,0x4C,0xC0,0x35,0x7B,0x5D,0xF7,0xE4,0x96,0x93,0x13,0x55,0xD8,0xCA,0,0,0,0,0,0,0,0,0,0,0,0};
typedef struct ModuleState {
    int rejected, cached;
    uint64_t next_refresh, next_navigation_refresh;
    PlayerProbeIdentity owner;
    EquipmentProbeResult equipment;
    EquipmentInspectResult inspection;
    int selected_slot;
    MapView map, displayed, pending, previous;
    uint64_t pending_observed;
    int pending_loaded, reset_pending;
    pthread_mutex_t map_lock;
} ModuleState;

static EdenDsmodBool supports(const char *build) { return build && !strcmp(build, expected_build); }
static int valid_host(const EdenDsmodHostApi *h) {
    // Only the initial eight-byte ABI prefix is read before validating the complete size.
    return h && h->abi_version == EDEN_DSMOD_MODULE_ABI_VERSION && h->struct_size == sizeof *h &&
        h->abi_hash == EDEN_DSMOD_MODULE_ABI_HASH && h->userdata &&
        (h->capabilities & REQUIRED_CAPS) == REQUIRED_CAPS && h->get_tick &&
        h->is_mapped && h->read_memory && h->begin_output && h->end_output &&
        h->publish_i64 && h->publish_f64 && h->publish_text;
}
static int valid_identity(const EdenDsmodHostApi *h) {
    return h->title_id == TITLE_ID && !memcmp(h->build_id, expected_bytes, sizeof expected_bytes);
}
static void *create(const EdenDsmodHostApi *h, const char *config) {
    (void)config;
    if (!valid_host(h) || !valid_identity(h)) return NULL;
    ModuleState *state=calloc(1,sizeof *state);
    if (!state) return NULL;
    if (pthread_mutex_init(&state->map_lock,NULL)) { free(state); return NULL; }
    map_view_clear(&state->map); state->selected_slot=-1;
    return state;
}

static void clear_cached(ModuleState *state) {
    state->equipment=(EquipmentProbeResult){0};
    state->inspection=(EquipmentInspectResult){0}; state->selected_slot=-1;
    pthread_mutex_lock(&state->map_lock);
    if (state->map.data.available || state->map.pinned) map_view_clear(&state->map);
    state->displayed=(MapView){0};state->pending=(MapView){0};state->previous=(MapView){0};
    state->pending_observed=0;state->pending_loaded=0;
    pthread_mutex_unlock(&state->map_lock);
    state->reset_pending=1;
    state->cached=0;
    state->next_navigation_refresh=0;
}

static void sample(void *instance, const EdenDsmodHostApi *h) {
    ModuleState *state=instance;
    if(!state)return;
    if(!valid_host(h)){state->rejected=1;return;}
    if(!valid_identity(h))state->rejected=1;
    DetailsProbeResult result={0};
    if(!state->rejected)details_probe(h,&result);
    int available=result.shared_identity_valid;
    const uint64_t now=h->get_tick(h->userdata);
    const int changed=available && (!state->cached || memcmp(&state->owner,&result.level.identity,sizeof state->owner));
    if (!available || changed) {
        clear_cached(state);
    }
    if (available && (!state->cached || now>=state->next_refresh)) {
        equipment_probe(h,&result.level.identity,&state->equipment);
        state->inspection=(EquipmentInspectResult){0};
        if (state->selected_slot>=0 && state->selected_slot<(int)EQUIPMENT_SLOT_COUNT && state->equipment.shared_identity_valid)
            equipment_inspect(h,&result.level.identity,(unsigned)state->selected_slot,
                              &state->equipment.slots[state->selected_slot],&state->inspection);
        MapProbeResult map={0};
        map_probe(h,&result.level.identity,&map);
        pthread_mutex_lock(&state->map_lock);
        const int new_world=!state->map.data.available||state->map.data.world_id!=map.world_id;
        map_view_update(&state->map,&map);
        if(new_world)state->reset_pending=1;
        pthread_mutex_unlock(&state->map_lock);
        if (map.available && (new_world||now>=state->next_navigation_refresh)) {
            NavProbeResult navigation={0};
            nav_probe(h,&result.level.identity,map.world_id,&navigation);
            pthread_mutex_lock(&state->map_lock);
            map_view_update_navigation(&state->map,&navigation);
            pthread_mutex_unlock(&state->map_lock);
            state->next_navigation_refresh=now+60; // Static terrain needs only 1Hz.
        }
        state->owner=result.level.identity; state->cached=1;
        state->next_refresh=now+15; // Runtime ticks are 60Hz; bound heavy scans to 4Hz.
    }
    // A world can change without changing the selected-player handle. Do not
    // keep the previous area's image or pin until the next 4Hz mask refresh.
    if (available) {
        pthread_mutex_lock(&state->map_lock);
        const int map_available=state->map.data.available;
        pthread_mutex_unlock(&state->map_lock);
        if (map_available) {
            uint32_t world=UINT32_MAX;
            float x=0,y=0;
            const int current=map_current_position(h,&result.level.identity,&world,&x,&y);
            pthread_mutex_lock(&state->map_lock);
            if (!current || world!=state->map.data.world_id) {
                map_view_clear(&state->map);
                state->reset_pending=1;
                state->next_refresh=0;
                state->next_navigation_refresh=0;
            } else {
                state->map.data.x=x;state->map.data.y=y;
            }
            pthread_mutex_unlock(&state->map_lock);
        }
        // Close all readers, including the world check, with the selected
        // player. No optional scan can leave earlier details visible on quit.
        PlayerProbeResult closing={0};
        player_probe(h,&closing);
        if (!closing.available || memcmp(&closing.identity,&result.level.identity,sizeof closing.identity)) {
            available=0;
            clear_cached(state);
        }
    }
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
    if(state->rejected)status="Companion unavailable. Check the game version and reload.";
    else if(!available)status="Character data unavailable. Start a game or wait for loading.";
    else if(partial)status="Some details are unavailable in this preview.";
    else status="Live character details.";
    h->begin_output(h->userdata);
    h->publish_text(h->userdata,"details.level",level);
    h->publish_text(h->userdata,"details.status",status);
    const int class_index=available&&result.level.hero_class_available?result.level.hero_class+1:0;
    h->publish_i64(h->userdata,"details.class_index",class_index);
    char paragon[64];
    if (available&&result.level.paragon_available)
        snprintf(paragon,sizeof paragon,"Paragon %" PRId32,result.level.paragon);
    else snprintf(paragon,sizeof paragon,"Paragon unavailable");
    h->publish_text(h->userdata,"details.paragon",paragon);
    for (unsigned i=0;i<EQUIPMENT_SLOT_COUNT;i++) {
        const EquipmentSlot *slot=&state->equipment.slots[i];
        const char *text="Unavailable";
        if (available&&state->equipment.shared_identity_valid&&slot->available)
            text=slot->occupied?"Equipped item":"Empty slot";
        char key[40];snprintf(key,sizeof key,"gear.%u.name",i);
        h->publish_text(h->userdata,key,text);
    }
    static const char *slot_names[EQUIPMENT_SLOT_COUNT]={"Head","Torso","Off-hand","Main hand",
        "Hands","Waist","Feet","Shoulders","Legs","Bracers","Right ring","Left ring","Neck"};
    const char *selected_name="Select an equipment slot", *selected_slot="Equipment inspection";
    const char *inspect_note="Tap an equipped slot to see its base item name.";
    if (available&&state->selected_slot>=0&&state->selected_slot<(int)EQUIPMENT_SLOT_COUNT) {
        const EquipmentSlot *slot=&state->equipment.slots[state->selected_slot];
        selected_slot=slot_names[state->selected_slot];
        selected_name="Unavailable";inspect_note="Item details are unavailable.";
        if (state->equipment.shared_identity_valid&&slot->available&&!slot->occupied) {
            selected_name="Empty slot";inspect_note="Nothing is equipped in this slot.";
        } else if (state->inspection.shared_identity_valid&&state->inspection.item_valid&&
                   state->inspection.base_name_available) {
            selected_name=state->inspection.base_name;
            inspect_note="Base item name. Rolled affixes and comparisons are not shown.";
        }
    }
    h->publish_text(h->userdata,"gear.selected_slot",selected_slot);
    h->publish_text(h->userdata,"gear.selected_name",selected_name);
    h->publish_text(h->userdata,"gear.inspect_note",inspect_note);
    pthread_mutex_lock(&state->map_lock);
    const MapView *view=&state->map;
    /* Keep terrain generations immutable while the host's asynchronous asset
     * worker loads them. Walking/zoom/pins never schedule a terrain image.
     * The pinned runtime drains completed images before sample(). Observe a
     * completed sink, then allow four further drain ticks before promotion. */
    if (!view->data.available ||
        (state->displayed.data.available&&state->displayed.data.world_id!=view->data.world_id) ||
        (state->pending.data.available&&state->pending.data.world_id!=view->data.world_id)) {
        state->displayed=(MapView){0};state->pending=(MapView){0};state->previous=(MapView){0};
        state->pending_loaded=0;state->pending_observed=0;
    }
    if (state->pending_loaded) {
        if (!state->pending_observed) state->pending_observed=now;
        if (now-state->pending_observed>=4) {
            state->previous=state->displayed;state->displayed=state->pending;
            state->pending=(MapView){0};state->pending_loaded=0;state->pending_observed=0;
        }
    }
    if (view->data.available&&!state->pending.data.available&&
        (!state->displayed.data.available||state->displayed.revision!=view->revision))
        state->pending=*view; // Coalesce later changes while this generation loads.
    const int terrain_available=view->navigation.available&&view->data.exploration_available;
    const int ready=available&&view->data.available&&state->displayed.data.available;
    h->publish_i64(h->userdata,"map.available",available&&view->data.available);
    h->publish_i64(h->userdata,"map.ready",ready);
    h->publish_i64(h->userdata,"map.reset_pending",state->reset_pending);state->reset_pending=0;
    h->publish_i64(h->userdata,"map.pinned",ready&&view->pinned);
    char map_key[80],pending_key[80];
    snprintf(map_key,sizeof map_key,"module:exploration:%" PRIu64,state->displayed.revision);
    snprintf(pending_key,sizeof pending_key,"module:exploration:%" PRIu64,
             state->pending.data.available?state->pending.revision:state->displayed.revision);
    h->publish_text(h->userdata,"map.image",map_key);
    h->publish_text(h->userdata,"map.pending",pending_key);
    h->publish_text(h->userdata,"map.pin.image","file:assets/icon-map-pin.png");
    MapProjection projection;
    if (ready&&map_view_projection(&state->displayed,&projection)) {
        float x,y,px,py;
        map_view_point(&projection,view->data.x,view->data.y,&x,&y);
        map_view_point(&projection,view->pin_x,view->pin_y,&px,&py);
        const double cx=x*MAP_COORD_SCALE,cy=(MAP_IMAGE_HEIGHT-y)*MAP_COORD_SCALE;
        const double half=MAP_WINDOW_WIDTH*projection.scale*MAP_COORD_SCALE/(2*view->zoom);
        const double height=half*MAP_IMAGE_HEIGHT/MAP_IMAGE_WIDTH;
        h->publish_f64(h->userdata,"map.player.x",cx);h->publish_f64(h->userdata,"map.player.y",cy);
        h->publish_f64(h->userdata,"map.pin.x",px*MAP_COORD_SCALE);
        h->publish_f64(h->userdata,"map.pin.y",(MAP_IMAGE_HEIGHT-py)*MAP_COORD_SCALE);
        h->publish_f64(h->userdata,"map.view.x0",cx-half);h->publish_f64(h->userdata,"map.view.x1",cx+half);
        h->publish_f64(h->userdata,"map.view.y0",cy-height);h->publish_f64(h->userdata,"map.view.y1",cy+height);
    }
    h->publish_text(h->userdata,"map.description",view->data.available?
        (terrain_available?"Drag to explore  /  Pinch to zoom  /  White marker: you":
         view->data.exploration_available?"Explored areas  /  White marker: you  /  Gold marker: your pin":
         "Your position is available. Explored areas are unavailable."):
        "Enter the world to begin exploring.");
    h->publish_text(h->userdata,"map.waiting",view->data.available?
                    "Drawing explored terrain...":"Exploration data unavailable");
    h->publish_text(h->userdata,"map.status",map_view_status(view));
    pthread_mutex_unlock(&state->map_lock);
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
    // Every row is replaced per sample. Heavy readers cache at most 15 ticks;
    // their numeric results are cleared when selected-player ownership changes.
}
static void tick(void *instance, const EdenDsmodHostApi *h) { (void)instance; (void)h; }
static void destroy(void *instance) {
    ModuleState *state=instance;
    if (state) pthread_mutex_destroy(&state->map_lock);
    free(state);
}
static void configure(void *instance,const EdenDsmodHostExtensions *host) {
    (void)instance;(void)host; // No guest mailbox or write service is requested.
}
static EdenDsmodBool action(void *instance,const char *name,int64_t argument) {
    ModuleState *state=instance;
    if (!state || state->rejected || !name) return 0;
    if (!strcmp(name,"inspect_equipment")) {
        if (!state->cached || argument<0 || argument>=EQUIPMENT_SLOT_COUNT) return 0;
        state->selected_slot=(int)argument; state->next_refresh=0;
        state->inspection=(EquipmentInspectResult){0};
        return EDEN_DSMOD_TRUE;
    }
    pthread_mutex_lock(&state->map_lock);
    const int accepted=map_view_action(&state->map,name);
    if(accepted&&(!strcmp(name,"map_zoom_in")||!strcmp(name,"map_zoom_out")))state->reset_pending=1;
    pthread_mutex_unlock(&state->map_lock);
    return accepted?EDEN_DSMOD_TRUE:EDEN_DSMOD_FALSE;
}
static EdenDsmodBool load_image(void *instance,const EdenDsmodHostApi *host,const char *key,
                               void *receiver,EdenDsmodImageSink sink) {
    (void)host;
    ModuleState *state=instance;
    if (!state || !key || !sink) return 0;
    static const char prefix[]="module:exploration:";
    if (strncmp(key,prefix,sizeof prefix-1)) return 0;
    const char *digit=key+sizeof prefix-1;
    uint64_t revision=0;
    unsigned count=0;
    for (;*digit;++digit) {
        if (*digit<'0'||*digit>'9'||++count>20) return 0;
        const unsigned value=(unsigned)(*digit-'0');
        if (revision>(UINT64_MAX-value)/10) return 0;
        revision=revision*10+value;
    }
    if (!count) return 0;
    MapView view;
    pthread_mutex_lock(&state->map_lock);
    const MapView *source=NULL;
    if (state->map.data.available) {
        if(state->pending.data.available&&revision==state->pending.revision)source=&state->pending;
        else if(state->displayed.data.available&&revision==state->displayed.revision)source=&state->displayed;
        else if(state->previous.data.available&&revision==state->previous.revision)source=&state->previous;
    }
    const int match=source!=NULL;
    if(match)view=*source;
    pthread_mutex_unlock(&state->map_lock);
    if (!match) {
        // Retired worlds must not expose their terrain. Current-world pending,
        // displayed and previous generations remain renderable after movement.
        const uint8_t clear[4]={0};
        sink(receiver,1,1,clear,sizeof clear);
        return EDEN_DSMOD_TRUE;
    }
    uint8_t *pixels=malloc(MAP_IMAGE_BYTES);
    if (!pixels) return 0;
    const int rendered=map_view_render(&view,pixels,MAP_IMAGE_BYTES);
    if (rendered) {
        pthread_mutex_lock(&state->map_lock);
        const int still_current=state->map.data.available&&state->map.data.world_id==view.data.world_id&&
            ((state->pending.data.available&&revision==state->pending.revision)||
             (state->displayed.data.available&&revision==state->displayed.revision)||
             (state->previous.data.available&&revision==state->previous.revision));
        if(still_current) {
            sink(receiver,MAP_IMAGE_WIDTH,MAP_IMAGE_HEIGHT,pixels,MAP_IMAGE_BYTES);
            if(state->pending.data.available&&revision==state->pending.revision)state->pending_loaded=1;
        } else {const uint8_t clear[4]={0};sink(receiver,1,1,clear,sizeof clear);}
        pthread_mutex_unlock(&state->map_lock);
    }
    free(pixels);
    return rendered?EDEN_DSMOD_TRUE:EDEN_DSMOD_FALSE;
}
static const EdenDsmodModuleExtensions extensions={
    .version=EDEN_DSMOD_EXT_VERSION,.struct_size=sizeof(EdenDsmodModuleExtensions),
    .abi_hash=EDEN_DSMOD_EXT_HASH,.configure=configure,.on_action=action,.load_image=load_image
};
#if defined(__GNUC__)
__attribute__((visibility("default")))
#endif
const EdenDsmodModuleExtensions *eden_dsmod_get_extensions(uint32_t version,uint64_t hash) {
    return version==EDEN_DSMOD_EXT_VERSION&&hash==EDEN_DSMOD_EXT_HASH?&extensions:NULL;
}
static const EdenDsmodModuleApi module = {
    .abi_version=EDEN_DSMOD_MODULE_ABI_VERSION, .struct_size=sizeof(EdenDsmodModuleApi),
    .abi_hash=EDEN_DSMOD_MODULE_ABI_HASH, .title_id=TITLE_ID,
    .name="Diablo III Duo 0.2.1-dev", .capabilities=REQUIRED_CAPS,
    .supports_build=supports, .create=create, .destroy=destroy, .sample=sample, .tick=tick
};
#if defined(__GNUC__)
__attribute__((visibility("default")))
#endif
const EdenDsmodModuleApi *eden_dsmod_get_module(uint32_t version, uint64_t hash) {
    return version == EDEN_DSMOD_MODULE_ABI_VERSION && hash == EDEN_DSMOD_MODULE_ABI_HASH ? &module : NULL;
}
