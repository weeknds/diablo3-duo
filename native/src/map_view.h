// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "map_probe.h"
#include "nav_probe.h"
#include <stddef.h>
#define MAP_IMAGE_WIDTH 2304u
#define MAP_IMAGE_HEIGHT 1160u
#define MAP_IMAGE_BYTES (MAP_IMAGE_WIDTH * MAP_IMAGE_HEIGHT * 4u)
#define MAP_COORD_SCALE 64.0f
#define MAP_WINDOW_WIDTH 360.0f

/* Presentation-only state. Pins never write to the game or survive a world change. */
typedef struct MapView {
    MapProbeResult data;
    NavProbeResult navigation;
    uint64_t revision;
    float zoom, pin_x, pin_y;
    int pinned, follow_player;
} MapView;
/* Immutable terrain framing, independent of the player and touch zoom. */
typedef struct MapProjection { float u, v, scale; } MapProjection;
int map_view_projection(const MapView *view, MapProjection *projection);
void map_view_point(const MapProjection *projection, float x, float y, float *px, float *py);
void map_view_clear(MapView *view);
void map_view_update(MapView *view, const MapProbeResult *data);
void map_view_update_navigation(MapView *view, const NavProbeResult *data);
const char *map_view_status(const MapView *view);
int map_view_action(MapView *view, const char *action);
int map_view_render(const MapView *view, uint8_t *rgba, size_t size);
