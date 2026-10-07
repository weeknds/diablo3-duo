// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "map_probe.h"

#define MARKERS_MAX_ITEMS 64u
#define MARKERS_MAX_READS 900u
#define MARKERS_MAX_BYTES 65536u

typedef enum MarkerKind {
    MARKER_NONE = 0,
    MARKER_QUEST,
    MARKER_PORTAL,
    MARKER_WAYPOINT,
    MARKER_SHRINE,
    MARKER_PYLON,
    MARKER_GOBLIN
} MarkerKind;

typedef struct Marker {
    uint32_t id;
    MarkerKind kind;
    float x, y;
} Marker;

typedef struct MarkersProbeResult {
    int available, shared_identity_valid;
    uint32_t world_id, count;
    Marker items[MARKERS_MAX_ITEMS];
    uint32_t reads, bytes;
    const char *reason;
} MarkersProbeResult;

/* Read-only, exact-build markers. A current exploration snapshot is required;
 * no marker outside its revealed cells is returned. Unknown icons are omitted. */
void markers_probe(const EdenDsmodHostApi *host,
                   const PlayerProbeIdentity *expected,
                   const MapProbeResult *map, MarkersProbeResult *result);
