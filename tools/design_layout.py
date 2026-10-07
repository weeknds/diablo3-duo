#!/usr/bin/env python3
"""Build native layout and original UI assets. Requires Pillow only for regeneration."""
from __future__ import annotations

import hashlib
import json
import math
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "package/dualscreen/assets"
FONTS = ROOT / "design/fonts"
C = {"bg": "11110E", "panel": "191813", "selected": "242117",
     "rule": "49412E", "gold": "D7B574", "text": "F1EBDD", "muted": "BDB4A0"}


def color(name):
    return "#FF" + C.get(name, name)


def font_assets():
    font = ImageFont.truetype(str(FONTS / "SourceSans3-Regular.otf"), 48,
                              layout_engine=ImageFont.Layout.BASIC)
    atlas = Image.new("RGBA", (1024, 512), (255, 255, 255, 0))
    records = []
    for index, cp in enumerate(range(32, 127)):
        mask, offset = font.getmask2(chr(cp), mode="L", anchor="ls")
        alpha = Image.frombytes("L", mask.size, bytes(mask)) if all(mask.size) else None
        ink = alpha.getbbox() if alpha else None
        x, y = index % 16 * 64 + 2, index // 16 * 80 + 2
        advance = round(font.getlength(chr(cp)))
        if ink:
            crop = alpha.crop(ink)
            w, h = crop.size
            bx, by = offset[0] + ink[0], -(offset[1] + ink[1])
            glyph = Image.new("RGBA", crop.size, "white")
            glyph.putalpha(crop)
            atlas.paste(glyph, (x, y))
        else:
            w = h = bx = by = 0
        assert w <= 60 and h <= 76 and 0 < advance <= 64
        records.append((x, y, w, h, bx, by, advance))
    cap = records[ord("H") - 32][3]
    # Original metrics written to Eden's documented in-core MFNT parser layout.
    # No font container or game asset is copied from a commercial game.
    header = bytearray(0x60)
    header[:4] = b"MFNT"
    struct.pack_into("<I", header, 0x1C, cap)
    struct.pack_into("<I", header, 0x20, len(records) + 1)
    struct.pack_into("<I", header, 0x28, len(header))
    metrics = bytes(header) + bytes(14) + b"".join(struct.pack("<4H2hH", *r) for r in records)
    (ASSETS / "duo-sans.mfnt").write_bytes(metrics)
    atlas.save(ASSETS / "duo-sans.png")
    (ASSETS / "OFL.txt").write_text((FONTS / "SourceSans-OFL.txt").read_text()
                                      + "\n\n" + (FONTS / "Cinzel-OFL.txt").read_text())
    (ASSETS / "NOTICE.txt").write_text(
        "Duo Sans is an ASCII bitmap derivative of Source Sans 3 Regular.\n"
        "Copyright 2010-2024 Adobe (http://www.adobe.com/). OFL-1.1.\n"
        "Modified font name: Duo Sans. Original reserved font name: Source.\n"
        "Cinzel heading images: Copyright 2020 The Cinzel Project Authors. OFL-1.1.\n"
        "Navigation pictograms are original code-drawn graphics.\n")


def text_width(text, scale):
    """Measure integer atlas advances, as Eden does, for fixed-label alignment."""
    metrics = (ASSETS / "duo-sans.mfnt").read_bytes()
    cap = struct.unpack_from("<I", metrics, 0x1C)[0]
    first = struct.unpack_from("<I", metrics, 0x28)[0]
    advance = sum(struct.unpack_from("<H", metrics, first + (ord(c) - 31) * 14 + 12)[0]
                  for c in text)
    return math.ceil(advance * scale * 5 / cap)


def title_asset(name, text, size=42, ink="text"):
    font = ImageFont.truetype(str(FONTS / "Cinzel.ttf"), size)
    box = font.getbbox(text)
    image = Image.new("RGBA", (box[2] - box[0] + 4, box[3] - box[1] + 4))
    ImageDraw.Draw(image).text((2 - box[0], 2 - box[1]), text, font=font, fill="#" + C[ink])
    image.save(ASSETS / f"{name}.png")
    return image.size


def icons():
    # Small navigation/slot pictograms, drawn consistently at 3 px nominal stroke.
    for name in ("combat", "map", "character"):
        im = Image.new("RGBA", (192, 192))
        d = ImageDraw.Draw(im)
        gold = "white"  # The runtime supplies the active/inactive color by tint.
        def line(p, width=9):
            d.line([(x * 3, y * 3) for x, y in p], fill=gold, width=width, joint="curve")
        if name == "combat":
            line([(12, 9), (46, 43), (54, 54)])
            line([(52, 9), (18, 43), (10, 54)])
            line([(9, 9), (17, 11), (12, 16), (9, 9)], 5)
            line([(55, 9), (47, 11), (52, 16), (55, 9)], 5)
            line([(37, 47), (48, 36)])
            line([(16, 36), (27, 47)])
        elif name == "map":
            line([(8, 16), (23, 10), (41, 16), (56, 10), (56, 48),
                  (41, 54), (23, 48), (8, 54), (8, 16)], 7)
            line([(23, 10), (23, 48)], 5)
            line([(41, 16), (41, 54)], 5)
        elif name == "character":
            line([(15, 8), (49, 8), (49, 56), (15, 56), (15, 8)], 7)
            for y in (22, 33, 44):
                line([(23, y), (41, y)], 6)
        im.resize((64, 64), Image.Resampling.LANCZOS).save(ASSETS / f"icon-{name}.png")


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


def heading(name, text, x, y, size=42, ink="text"):
    w, h = title_asset(name, text, size, ink)
    return image(name, x, y, w, h)


def chrome(page):
    widgets = [image("icon-combat", 39, 20, 40, 40),
               heading("brand", "DIABLO III DUO", 98, 27, 29, "gold"),
               rect(988, 22, 208, 40, "panel", "rule"),
               label("DESIGN PREVIEW", 1010, 32, 3, "muted", 174, "preview_badge"),
               rect(0, 83, 1240, 1, "rule"), rect(0, 980, 1240, 100, "bg"),
               rect(0, 980, 1240, 1, "rule")]
    for i, (key, title) in enumerate((("combat", "Combat"), ("map", "Map"), ("character", "Character"))):
        x = i * 413
        active = key == page
        group_x = x + (413 - 38 - 16 - text_width(title, 5)) // 2
        widgets += [rect(x, 982, 413 if i < 2 else 414, 98, "selected" if active else "bg", tap=f"open_{key}"),
                    image(f"icon-{key}", group_x, 1011, 38, 38, "gold" if active else "muted"),
                    label(title, group_x + 54, 1018, 5, "gold" if active else "muted", 225)]
        if active:
            widgets.append(rect(x + 42, 979, 330, 3, "gold"))
    return widgets


def status():
    return [rect(44, 889, 1152, 1, "rule"),
            label("Live game data is not connected", 44, 918, 4, "muted", 1000, "connection")]


def build_layout():
    char = chrome("character") + [heading("character-title", "Character", 44, 119),
            label("Build details at a glance", 46, 177, 5, "muted"),
            label("Level", 974, 128, 4, "muted", 106),
            label("--", 1076, 119, 7, "text", 120, "level", min_scale=5),
            rect(44, 237, 1152, 338, "panel", "rule"),
            rect(620, 261, 1, 290, "rule"),
            rect(72, 406, 1096, 1, "rule")]
    for i, (name, ident) in enumerate((("Attacks per second", "aps"),
            ("Cooldown reduction", "cdr"), ("Armor", "armor"), ("Movement bonus", "movement"))):
        x, y = 76 + i % 2 * 576, 266 + i // 2 * 169
        char += [label(name, x, y, 5, "muted", 510),
                 label("--", x, y + 55, 10, "text", 510, ident, min_scale=8)]
    char += [heading("skills-overview", "Equipped skills", 44, 620, 31, "gold"),
             rect(968, 591, 228, 88, "selected", "rule", "open_combat"),
             label("View skills", 1008, 623, 4, "gold", 165)]
    for i in range(6):
        x, y = 44 + i // 3 * 600, 697 + i % 3 * 62
        char += [label(str(i + 1), x + 4, y + 3, 4, "gold", 32),
                 label("Unavailable", x + 48, y, 5, "muted", 488, f"skill_{i}", min_scale=4)]
        if i % 3 < 2:
            char.append(rect(x, y + 45, 552, 1, "rule"))
    char += status()

    combat = chrome("combat") + [heading("combat-title", "Skills & runes", 44, 119),
             label("Equipped skills and their selected runes", 46, 177, 5, "muted")]
    for i in range(6):
        x, y = 44 + i // 3 * 600, 247 + i % 3 * 140
        combat += [rect(x, y, 552, 122, "panel", "rule"),
                   label(str(i + 1), x + 24, y + 29, 4, "gold", 32),
                   label("Unavailable", x + 76, y + 25, 5, "text", 452, f"skill_{i}", min_scale=4),
                   label("Rune unavailable", x + 76, y + 73, 4, "muted", 452, f"rune_{i}")]
    combat += [heading("rune-effects-title", "Runes & effects", 44, 709, 31, "gold"),
               label("Rune descriptions and active buff timers", 46, 776, 4, "muted"),
               label("are not available in this preview.", 46, 817, 4, "muted")] + status()

    map_page = chrome("map") + [heading("map-title", "Map", 44, 119),
              label("Location details", 46, 177, 5, "muted"),
              image("icon-map", 572, 386, 96, 96, "muted"),
              heading("map-unavailable", "Map unavailable", 0, 535, 34),
              label("Map support is not included in this preview.", 0, 611, 4, "muted", 1000)] + status()
    map_heading = next(w for w in map_page if w.get("src") == "file:assets/map-unavailable.png")
    map_heading["rect"][0] = (1240 - map_heading["rect"][2]) // 2
    map_note = next(w for w in map_page if w.get("text") == "Map support is not included in this preview.")
    map_note["wrap_width"] = text_width(map_note["text"], 4)
    map_note["rect"][0] = (1240 - map_note["wrap_width"]) // 2
    return {"format": 1, "name": "Diablo III Duo - Design Preview", "title_id": "01001B300B9BE000",
            "min_runtime": 18, "canvas_w": 1240, "canvas_h": 1080, "background": color("bg"),
            "nav": False, "requires_module": False, "font": "file:assets/duo-sans.mfnt",
            "font_atlas": "file:assets/duo-sans.png",
            "actions": {f"open_{p}": {"kind": "page", "page": p} for p in ("combat", "map", "character")},
            "pages": [{"id": name, "title": name.title(), "widgets": widgets}
                      for name, widgets in (("character", char), ("combat", combat), ("map", map_page))]}


def main():
    ASSETS.mkdir(parents=True, exist_ok=True)
    # Retired assets from our previous design, never a broad directory cleanup.
    for name in ("ranger.png", "details-title.png", "skill-details-title.png",
                 "icon-lock.png", "icon-shield.png", "icon-arrow.png"):
        (ASSETS / name).unlink(missing_ok=True)
    font_assets()
    icons()
    manifest = build_layout()
    (ROOT / "package/dualscreen/manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    # Only explicitly referenced assets can enter the package inventory.
    names = {w["src"][5:] for p in manifest["pages"] for w in p["widgets"] if w["type"] == "image"}
    names.update(manifest[k][5:] for k in ("font", "font_atlas"))
    names.update({"assets/OFL.txt", "assets/NOTICE.txt"})
    hashes = {"dualscreen/" + name: hashlib.sha256((ASSETS.parent / name).read_bytes()).hexdigest()
              for name in sorted(names)}
    (ROOT / "package-assets.json").write_text(json.dumps(hashes, indent=2) + "\n")
    print(f"Generated {len(manifest['pages'])} native pages; {len(hashes)} allowlisted assets.")


if __name__ == "__main__":
    main()
