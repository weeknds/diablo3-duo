// SPDX-License-Identifier: GPL-3.0-or-later
#include "markers_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static MapProbeResult terrain(void) {
    MapProbeResult m={.available=1,.shared_identity_valid=1,.exploration_available=1,
                      .world_id=77,.tile_count=1};
    m.tiles[0]=(MapExplorationTile){.min_x=-100,.min_y=-100,.max_x=100,.max_y=100,
                                  .columns=2,.rows=2,.cells={0x80}};
    return m;
}
static MarkersProbeResult points(void) {
    return (MarkersProbeResult){.available=1,.shared_identity_valid=1,.world_id=77,
        .count=3,.items={{10,MARKER_QUEST,-80,-80},{20,MARKER_WAYPOINT,-20,-20},
                        {30,MARKER_GOBLIN,-30,-30}}};
}
int main(void) {
    MarkersView v;markers_view_clear(&v);
    MapProbeResult m=terrain();MarkersProbeResult p=points();
    markers_view_update(&v,&p,&m,100);
    assert(v.count==3&&v.selected==-1&&markers_view_alert(&v,100));
    assert(markers_view_action(&v,"poi_select",1)&&v.selected==1);
    const Marker tmp=p.items[0];p.items[0]=p.items[1];p.items[1]=tmp;
    markers_view_update(&v,&p,&m,130);
    assert(v.selected==1&&v.slots[1].marker.id==20); // Selection survives list reorder.
    assert(markers_view_alert(&v,459)&&!markers_view_alert(&v,460));
    markers_view_update(&v,&p,&m,500);
    assert(!markers_view_alert(&v,500)); // Same goblin never alerts every scan.
    p.items[0].id=21;
    markers_view_update(&v,&p,&m,510);
    assert(v.selected==-1&&v.selection_pending); // Reused slot is not the old target.
    assert(markers_view_action(&v,"poi_next",0)&&v.selected==0);
    assert(markers_view_action(&v,"poi_next",0)&&v.selected==1);
    assert(markers_view_action(&v,"poi_select",1)&&v.selected==-1);
    assert(!markers_view_action(&v,"poi_select",64));
    assert(!markers_view_action(&v,"poi_select",-1));

    p.items[0].x=30; // Hidden cell.
    p.items[1].x=NAN;
    p.items[2].kind=MARKER_NONE;
    markers_view_update(&v,&p,&m,520);
    assert(v.count==0&&!markers_view_alert(&v,520));
    p=points();p.items[1]=p.items[0]; // Duplicate identity is shown once.
    markers_view_update(&v,&p,&m,530);assert(v.count==2);
    p.available=0;markers_view_update(&v,&p,&m,540);
    assert(!v.count&&!markers_view_alert(&v,540));
    p=points();markers_view_update(&v,&p,&m,550);
    assert(v.count==3&&!markers_view_alert(&v,550)); // A read gap is not a new goblin.
    p.world_id=88;markers_view_update(&v,&p,&m,560);assert(v.count==0);
    m.world_id=88;markers_view_update(&v,&p,&m,570);
    assert(v.count==3&&markers_view_alert(&v,570));

    char label[128];double angle=99;
    markers_view_action(&v,"poi_select",0);
    assert(markers_view_focus(&v,-70,-70,label,sizeof label,&angle));
    assert(strstr(label,"Quest objective")&&strstr(label,"above you")&&fabs(angle)<0.001);
    v.slots[0].marker.x=-60;v.slots[0].marker.y=-60;
    assert(markers_view_focus(&v,-70,-70,label,sizeof label,&angle)&&fabs(angle-180)<0.001);
    assert(strstr(label,"below you"));
    v.slots[0].marker.x=-80;v.slots[0].marker.y=-60;
    assert(markers_view_focus(&v,-70,-70,label,sizeof label,&angle)&&fabs(angle-90)<0.001);
    v.slots[0].marker.x=-70;v.slots[0].marker.y=-70;
    assert(markers_view_focus(&v,-70,-70,label,sizeof label,&angle)&&strstr(label,"near you"));
    assert(!markers_view_focus(&v,NAN,-70,label,sizeof label,&angle));
    assert(!markers_view_focus(&v,-70,-70,label,2,&angle));
    m.exploration_available=0;markers_view_update(&v,&p,&m,580);
    assert(!v.count&&v.selected==-1&&!markers_view_alert(&v,580));
    assert(!markers_view_action(&v,"poi_next",0));
    puts("PASS marker visibility, stable selection, directions and goblin alerts");
}
