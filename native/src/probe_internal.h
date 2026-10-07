// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "player_probe.h"
#include "health_probe.h"
#include <limits.h>
#include <string.h>
#define ROOT_OFFSET UINT64_C(0x114A840)

typedef struct Reader { const EdenDsmodHostApi *host; PlayerProbeResult *out; uint32_t max_reads, max_bytes; HealthDiagnostics *diagnostic; } Reader;
typedef PlayerProbeIdentity Snapshot;

static int add(uint64_t base, uint64_t offset, uint64_t *address) {
    if (!base || base > UINT64_MAX - offset) return 0;
    *address = base + offset;
    return 1;
}

static int reject_read(Reader *r, const char *failure) {
    if (r->diagnostic) r->diagnostic->failure = failure;
    return 0;
}

static int read_at(Reader *r, uint64_t address, void *output, uint32_t size, uint32_t alignment) {
    if (r->diagnostic) {
        r->diagnostic->access_address = address; r->diagnostic->access_size = size;
        r->diagnostic->access_known = 1; r->diagnostic->pointer_known = 0;
        r->diagnostic->failure = "NONE";
    }
    if (!address) return reject_read(r, "ADDRESS_ZERO");
    if (address % alignment) return reject_read(r, "ADDRESS_ALIGNMENT");
    if (address > UINT64_MAX - size) return reject_read(r, "ADDRESS_OVERFLOW");
    if (r->out->reads >= r->max_reads || r->out->bytes > r->max_bytes ||
        size > r->max_bytes - r->out->bytes) return reject_read(r, "BUDGET");
    if (r->host->is_mapped(r->host->userdata, address, size) != EDEN_DSMOD_TRUE) return reject_read(r, "UNMAPPED");
    r->out->reads++; r->out->bytes += size;
    return r->host->read_memory(r->host->userdata, address, output, size) == EDEN_DSMOD_TRUE || reject_read(r, "READ_FAILED");
}

static int read_offset(Reader *r, uint64_t base, uint64_t offset, void *out, uint32_t size, uint32_t alignment) {
    uint64_t address;
    if (!add(base, offset, &address)) {
        if (r->diagnostic) { r->diagnostic->access_known = 0; r->diagnostic->access_size = size; }
        return reject_read(r, base ? "ADDRESS_OVERFLOW" : "ADDRESS_ZERO");
    }
    return read_at(r, address, out, size, alignment);
}

static int pointer(Reader *r, uint64_t base, uint64_t offset, uint64_t *out) {
    if (r->diagnostic) r->diagnostic->pointer_known = 0;
    if (!read_offset(r, base, offset, out, 8, 8)) return 0;
    if (r->diagnostic) { r->diagnostic->pointer_known = 1; r->diagnostic->pointer_value = *out; }
    if (!*out) return reject_read(r, "POINTER_NULL");
    if (*out % 8) return reject_read(r, "POINTER_ALIGNMENT");
    return 1;
}

static int snapshot(Reader *r, Snapshot *s) {
    memset(s, 0, sizeof *s);
    r->out->reason = "UNAVAILABLE: main/client route";
    if (!pointer(r, r->host->main_base, ROOT_OFFSET, &s->globals) ||
        !pointer(r, s->globals, 0x10, &s->client)) return 0;
    r->out->reason = "UNAVAILABLE: startup or game-type gate";
    if (!read_offset(r, s->client, 0x84, &s->game_type, 4, 4) || s->game_type <= 0) return 0;
    r->out->reason = "UNAVAILABLE: single-player selector";
    if (!pointer(r, s->client, 0xBA0, &s->selector) ||
        !pointer(r, s->client, 0x948, &s->players)) return 0;
    int32_t selector[5];
    if (!read_at(r, s->selector, selector, sizeof selector, 4)) return 0;
    memcpy(s->slots, selector, sizeof s->slots); s->count = selector[4];
    if (s->count != 1) return 0;
    int32_t index = -1;
    for (unsigned i = 0; i < 4; ++i) {
        if (s->slots[i] == -1) continue;
        if (s->slots[i] < 0 || s->slots[i] > 3 || index != -1) return 0;
        index = s->slots[i];
    }
    if (index < 0 || !add(s->players, (uint64_t)index * 0xEBF8 + 0x60, &s->player)) return 0;
    r->out->reason = "UNAVAILABLE: player identity";
    uint32_t ids[3];
    if (!read_at(r, s->player, ids, sizeof ids, 4)) return 0;
    s->player_index = (int32_t)ids[0]; s->acd_id = ids[1]; s->actor_id = ids[2];
    if (s->player_index != index || s->acd_id == UINT32_MAX || s->actor_id == UINT32_MAX) return 0;
    r->out->reason = "UNAVAILABLE: actor pool identity";
    if (!pointer(r, s->client, 0xA98, &s->pool) ||
        !read_offset(r, s->pool, 0x100, &s->capacity, 4, 4) ||
        (s->actor_id & 0xffffu) >= s->capacity ||
        !read_offset(r, s->pool, 0x168, &s->shift, 4, 4) || s->shift > 16 ||
        !pointer(r, s->pool, 0x120, &s->pages)) return 0;
    const uint32_t low = s->actor_id & 0xffffu;
    if (!pointer(r, s->pages, (uint64_t)(low >> s->shift) * 8, &s->page) ||
        !add(s->page, (uint64_t)(low & ((UINT32_C(1) << s->shift) - 1)) * 0x410, &s->actor) ||
        !read_at(r, s->actor, &s->actor_identity, 4, 4) || s->actor_identity != s->actor_id) return 0;
    return 1;
}
