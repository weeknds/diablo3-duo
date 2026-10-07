// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "map_probe.h"
#include "nav_probe.h"
#include <stddef.h>
#define MAP_IMAGE_WIDTH 1152u
#define MAP_IMAGE_HEIGHT 580u
#define MAP_IMAGE_BYTES (MAP_IMAGE_WIDTH * MAP_IMAGE_HEIGHT * 4u)

/* Presentation-only state. Pins never write to the game or survive a world change. */
typedef struct MapView {
    MapProbeResult data;
    NavProbeResult navigation;
    uint64_t revision;
    float zoom, pin_x, pin_y;
    int pinned, follow_player;
} MapView;
void map_view_clear(MapView *view);
void map_view_update(MapView *view, const MapProbeResult *data);
void map_view_update_navigation(MapView *view, const NavProbeResult *data);
const char *map_view_status(const MapView *view);
int map_view_action(MapView *view, const char *action);
int map_view_render(const MapView *view, uint8_t *rgba, size_t size);
