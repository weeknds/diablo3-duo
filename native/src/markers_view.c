// SPDX-License-Identifier: GPL-3.0-or-later
#include "markers_view.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void markers_view_clear(MarkersView *v) {
    if(v)*v=(MarkersView){.selected=-1,.selection_pending=1};
}
static int known(const Marker *m, const MapProbeResult *map) {
    if(m->kind<MARKER_QUEST||m->kind>MARKER_GOBLIN||!isfinite(m->x)||!isfinite(m->y))return 0;
    for(uint32_t i=0;i<map->tile_count;i++) {
        const MapExplorationTile *t=&map->tiles[i];
        if(!t->columns||t->columns>32||!t->rows||t->rows>32||
           !isfinite(t->min_x)||!isfinite(t->min_y)||!isfinite(t->max_x)||!isfinite(t->max_y)||
           !(t->max_x>t->min_x)||!(t->max_y>t->min_y)||
           m->x<t->min_x||m->x>=t->max_x||m->y<t->min_y||m->y>=t->max_y)continue;
        const unsigned col=(unsigned)((m->x-t->min_x)/(t->max_x-t->min_x)*t->columns);
        const unsigned row=(unsigned)((m->y-t->min_y)/(t->max_y-t->min_y)*t->rows);
        if(col<t->columns&&row<t->rows&&map_cell_visibility(t,col,row))return 1;
    }
    return 0;
}
static int same(const Marker *a,const Marker *b) {return a->id==b->id&&a->kind==b->kind;}
static int seen_goblin(MarkersView *v,uint32_t id) {
    for(uint32_t i=0;i<v->seen_count;i++)if(v->seen[i]==id)return 1;
    v->seen[v->seen_cursor]=id;
    v->seen_cursor=(v->seen_cursor+1)%MARKERS_SEEN_GOBLINS;
    if(v->seen_count<MARKERS_SEEN_GOBLINS)v->seen_count++;
    return 0;
}
void markers_view_update(MarkersView *v,const MarkersProbeResult *p,
                         const MapProbeResult *map,uint64_t tick) {
    if(!v)return;
    if(!map||!map->available||!map->shared_identity_valid||!map->exploration_available||
       map->tile_count>MAP_MAX_TILES) {markers_view_clear(v);return;}
    if(!v->has_world||v->world_id!=map->world_id) {
        markers_view_clear(v);v->has_world=1;v->world_id=map->world_id;
    }
    v->available=p&&p->available&&p->shared_identity_valid&&p->world_id==map->world_id&&
                 p->count<=MARKERS_MAX_ITEMS;
    MarkerSlot next[MARKERS_MAX_ITEMS]={0};
    uint8_t consumed[MARKERS_MAX_ITEMS]={0};
    if(v->available) {
        // Reserve surviving identities before assigning free slots. A reordered
        // game list must not move the user's selected target to another icon.
        for(unsigned i=0;i<MARKERS_MAX_ITEMS;i++)if(v->slots[i].active) {
            for(uint32_t j=0;j<p->count;j++)if(!consumed[j]&&
                same(&v->slots[i].marker,&p->items[j])&&known(&p->items[j],map)) {
                next[i]=(MarkerSlot){1,p->items[j]};consumed[j]=1;break;
            }
        }
        if(v->selected>=0&&!next[v->selected].active) {
            v->selected=-1;v->selection_pending=1;
        }
        for(uint32_t j=0;j<p->count;j++)if(!consumed[j]&&known(&p->items[j],map)) {
            int duplicate=0;
            for(unsigned i=0;i<MARKERS_MAX_ITEMS;i++)
                if(next[i].active&&same(&next[i].marker,&p->items[j]))duplicate=1;
            if(duplicate)continue;
            for(unsigned i=0;i<MARKERS_MAX_ITEMS;i++)if(!next[i].active) {
                next[i]=(MarkerSlot){1,p->items[j]};break;
            }
        }
    } else if(v->selected>=0) {v->selected=-1;v->selection_pending=1;}
    memcpy(v->slots,next,sizeof next);v->count=0;
    for(unsigned i=0;i<MARKERS_MAX_ITEMS;i++)if(next[i].active) {
        v->count++;
        if(next[i].marker.kind==MARKER_GOBLIN&&!seen_goblin(v,next[i].marker.id)) {
            v->alert_id=next[i].marker.id;
            v->alert_until=tick>UINT64_MAX-MARKERS_ALERT_TICKS?UINT64_MAX:tick+MARKERS_ALERT_TICKS;
        }
    }
    int alert_present=0;
    for(unsigned i=0;i<MARKERS_MAX_ITEMS;i++)if(next[i].active&&
        next[i].marker.kind==MARKER_GOBLIN&&next[i].marker.id==v->alert_id)alert_present=1;
    if(!alert_present)v->alert_until=0;
}
int markers_view_action(MarkersView *v,const char *action,int64_t arg) {
    if(!v||!action)return 0;
    if(!strcmp(action,"poi_select")) {
        if(arg<0||arg>=MARKERS_MAX_ITEMS||!v->slots[arg].active)return 0;
        v->selected=v->selected==(int)arg?-1:(int)arg;
    } else if(!strcmp(action,"poi_next")) {
        if(!v->count)return 0;
        for(unsigned step=1;step<=MARKERS_MAX_ITEMS;step++) {
            const unsigned i=(unsigned)(v->selected+(int)step)%MARKERS_MAX_ITEMS;
            if(v->slots[i].active){v->selected=(int)i;break;}
        }
    } else return 0;
    v->selection_pending=1;return 1;
}
const char *marker_name(MarkerKind kind) {
    static const char *names[]={"","Quest objective","Entrance / exit","Waypoint",
                               "Shrine","Pylon","Treasure goblin"};
    return kind>=MARKER_QUEST&&kind<=MARKER_GOBLIN?names[kind]:"";
}
const char *marker_icon(MarkerKind kind) {
    static const char *icons[]={"","file:assets/icon-poi-quest.png","file:assets/icon-poi-portal.png",
        "file:assets/icon-poi-waypoint.png","file:assets/icon-poi-shrine.png",
        "file:assets/icon-poi-pylon.png","file:assets/icon-poi-goblin.png"};
    return kind>=MARKER_QUEST&&kind<=MARKER_GOBLIN?icons[kind]:"";
}
int markers_view_focus(const MarkersView *v,float x,float y,char *text,size_t size,double *angle) {
    if(text&&size)text[0]=0;
    if(angle)*angle=0;
    if(!v||!text||!size||!angle||v->selected<0||v->selected>=(int)MARKERS_MAX_ITEMS||
       !v->slots[v->selected].active||!isfinite(x)||!isfinite(y))return 0;
    const Marker *m=&v->slots[v->selected].marker;
    const double dx=(double)m->x-x,dy=(double)m->y-y;
    const int near=dx*dx+dy*dy<6.25;
    double degrees=atan2(dy-dx,-dx-dy)*180.0/3.14159265358979323846;
    if(degrees<0)degrees+=360;
    static const char *directions[]={"above you","upper right from you","right of you",
        "lower right from you","below you","lower left from you","left of you","upper left from you"};
    const unsigned sector=(unsigned)floor((degrees+22.5)/45.0)%8;
    int length=snprintf(text,size,"%s  /  %s",marker_name(m->kind),near?"near you":directions[sector]);
    if(length<0||(size_t)length>=size){text[0]=0;return 0;}
    *angle=near?0:degrees;
    return near?2:1; // A coincident target has a label but no direction arrow.
}
int markers_view_alert(const MarkersView *v,uint64_t tick) {
    return v&&v->alert_until&&tick<v->alert_until;
}
