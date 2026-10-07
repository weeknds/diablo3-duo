// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "markers_probe.h"
#include <stddef.h>

#define MARKERS_SEEN_GOBLINS 128u
#define MARKERS_ALERT_TICKS 360u
typedef struct MarkerSlot { int active; Marker marker; } MarkerSlot;
typedef struct MarkersView {
    uint32_t world_id, count;
    int has_world, available, selected, selection_pending;
    MarkerSlot slots[MARKERS_MAX_ITEMS];
    uint32_t seen[MARKERS_SEEN_GOBLINS], seen_count, seen_cursor;
    uint32_t alert_id;
    uint64_t alert_until;
} MarkersView;

void markers_view_clear(MarkersView *view);
void markers_view_update(MarkersView *view, const MarkersProbeResult *data,
                         const MapProbeResult *map, uint64_t tick);
int markers_view_action(MarkersView *view, const char *action, int64_t argument);
const char *marker_name(MarkerKind kind);
const char *marker_icon(MarkerKind kind);
int markers_view_focus(const MarkersView *view, float x, float y,
                       char *text, size_t size, double *angle);
int markers_view_alert(const MarkersView *view, uint64_t tick);
