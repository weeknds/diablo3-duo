// SPDX-License-Identifier: GPL-3.0-or-later
#include "map_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void map_view_clear(MapView *v) {
    const uint64_t next = v->revision + 1;
    *v = (MapView){.revision = next, .zoom = 1.0f, .follow_player = 1};
}

void map_view_update(MapView *v, const MapProbeResult *data) {
    if (!data->available || !data->shared_identity_valid) {
        if (v->data.available || v->pinned) map_view_clear(v);
        return;
    }
    if (!v->data.available || v->data.world_id != data->world_id) map_view_clear(v);
    /* Counters and diagnostic pointers are not image content. */
    int changed = v->data.exploration_available != data->exploration_available ||
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
    return 1;
}

/* The ordinary game minimap rotates world XY by -135 degrees and flips
 * screen Y. Both axes retain the same scale (no isometric squash).
 * Executable projection: 0x3EBD3C..0x3EC05C; marker: 0x3E9D6C. */
static void project(float x,float y,float *u,float *v) {
    *u=(y-x)*0.7071067812f; *v=(x+y)*0.7071067812f;
}
static void unproject(const MapProjection *p,float px,float py,float *x,float *y) {
    const float u=p->u+(px-MAP_IMAGE_WIDTH/2.0f)/p->scale;
    const float v=p->v+(py-MAP_IMAGE_HEIGHT/2.0f)/p->scale;
    *x=(v-u)*0.7071067812f; *y=(u+v)*0.7071067812f;
}
void map_view_point(const MapProjection *p,float x,float y,float *px,float *py) {
    float u,v; project(x,y,&u,&v);
    *px=MAP_IMAGE_WIDTH/2.0f+(u-p->u)*p->scale;
    *py=MAP_IMAGE_HEIGHT/2.0f+(v-p->v)*p->scale;
}
int map_view_projection(const MapView *view,MapProjection *p) {
    if (!view||!p||!view->data.available||!view->data.shared_identity_valid||
        view->data.tile_count>MAP_MAX_TILES||!isfinite(view->data.x)||!isfinite(view->data.y)) return 0;
    const MapProbeResult *d=&view->data;
    float min_u=INFINITY,min_v=INFINITY,max_u=-INFINITY,max_v=-INFINITY;
    if (d->exploration_available) for (unsigned i=0;i<d->tile_count;++i) {
        const MapExplorationTile *t=&d->tiles[i];
        if (!isfinite(t->min_x)||!isfinite(t->min_y)||!isfinite(t->max_x)||!isfinite(t->max_y)||
            fabsf(t->min_x)>1000000||fabsf(t->min_y)>1000000||
            fabsf(t->max_x)>1000000||fabsf(t->max_y)>1000000||
            t->max_x<=t->min_x||t->max_y<=t->min_y||
            !t->columns||!t->rows||t->columns>32||t->rows>32) return 0;
        for(unsigned corner=0;corner<4;++corner) {
            float u,v;project(corner&1?t->max_x:t->min_x,corner&2?t->max_y:t->min_y,&u,&v);
            min_u=fminf(min_u,u);max_u=fmaxf(max_u,u);
            min_v=fminf(min_v,v);max_v=fmaxf(max_v,v);
        }
    }
    if (!isfinite(min_u)) {
        float u,v; project(d->x,d->y,&u,&v);
        min_u=u-90;max_u=u+90;min_v=v-45;max_v=v+45;
    }
    p->u=(min_u+max_u)*0.5f; p->v=(min_v+max_v)*0.5f;
    p->scale=fminf(MAP_IMAGE_WIDTH/(max_u-min_u+40),MAP_IMAGE_HEIGHT/(max_v-min_v+40));
    return isfinite(p->scale)&&p->scale>0;
}
static void pixel(uint8_t *p,size_t i,uint32_t color) {
    p+=i*4; p[0]=(uint8_t)(color>>16);p[1]=(uint8_t)(color>>8);p[2]=(uint8_t)color;p[3]=255;
}
/* All four corners matter after rotation. Clamp before converting to integers. */
static void bounds(const MapProjection *p,const float x[4],const float y[4],int out[4]) {
    float x0=MAP_IMAGE_WIDTH,y0=MAP_IMAGE_HEIGHT,x1=0,y1=0;
    for(unsigned i=0;i<4;++i) {
        float px,py;map_view_point(p,x[i],y[i],&px,&py);
        x0=fminf(x0,px);x1=fmaxf(x1,px);y0=fminf(y0,py);y1=fmaxf(y1,py);
    }
    out[0]=(int)fmaxf(0,fminf(MAP_IMAGE_WIDTH-1,floorf(x0)));
    out[1]=(int)fmaxf(0,fminf(MAP_IMAGE_HEIGHT-1,floorf(y0)));
    out[2]=(int)fmaxf(0,fminf(MAP_IMAGE_WIDTH-1,ceilf(x1)));
    out[3]=(int)fmaxf(0,fminf(MAP_IMAGE_HEIGHT-1,ceilf(y1)));
}
static void reveal(uint8_t *fog,const MapProbeResult *d,const MapProjection *p) {
    if (!d->exploration_available) return;
    for(unsigned i=0;i<d->tile_count;++i) {
        const MapExplorationTile *t=&d->tiles[i];
        const float xs[4]={t->min_x,t->max_x,t->min_x,t->max_x};
        const float ys[4]={t->min_y,t->min_y,t->max_y,t->max_y};
        int b[4];bounds(p,xs,ys,b);
        const float cw=t->columns/(t->max_x-t->min_x),rh=t->rows/(t->max_y-t->min_y);
        for(int y=b[1];y<=b[3];++y) for(int x=b[0];x<=b[2];++x) {
            float wx,wy;unproject(p,x+0.5f,y+0.5f,&wx,&wy);
            if(wx<t->min_x||wx>=t->max_x||wy<t->min_y||wy>=t->max_y)continue;
            const unsigned shade=map_cell_visibility(t,(unsigned)((wx-t->min_x)*cw),
                                                       (unsigned)((wy-t->min_y)*rh));
            uint8_t *at=&fog[(size_t)y*MAP_IMAGE_WIDTH+(unsigned)x];
            if(*at<shade)*at=(uint8_t)shade;
        }
    }
}
static void terrain(uint8_t *ground,const uint8_t *fog,const MapProjection *p,const NavProbeResult *nav) {
    for(unsigned n=0;n<nav->grid_count;++n) {
        const NavTerrainGrid *g=&nav->grids[n];
        const float det=g->axis_x_x*g->axis_y_y-g->axis_x_y*g->axis_y_x;
        if(!g->columns||!g->rows||g->columns>512||g->rows>512||
           g->cell_offset>nav->cell_count||(uint32_t)g->columns*g->rows>nav->cell_count-g->cell_offset||
           !isfinite(g->origin_x)||!isfinite(g->origin_y)||fabsf(g->origin_x)>1000000||fabsf(g->origin_y)>1000000||
           !isfinite(g->axis_x_x)||!isfinite(g->axis_x_y)||!isfinite(g->axis_y_x)||!isfinite(g->axis_y_y)||
           fabsf(g->axis_x_x)>2.51f||fabsf(g->axis_x_y)>2.51f||fabsf(g->axis_y_x)>2.51f||fabsf(g->axis_y_y)>2.51f||
           !isfinite(det)||fabsf(det)<0.001f)continue;
        float xs[4],ys[4];
        for(unsigned corner=0;corner<4;++corner) {
            const float c=corner&1?g->columns:0,r=corner&2?g->rows:0;
            xs[corner]=g->origin_x+c*g->axis_x_x+r*g->axis_y_x;
            ys[corner]=g->origin_y+c*g->axis_x_y+r*g->axis_y_y;
        }
        int b[4];bounds(p,xs,ys,b);
        for(int y=b[1];y<=b[3];++y) for(int x=b[0];x<=b[2];++x) {
            const size_t at=(size_t)y*MAP_IMAGE_WIDTH+(unsigned)x;
            if(!fog[at])continue;
            float wx,wy;unproject(p,x+0.5f,y+0.5f,&wx,&wy);
            const float dx=wx-g->origin_x,dy=wy-g->origin_y;
            const float column=(dx*g->axis_y_y-dy*g->axis_y_x)/det;
            const float row=(dy*g->axis_x_x-dx*g->axis_x_y)/det;
            if(!isfinite(column)||!isfinite(row)||column<0||row<0||column>=g->columns||row>=g->rows)continue;
            if(nav_cell_ground(nav,n,(unsigned)column,(unsigned)row))ground[at]=fog[at];
        }
    }
}
int map_view_render(const MapView *v,uint8_t *rgba,size_t size) {
    MapProjection p;
    if(!rgba||size!=MAP_IMAGE_BYTES||!map_view_projection(v,&p)||
       !isfinite(v->zoom)||v->zoom<0.5f||v->zoom>8||
       (v->pinned&&(!isfinite(v->pin_x)||!isfinite(v->pin_y))))return 0;
    const size_t count=MAP_IMAGE_WIDTH*MAP_IMAGE_HEIGHT;
    uint8_t *fog=calloc(count,1),*ground=calloc(count,1);
    if(!fog||!ground){free(fog);free(ground);return 0;}
    reveal(fog,&v->data,&p);
    const NavProbeResult *n=&v->navigation;
    const int has_terrain=n->available&&n->shared_identity_valid&&n->world_id==v->data.world_id&&
        n->grid_count<=NAV_MAX_GRIDS&&n->cell_count<=NAV_MAX_CELLS&&v->data.exploration_available;
    if(has_terrain)terrain(ground,fog,&p,n);
    for(unsigned y=0;y<MAP_IMAGE_HEIGHT;++y)for(unsigned x=0;x<MAP_IMAGE_WIDTH;++x) {
        const size_t at=(size_t)y*MAP_IMAGE_WIDTH+x;
        uint32_t ink=0x141410;
        if(has_terrain&&ground[at]) {
            const unsigned s=ground[at];
            const int edge=x<1||y<1||x+1>=MAP_IMAGE_WIDTH||y+1>=MAP_IMAGE_HEIGHT||
                !ground[at-1]||!ground[at+1]||!ground[at-MAP_IMAGE_WIDTH]||!ground[at+MAP_IMAGE_WIDTH];
            ink=edge?(s==2?0xAD9566:0x655A42):(s==2?0x24221C:0x1C1B16);
        } else if(!has_terrain&&fog[at]) ink=fog[at]==2?0x24221C:0x1C1B16;
        pixel(rgba,at,ink);
    }
    free(fog);free(ground);
    /* Player, pin and viewport are native map transforms, never baked here. */
    return 1;
}
