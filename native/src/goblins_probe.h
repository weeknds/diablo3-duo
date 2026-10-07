// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "markers_probe.h"

#define GOBLINS_MAX_NODES 256u
#define GOBLINS_MAX_READS 9000u
#define GOBLINS_MAX_BYTES 384000u

/* Returns a separate marker batch. Uses the native minimap actor list, resident
 * monster-family metadata and a strict subset of native visibility gates.
 * Missing data clears the batch. No guest calls, writes, or asset loading. */
void goblins_probe(const EdenDsmodHostApi *host,
                   const PlayerProbeIdentity *expected,
                   const MapProbeResult *map, MarkersProbeResult *result);
