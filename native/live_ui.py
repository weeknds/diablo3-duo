#!/usr/bin/env python3
"""Compose the approved skin with the unchanged, exact-build native reader."""
import copy
import hashlib
import math
import struct
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
MODULE = 'modules/android-arm64-v8a/01001B300B9BE000.so'
MODULE_HASH = '9736a2ed41d0f302f5883e3e508238c9022b216816d70fc63b1633799ab53644'


BUILD_ID = '2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000'
VERSION = '0.1.2-rc.1'
ASSETS = REPO / 'package/dualscreen/assets'
C = {'bg':'11110E','selected':'242117','rule':'49412E','gold':'D7B574','text':'F1EBDD','muted':'BDB4A0'}

def color(name):
    return "#FF" + C.get(name, name)

def text_width(text, scale):
    """Measure integer atlas advances, as Eden does, for fixed-label alignment."""
    metrics = (ASSETS / "duo-sans.mfnt").read_bytes()
    cap = struct.unpack_from("<I", metrics, 0x1C)[0]
    first = struct.unpack_from("<I", metrics, 0x28)[0]
    advance = sum(struct.unpack_from("<H", metrics, first + (ord(c) - 31) * 14 + 12)[0]
                  for c in text)
    return math.ceil(advance * scale * 5 / cap)

def rect(x, y, w, h, bg="panel", stroke=None, tap=None):
    item = {"type": "rect", "rect": [x, y, w, h], "bg": color(bg),
            "color": color(stroke) if stroke else 0, "frame": 1 if stroke else 0}
    if tap:
        item["on_tap"] = tap
    return item

def label(text, x, y, scale=5, ink="text", width=1100, ident=None, min_scale=None):
    item = {"type": "label", "rect": [x, y, 0, 0], "text": text,
            "text_scale": scale, "color": color(ink), "wrap_width": min(width, 1240 - x),
            "max_lines": 1, "fit_text": True, "text_min_scale": min_scale or scale}
    if ident:
        item["id"] = ident
    return item

def image(name, x, y, w, h, tint=None):
    item = {"type": "image", "rect": [x, y, w, h], "src": f"file:assets/{name}.png"}
    if name.startswith("icon-") and tint is None:
        tint = "gold"
    if tint:
        item["tint"] = color(tint)
    return item


def pair(widget, key):
    offline = copy.deepcopy(widget)
    offline['id'] += '_offline'
    offline['need_bind'] = '!module_ready'
    live = copy.deepcopy(widget)
    live.update(bind_text=key, need_bind='module_ready', hide_bind='module_error', hide_eq=1)
    return [offline, live]


def make_manifest():
    m = json.loads((REPO / 'package/dualscreen/manifest.json').read_text())
    m.update(name='Diablo III Duo - Live Candidate', requires_module=True,
             module_tick_hidden=False, module={'abi': 1, 'build_ids': [BUILD_ID], 'libraries': {}})
    m['module']['libraries'] = {'android-arm64-v8a': {'path': MODULE, 'sha256': MODULE_HASH}}
    m['actions'].pop('open_map')
    m['pages'] = [p for p in m['pages'] if p['id'] != 'map']
    keys = {'level': 'details.level', 'aps': 'stats.0.value', 'cdr': 'stats.1.value',
            'armor': 'stats.2.value', 'movement': 'stats.3.value', 'connection': 'details.status'}
    keys.update({f'skill_{i}':f'skills.{i}.name' for i in range(6)})
    keys.update({f'rune_{i}':f'skills.{i}.rune' for i in range(6)})
    for p in m['pages']:
        widgets = []
        for w in p['widgets']:
            if w['rect'][1] >= 979 or w.get('text') == 'Level':
                continue
            ident = w.get('id')
            if ident == 'preview_badge':
                w['text'] = 'DEVICE TEST'
                w['rect'][0] = 1027
            if ident == 'level':
                w.update(text='Level unavailable', rect=[860, 128, 0, 0], wrap_width=336,
                         text_scale=5, text_min_scale=5)
            if ident in ('aps', 'cdr', 'armor', 'movement'):
                w['text_min_scale'] = 4  # Preserve all27 native numeric buffer characters.
            if ident == 'connection':
                w['text'] = 'Waiting for the companion to connect.'
            if w.get('text') == 'Equipped skills and their selected runes':
                w['text'] = 'Your currently equipped skills'
            if w.get('text') == 'are not available in this preview.':
                w['text'] = 'are not included in this version.'
            if ident in keys:
                variants = pair(w, keys[ident])
                if ident == 'connection':
                    variants[0].update(hide_bind='module_error', hide_eq=1)
                    error = copy.deepcopy(w)
                    error.update(id='connection_error', need_bind='module_error',
                                 text='Companion unavailable. Check the game version and reload.')
                    variants.append(error)
                widgets += variants
            else:
                widgets.append(w)
        widgets += [rect(0, 980, 1240, 100, 'bg'), rect(0, 980, 1240, 1, 'rule')]
        for i, (key, title) in enumerate([('character', 'Character'), ('combat', 'Skills')]):
            x = i * 620
            active = key == p['id']
            gx = x + (620 - 54 - text_width(title, 5)) // 2
            widgets += [rect(x, 982, 620, 98, 'selected' if active else 'bg', tap='open_' + key),
                        image('icon-' + key, gx, 1011, 38, 38, 'gold' if active else 'muted'),
                        label(title, gx + 54, 1018, 5, 'gold' if active else 'muted', 260)]
            if active:
                widgets.append(rect(x + 42, 979, 536, 3, 'gold'))
        p['widgets'] = widgets
    return m
