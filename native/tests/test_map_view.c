// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic exploration and coordinates. No fixture is a game observation.
#include "map_view.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__func__,__LINE__,#x); exit(1); } } while(0)

static MapProbeResult fixture(void) {
    MapProbeResult d={.available=1,.shared_identity_valid=1,.exploration_available=1,.world_id=17,.tile_count=1};
    d.tiles[0]=(MapExplorationTile){.scene_id=12,.min_x=-20,.min_y=-20,.max_x=20,.max_y=20,.columns=2,.rows=2,.cells={0x27}};
    return d;
}

static uint32_t color(const uint8_t *pixels, unsigned x, unsigned y) {
    CHECK(x<MAP_IMAGE_WIDTH && y<MAP_IMAGE_HEIGHT);
    const uint8_t *p=pixels+(y*MAP_IMAGE_WIDTH+x)*4;
    CHECK(p[3]==255);
    return (uint32_t)p[0]<<16 | (uint32_t)p[1]<<8 | p[2];
}

static unsigned count_color(const uint8_t *pixels, uint32_t rgb) {
    unsigned count=0;
    for(unsigned y=0;y<MAP_IMAGE_HEIGHT;y++) for(unsigned x=0;x<MAP_IMAGE_WIDTH;x++)
        count+=color(pixels,x,y)==rgb;
    return count;
}

static uint8_t *buffer(void) {
    uint8_t *p=malloc(MAP_IMAGE_BYTES+32);CHECK(p);
    memset(p,0xA5,16);memset(p+16,0,MAP_IMAGE_BYTES);memset(p+16+MAP_IMAGE_BYTES,0x5A,16);
    return p;
}

static void guard(const uint8_t *p) {
    for(unsigned i=0;i<16;i++){CHECK(p[i]==0xA5);CHECK(p[16+MAP_IMAGE_BYTES+i]==0x5A);}
}

static void render_mask_orientation_and_marker(void) {
    MapView v={0};MapProbeResult d=fixture();map_view_update(&v,&d);
    uint8_t *p=buffer();MapView before=v;
    CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);CHECK(!memcmp(&before,&v,sizeof v));
    /* Width=40, height=40; 480/40 is the independent fit scale, center=(576,290).
     * Column-major mask0x27 => lowerleft unseen, upperleft partial, right clear. */
    CHECK(color(p+16,456,410)==0x141410);
    CHECK(color(p+16,456,170)==0x373529);
    CHECK(color(p+16,696,410)==0x665A3C);
    CHECK(color(p+16,696,170)==0x665A3C);
    CHECK(color(p+16,576,290)==0xD7B574);
    CHECK(color(p+16,582,290)==0xF1EBDD);
    CHECK(color(p+16,0,0)==0x141410);
    free(p);
}

static void actions_and_clamps(void) {
    MapView v={0};MapProbeResult d=fixture();
    CHECK(!map_view_action(&v,"map_pin"));map_view_update(&v,&d);
    CHECK(v.zoom==1 && !v.pinned && !v.follow_player);
    const uint64_t first=v.revision;
    CHECK(!map_view_action(&v,NULL) && !map_view_action(&v,"guest_move") && !map_view_action(&v,"map_clear_pin"));
    CHECK(v.revision==first);
    for(unsigned i=0;i<30;i++)map_view_action(&v,"map_zoom_in");
    CHECK(v.zoom==8);uint64_t r=v.revision;CHECK(!map_view_action(&v,"map_zoom_in") && v.revision==r);
    for(unsigned i=0;i<30;i++)map_view_action(&v,"map_zoom_out");
    CHECK(v.zoom==0.5f);r=v.revision;CHECK(!map_view_action(&v,"map_zoom_out") && v.revision==r);
    CHECK(map_view_action(&v,"map_recenter") && v.follow_player);
    CHECK(map_view_action(&v,"map_pin") && v.pinned && v.pin_x==d.x && v.pin_y==d.y);
    CHECK(map_view_action(&v,"map_clear_pin") && !v.pinned);
    CHECK(!memcmp(&v.data,&d,sizeof d));
}

static void pin_stays_at_recorded_position_and_clears(void) {
    MapView v={0};MapProbeResult d=fixture();
    d.tiles[0]=(MapExplorationTile){.scene_id=12,.min_x=-100,.min_y=-100,.max_x=100,.max_y=100,.columns=1,.rows=1,.fully_revealed=1};
    map_view_update(&v,&d);CHECK(map_view_action(&v,"map_pin"));
    d.x=10;d.y=10;map_view_update(&v,&d);CHECK(v.pin_x==0 && v.pin_y==0);
    uint8_t *p=buffer();CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
    CHECK(color(p+16,576,270)==0xDAA759); // the pin's ring at its original position
    CHECK(color(p+16,600,266)==0xD7B574); // moved player's center
    CHECK(count_color(p+16,0xDAA759)>0);
    CHECK(map_view_action(&v,"map_recenter"));CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));
    CHECK(color(p+16,576,290)==0xD7B574);
    CHECK(map_view_action(&v,"map_clear_pin"));CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));
    CHECK(count_color(p+16,0xDAA759)==0);guard(p);free(p);
}

static void revisions_and_lifecycle(void) {
    MapView v={0};MapProbeResult d=fixture();map_view_update(&v,&d);uint64_t r=v.revision;
    map_view_update(&v,&d);CHECK(v.revision==r);
    d.reads++;d.bytes++;d.reason="another diagnostic";d.exploration_reason="another status";
    map_view_update(&v,&d);CHECK(v.revision==r);
    d.tiles[0].cells[0]=0x55;map_view_update(&v,&d);CHECK(v.revision>r);r=v.revision;
    CHECK(map_view_action(&v,"map_pin"));CHECK(map_view_action(&v,"map_zoom_in"));
    CHECK(map_view_action(&v,"map_recenter"));d.world_id++;map_view_update(&v,&d);
    CHECK(v.revision>r && !v.pinned && v.zoom==1 && !v.follow_player && v.data.world_id==d.world_id);
    CHECK(map_view_action(&v,"map_pin"));d.available=0;map_view_update(&v,&d);
    CHECK(!v.data.available && !v.pinned && !v.data.tile_count && v.zoom==1);r=v.revision;
    map_view_update(&v,&d);CHECK(v.revision==r);
    d=fixture();map_view_update(&v,&d);CHECK(v.data.available);
    d.shared_identity_valid=0;map_view_update(&v,&d);CHECK(!v.data.available && !v.data.tile_count);
}

static void unavailable_exploration_is_not_fabricated(void) {
    MapView v={0};MapProbeResult d=fixture();d.exploration_available=0;d.tile_count=0;
    map_view_update(&v,&d);uint8_t *p=buffer();CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
    CHECK(count_color(p+16,0x665A3C)==0 && count_color(p+16,0x373529)==0);
    CHECK(color(p+16,576,290)==0xD7B574);free(p);
}

static void malformed_render_rejected(void) {
    MapView v={0};MapProbeResult d=fixture();map_view_update(&v,&d);uint8_t *p=buffer();
    CHECK(!map_view_render(NULL,p+16,MAP_IMAGE_BYTES));CHECK(!map_view_render(&v,NULL,MAP_IMAGE_BYTES));
    CHECK(!map_view_render(&v,p+16,MAP_IMAGE_BYTES-1));CHECK(!map_view_render(&v,p+16,MAP_IMAGE_BYTES+1));
    for(unsigned kind=0;kind<14;kind++) {
        map_view_clear(&v);d=fixture();map_view_update(&v,&d);
        switch(kind){
            case 0:v.data.available=0;break;
            case 1:v.data.tile_count=MAP_MAX_TILES+1;break;
            case 2:v.data.x=NAN;break;
            case 3:v.data.y=INFINITY;break;
            case 4:v.zoom=NAN;break;
            case 5:v.zoom=0.49f;break;
            case 6:v.zoom=8.01f;break;
            case 7:v.data.tiles[0].min_x=NAN;break;
            case 8:v.data.tiles[0].max_y=v.data.tiles[0].min_y;break;
            case 9:v.data.tiles[0].columns=0;break;
            case 10:v.data.tiles[0].rows=33;break;
            case 11:v.data.shared_identity_valid=0;break;
            case 12:v.pinned=1;v.pin_x=NAN;break;
            case 13:v.pinned=1;v.pin_y=INFINITY;break;
        }
        CHECK(!map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
    }
    map_view_clear(&v);d=fixture();map_view_update(&v,&d);v.data.shared_identity_valid=0;
    CHECK(!map_view_action(&v,"map_pin"));free(p);
}

static NavProbeResult navigation_fixture(void) {
    NavProbeResult n={.available=1,.shared_identity_valid=1,.world_id=17,.grid_count=1,.cell_count=6};
    /* A 3x2 L shape rotated 90 degrees. The scene ID intentionally differs from
     * the exploration tile: only common world coordinates establish overlap. */
    n.grids[0]=(NavTerrainGrid){.scene_id=987,.columns=3,.rows=2,
        .origin_x=10,.origin_y=5,.axis_x_x=0,.axis_x_y=2.5f,.axis_y_x=-2.5f,.axis_y_y=0};
    n.ground[0]=0x23; // (0,0), (1,0), (2,1); all other cells blocked.
    return n;
}

static void navigation_shape_rotation_and_fog(void) {
    MapView v={0};MapProbeResult d=fixture();NavProbeResult n=navigation_fixture();
    map_view_update(&v,&d);map_view_update_navigation(&v,&n);uint8_t *p=buffer();
    MapView before=v;CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
    CHECK(!memcmp(&before,&v,sizeof v));
    /* At the established 12 pixels/world-unit projection, cell centers are
     * world(8.75,6.25),(8.75,8.75),(6.25,11.25), independent of the renderer. */
    CHECK(color(p+16,681,215)==0x706044);
    CHECK(color(p+16,681,185)==0x706044);
    CHECK(color(p+16,651,155)==0x706044);
    CHECK(color(p+16,681,155)==0x242217); // blocked (2,0)
    CHECK(color(p+16,651,215)==0x242217); // blocked (0,1)
    CHECK(color(p+16,651,185)==0x242217); // blocked (1,1)
    CHECK(color(p+16,711,215)==0x242217); // outside the rotated grid

    /* One 2.5-unit ground cell straddles X=0 in the lower half. Its left part
     * exists in the nav data but is wholly unseen in the native fog fixture. */
    n.grid_count=2;n.cell_count=7;
    n.grids[1]=(NavTerrainGrid){.scene_id=988,.columns=1,.rows=1,.cell_offset=6,
        .origin_x=-1.25f,.origin_y=-15,.axis_x_x=2.5f,.axis_y_y=2.5f};
    n.ground[0]|=0x40;
    map_view_update_navigation(&v,&n);CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
    for(unsigned y=441;y<=468;y++)for(unsigned x=562;x<576;x++)
        CHECK(color(p+16,x,y)==0x141410);
    CHECK(color(p+16,581,455)==0x706044);

    /* Partial exploration must keep its darker shade even for known ground. */
    n.grid_count=3;n.cell_count=8;
    n.grids[2]=(NavTerrainGrid){.scene_id=989,.columns=1,.rows=1,.cell_offset=7,
        .origin_x=-10,.origin_y=10,.axis_x_x=2.5f,.axis_y_y=2.5f};
    n.ground[0]|=0x80;
    map_view_update_navigation(&v,&n);CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));
    CHECK(color(p+16,471,155)==0x443B2B);guard(p);

    d.exploration_available=0;d.tile_count=0;map_view_update(&v,&d);
    CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
    CHECK(count_color(p+16,0x706044)==0&&count_color(p+16,0x443B2B)==0);
    CHECK(color(p+16,681,215)==0x141410);free(p);
}

static void navigation_revisions_and_lifecycle(void) {
    MapView v={0};MapProbeResult d=fixture();NavProbeResult n=navigation_fixture();
    map_view_update(&v,&d);const uint64_t before=v.revision;
    map_view_update_navigation(&v,&n);CHECK(v.revision>before&&v.navigation.available);
    uint64_t r=v.revision;
    n.reads=400;n.bytes=4096;n.reason="new telemetry only";
    map_view_update_navigation(&v,&n);CHECK(v.revision==r);
    CHECK(v.navigation.reads==400&&v.navigation.bytes==4096);
    n.ground[0]^=1;map_view_update_navigation(&v,&n);CHECK(v.revision>r);r=v.revision;
    n.grids[0].origin_x++;map_view_update_navigation(&v,&n);CHECK(v.revision>r);r=v.revision;
    n.world_id++;map_view_update_navigation(&v,&n);
    CHECK(v.revision>r&&!v.navigation.available&&!v.navigation.grid_count&&!v.navigation.cell_count);
    for(unsigned i=0;i<NAV_CELL_BYTES;i++)CHECK(v.navigation.ground[i]==0);
    r=v.revision;map_view_update_navigation(&v,&n);CHECK(v.revision==r);

    n=navigation_fixture();map_view_update_navigation(&v,&n);CHECK(v.navigation.available);
    d.world_id++;map_view_update(&v,&d);CHECK(!v.navigation.available&&!v.navigation.grid_count);
    n.world_id=d.world_id;map_view_update_navigation(&v,&n);CHECK(v.navigation.available);
    n.shared_identity_valid=0;map_view_update_navigation(&v,&n);CHECK(!v.navigation.available);
    n.shared_identity_valid=1;map_view_update_navigation(&v,&n);CHECK(v.navigation.available);
    d.shared_identity_valid=0;map_view_update(&v,&d);CHECK(!v.navigation.available&&!v.data.available);
}

static void malformed_navigation_stays_bounded(void) {
    uint8_t *p=buffer();
    for(unsigned kind=0;kind<18;kind++) {
        MapView v={0};MapProbeResult d=fixture();NavProbeResult n=navigation_fixture();
        map_view_update(&v,&d);map_view_update_navigation(&v,&n);
        /* Deliberately inject malformed publication state to exercise the
         * renderer's final bounds check, even if a producer rejected it first. */
        switch(kind) {
            case 0:v.navigation.grid_count=NAV_MAX_GRIDS+1;break;
            case 1:v.navigation.cell_count=NAV_MAX_CELLS+1;break;
            case 2:v.navigation.grids[0].columns=0;break;
            case 3:v.navigation.grids[0].rows=513;break;
            case 4:v.navigation.grids[0].origin_x=NAN;break;
            case 5:v.navigation.grids[0].origin_y=INFINITY;break;
            case 6:v.navigation.grids[0].axis_x_x=NAN;break;
            case 7:v.navigation.grids[0].axis_y_y=INFINITY;break;
            case 8:v.navigation.grids[0].axis_x_y=0;break;
            case 9:v.navigation.grids[0].axis_y_x=FLT_MAX;break;
            case 10:v.navigation.grids[0].axis_x_y=1.0e-30f;v.navigation.grids[0].axis_y_x=-1.0e30f;break;
            case 11:v.navigation.grids[0].cell_offset=UINT32_MAX;break;
            case 12:v.navigation.grids[0].cell_offset=NAV_MAX_CELLS;break;
            case 13:v.navigation.cell_count=1;break; // full grid is truncated, so no cell may be drawn
            case 14:v.navigation.world_id++;break;
            case 15:v.navigation.shared_identity_valid=0;break;
            case 16:v.navigation.available=0;break;
            case 17:v.navigation.grids[0].origin_x=FLT_MAX;break;
        }
        CHECK(map_view_render(&v,p+16,MAP_IMAGE_BYTES));guard(p);
        CHECK(count_color(p+16,0x706044)==0&&count_color(p+16,0x443B2B)==0);
        CHECK(count_color(p+16,0xB29B6C)==0&&count_color(p+16,0x655839)==0);
    }
    free(p);
}

static void partial_terrain_warning_survives_pin(void) {
    MapView v={.data={.available=1,.exploration_available=1},
               .navigation={.available=1,.partial=1}};
    CHECK(strstr(map_view_status(&v),"terrain is unavailable"));
    v.pinned=1;
    CHECK(strstr(map_view_status(&v),"terrain is unavailable"));
    CHECK(strstr(map_view_status(&v),"pinned"));
    v.data.available=0;
    CHECK(!strstr(map_view_status(&v),"pinned"));
}

int main(void) {
    render_mask_orientation_and_marker();actions_and_clamps();pin_stays_at_recorded_position_and_clears();
    revisions_and_lifecycle();unavailable_exploration_is_not_fabricated();malformed_render_rejected();
    navigation_shape_rotation_and_fog();navigation_revisions_and_lifecycle();malformed_navigation_stays_bounded();
    partial_terrain_warning_survives_pin();
    puts("PASS map-view fog, terrain rotation, blocked cells, bounds, markers, pins, revisions and lifecycle suite");return 0;
}
