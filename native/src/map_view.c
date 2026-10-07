// SPDX-License-Identifier: GPL-3.0-or-later
#include "map_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void map_view_clear(MapView *v) {
    const uint64_t next = v->revision + 1;
    *v = (MapView){.revision = next, .zoom = 1.0f};
}

void map_view_update(MapView *v, const MapProbeResult *data) {
    if (!data->available || !data->shared_identity_valid) {
        if (v->data.available || v->pinned) map_view_clear(v);
        return;
    }
    if (!v->data.available || v->data.world_id != data->world_id) map_view_clear(v);
    /* Counters and diagnostic pointers are not image content. */
    int changed = v->data.x != data->x || v->data.y != data->y ||
        v->data.exploration_available != data->exploration_available ||
        v->data.tile_count != data->tile_count ||
        memcmp(v->data.tiles, data->tiles, sizeof data->tiles);
    v->data = *data;
    if (changed) ++v->revision;
}

void map_view_update_navigation(MapView *v, const NavProbeResult *data) {
    NavProbeResult empty={0};
    if (!v->data.available || !data->available || !data->shared_identity_valid ||
        data->world_id!=v->data.world_id) data=&empty;
    const NavProbeResult *old=&v->navigation;
    const int changed=old->available!=data->available || old->world_id!=data->world_id ||
        old->grid_count!=data->grid_count || old->cell_count!=data->cell_count ||
        memcmp(old->grids,data->grids,sizeof old->grids) ||
        memcmp(old->ground,data->ground,sizeof old->ground);
    v->navigation=*data;
    if (changed) ++v->revision;
}

const char *map_view_status(const MapView *v) {
    if (!v->data.available) return "Exploration data unavailable. Start a game or wait for loading.";
    const int terrain_available=v->navigation.available&&v->data.exploration_available;
    if (terrain_available&&v->navigation.partial) return v->pinned?
        "Location pinned. Some terrain is unavailable in this area.":
        "Some terrain is unavailable. Doors and moving objects are not shown.";
    if (v->pinned) return "Location pinned for this area. Leaving the world clears the pin.";
    return terrain_available?"Terrain navigation. Pylons, exits and rift timing are not shown.":
        "Exploration coverage. Room walls, pylons and rift timing are not shown.";
}

int map_view_action(MapView *v, const char *action) {
    if (!v->data.available || !v->data.shared_identity_valid || !action) return 0;
    if (!strcmp(action,"map_zoom_in")) {
        if (v->zoom >= 8.0f) return 0;
        v->zoom = fminf(8.0f,v->zoom*1.5f);
    } else if (!strcmp(action,"map_zoom_out")) {
        if (v->zoom <= 0.5f) return 0;
        v->zoom = fmaxf(0.5f,v->zoom/1.5f);
    } else if (!strcmp(action,"map_recenter")) {
        v->follow_player = 1;
    } else if (!strcmp(action,"map_pin")) {
        v->pinned = 1; v->pin_x = v->data.x; v->pin_y = v->data.y;
    } else if (!strcmp(action,"map_clear_pin")) {
        if (!v->pinned) return 0;
        v->pinned = 0;
    } else return 0;
    ++v->revision;
    return 1;
}

static void pixel(uint8_t *p, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= (int)MAP_IMAGE_WIDTH || y >= (int)MAP_IMAGE_HEIGHT) return;
    uint8_t *at = p + ((size_t)y * MAP_IMAGE_WIDTH + (unsigned)x) * 4u;
    at[0] = (uint8_t)(color >> 16); at[1] = (uint8_t)(color >> 8);
    at[2] = (uint8_t)color; at[3] = 255;
}

static void box(uint8_t *p, int x0, int y0, int x1, int y1, uint32_t color) {
    if (x0 < 1) x0 = 1;
    if (y0 < 1) y0 = 1;
    if (x1 >= (int)MAP_IMAGE_WIDTH) x1 = (int)MAP_IMAGE_WIDTH-1;
    if (y1 >= (int)MAP_IMAGE_HEIGHT) y1 = (int)MAP_IMAGE_HEIGHT-1;
    for (int y=y0; y<y1; ++y) for (int x=x0; x<x1; ++x) pixel(p,x,y,color);
}

static void fog_box(uint8_t *mask,int x0,int y0,int x1,int y1,unsigned shade) {
    if (!mask) return;
    if (x0<1) x0=1;
    if (y0<1) y0=1;
    if (x1>=(int)MAP_IMAGE_WIDTH) x1=(int)MAP_IMAGE_WIDTH-1;
    if (y1>=(int)MAP_IMAGE_HEIGHT) y1=(int)MAP_IMAGE_HEIGHT-1;
    for (int y=y0;y<y1;++y) for (int x=x0;x<x1;++x) {
        uint8_t *at=mask+(size_t)y*MAP_IMAGE_WIDTH+(unsigned)x;
        if (*at<shade) *at=(uint8_t)shade;
    }
}

static void circle(uint8_t *p, int x, int y, int radius, uint32_t color) {
    for (int dy=-radius; dy<=radius; ++dy) for (int dx=-radius; dx<=radius; ++dx)
        if (dx*dx+dy*dy <= radius*radius) pixel(p,x+dx,y+dy,color);
}

typedef struct Projection { float x, y, scale; } Projection;
static int sx(const Projection *p, float x) {
    return (int)fmaxf(-100000.0f,fminf(100000.0f,MAP_IMAGE_WIDTH/2.0f+(x-p->x)*p->scale));
}
static int sy(const Projection *p, float y) {
    return (int)fmaxf(-100000.0f,fminf(100000.0f,MAP_IMAGE_HEIGHT/2.0f-(y-p->y)*p->scale));
}

static void terrain(uint8_t *pixels,const uint8_t *fog,const Projection *p,
                    const NavProbeResult *nav) {
    for (unsigned n=0;n<nav->grid_count;++n) {
        const NavTerrainGrid *g=&nav->grids[n];
        const float det=g->axis_x_x*g->axis_y_y-g->axis_x_y*g->axis_y_x;
        if (!g->columns || !g->rows || g->columns>512 || g->rows>512 ||
            g->cell_offset>nav->cell_count ||
            (uint32_t)g->columns*g->rows>nav->cell_count-g->cell_offset ||
            !isfinite(g->origin_x)||!isfinite(g->origin_y)||
            fabsf(g->origin_x)>1000000||fabsf(g->origin_y)>1000000||
            !isfinite(g->axis_x_x)||!isfinite(g->axis_x_y)||
            !isfinite(g->axis_y_x)||!isfinite(g->axis_y_y)||
            fabsf(g->axis_x_x)>2.51f||fabsf(g->axis_x_y)>2.51f||
            fabsf(g->axis_y_x)>2.51f||fabsf(g->axis_y_y)>2.51f||
            !isfinite(det)||fabsf(det)<0.001f) continue;
        int x0=(int)MAP_IMAGE_WIDTH,y0=(int)MAP_IMAGE_HEIGHT,x1=0,y1=0;
        for (unsigned corner=0;corner<4;++corner) {
            const float c=(corner&1)?g->columns:0, r=(corner&2)?g->rows:0;
            const int x=sx(p,g->origin_x+c*g->axis_x_x+r*g->axis_y_x);
            const int y=sy(p,g->origin_y+c*g->axis_x_y+r*g->axis_y_y);
            if(x<x0)x0=x;
            if(x>x1)x1=x;
            if(y<y0)y0=y;
            if(y>y1)y1=y;
        }
        if(x0<1)x0=1;
        if(y0<1)y0=1;
        if(x1>=(int)MAP_IMAGE_WIDTH)x1=(int)MAP_IMAGE_WIDTH-1;
        if(y1>=(int)MAP_IMAGE_HEIGHT)y1=(int)MAP_IMAGE_HEIGHT-1;
        const float edge=fminf(0.32f,0.8f/(2.5f*p->scale));
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            const unsigned shade=fog[(size_t)y*MAP_IMAGE_WIDTH+(unsigned)x];
            if(!shade)continue;
            const float dx=p->x+(x+0.5f-MAP_IMAGE_WIDTH/2.0f)/p->scale-g->origin_x;
            const float dy=p->y-(y+0.5f-MAP_IMAGE_HEIGHT/2.0f)/p->scale-g->origin_y;
            const float column=(dx*g->axis_y_y-dy*g->axis_y_x)/det;
            const float row=(dy*g->axis_x_x-dx*g->axis_x_y)/det;
            if(!isfinite(column)||!isfinite(row)||column<0||row<0||
                column>=g->columns||row>=g->rows)continue;
            const unsigned c=(unsigned)column,r=(unsigned)row;
            if(!nav_cell_ground(nav,n,c,r))continue;
            const float cx=column-c,ry=row-r;
            const int border=(cx<edge&&(c==0||!nav_cell_ground(nav,n,c-1,r)))||
                (cx>1-edge&&!nav_cell_ground(nav,n,c+1,r))||
                (ry<edge&&(r==0||!nav_cell_ground(nav,n,c,r-1)))||
                (ry>1-edge&&!nav_cell_ground(nav,n,c,r+1));
            pixel(pixels,x,y,border?(shade==2?0xB29B6C:0x655839):(shade==2?0x706044:0x443B2B));
        }
    }
}

int map_view_render(const MapView *v, uint8_t *rgba, size_t size) {
    if (!v || !rgba || size != MAP_IMAGE_BYTES || !v->data.available || !v->data.shared_identity_valid ||
        v->data.tile_count > MAP_MAX_TILES || !isfinite(v->data.x) || !isfinite(v->data.y) ||
        !isfinite(v->zoom) || v->zoom < 0.5f || v->zoom > 8.0f ||
        (v->pinned && (!isfinite(v->pin_x) || !isfinite(v->pin_y)))) return 0;
    const MapProbeResult *d = &v->data;
    float min_x=d->x-20, max_x=d->x+20, min_y=d->y-20, max_y=d->y+20;
    if (d->exploration_available) for (unsigned i=0; i<d->tile_count; ++i) {
        const MapExplorationTile *t=&d->tiles[i];
        if (!isfinite(t->min_x)||!isfinite(t->min_y)||!isfinite(t->max_x)||!isfinite(t->max_y)||
            t->max_x<=t->min_x||t->max_y<=t->min_y) return 0;
        min_x=fminf(min_x,t->min_x); max_x=fmaxf(max_x,t->max_x);
        min_y=fminf(min_y,t->min_y); max_y=fmaxf(max_y,t->max_y);
    }
    Projection p={(min_x+max_x)/2,(min_y+max_y)/2,
                  fminf((MAP_IMAGE_WIDTH-120)/(max_x-min_x),(MAP_IMAGE_HEIGHT-100)/(max_y-min_y))*v->zoom};
    if (v->follow_player) { p.x=d->x; p.y=d->y; }
    if (!isfinite(p.scale) || p.scale<=0) return 0;
    const NavProbeResult *nav=&v->navigation;
    const int has_terrain=nav->available&&nav->shared_identity_valid&&nav->world_id==d->world_id&&
        nav->grid_count<=NAV_MAX_GRIDS&&nav->cell_count<=NAV_MAX_CELLS&&d->exploration_available;
    uint8_t *fog=has_terrain?calloc(MAP_IMAGE_WIDTH*MAP_IMAGE_HEIGHT,1):NULL;
    for (unsigned y=0;y<MAP_IMAGE_HEIGHT;y++) for(unsigned x=0;x<MAP_IMAGE_WIDTH;x++)
        pixel(rgba,(int)x,(int)y,0x141410);
    if (d->exploration_available) for (unsigned i=0; i<d->tile_count; ++i) {
        const MapExplorationTile *t=&d->tiles[i];
        if (!t->columns||!t->rows||t->columns>32||t->rows>32) { free(fog);return 0; }
        const float w=(t->max_x-t->min_x)/t->columns, h=(t->max_y-t->min_y)/t->rows;
        for (unsigned c=0;c<t->columns;c++) for(unsigned r=0;r<t->rows;r++) {
            const unsigned visibility=map_cell_visibility(t,c,r);
            if (!visibility) continue;
            const int x0=sx(&p,t->min_x+c*w),x1=sx(&p,t->min_x+(c+1)*w)+1;
            const int y0=sy(&p,t->min_y+(r+1)*h),y1=sy(&p,t->min_y+r*h)+1;
            box(rgba,x0,y0,x1,y1,fog?(visibility==2?0x242217:0x1C1B15):
                                                    (visibility==2?0x665A3C:0x373529));
            fog_box(fog,x0,y0,x1,y1,visibility);
        }
    }
    if(fog)terrain(rgba,fog,&p,nav);
    free(fog);
    if (v->pinned) {
        const int x=sx(&p,v->pin_x), y=sy(&p,v->pin_y);
        circle(rgba,x,y-12,13,0x141410);circle(rgba,x,y-12,10,0xDAA759);
        for (int row=0;row<12;row++) box(rgba,x-6+row/2,y-5+row,x+7-row/2,y-4+row,0xDAA759);
        circle(rgba,x,y-12,3,0x141410);
    }
    const int px=sx(&p,d->x),py=sy(&p,d->y);
    circle(rgba,px,py,12,0x141410);circle(rgba,px,py,8,0xF1EBDD);
    circle(rgba,px,py,3,0xD7B574);
    return 1;
}
