#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Regenerate the native companion's original typography and line icons.

Pillow is needed only for regeneration; the ordinary build uses pinned assets.
No game art or reference screenshot pixels are used.
"""
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, PngImagePlugin

ROOT = Path(__file__).resolve().parent
FONTS = ROOT.parent / "design/fonts"
OUT = ROOT / "assets"
GOLD = "#D7B574"
CREAM = "#F1EBDD"


def save(image, name):
    info = PngImagePlugin.PngInfo()
    origin = ("Original line geometry drawn by native/artwork.py; GPL-3.0-or-later."
              if name.startswith('icon-') or name == 'map-background' else
              "Typography rasterized by native/artwork.py from bundled OFL Cinzel or Source Sans 3; "
              "see native/assets/NOTICE.txt and OFL.txt.")
    info.add_text("Source", origin)
    info.add_text("impeccable:prompt", origin + " No generated image, game artwork or screenshot pixels.")
    image.save(OUT / (name + ".png"), pnginfo=info)


def heading(name, text, width, size, ink=CREAM):
    font = ImageFont.truetype(str(FONTS / "Cinzel.ttf"), size * 2)
    box = font.getbbox(text)
    height = size + 12
    assert box[2] - box[0] <= width * 2, text
    im = Image.new("RGBA", (width * 2, height * 2))
    ImageDraw.Draw(im).text((0, 4 - box[1]), text, font=font, fill=ink)
    save(im.resize((width, height), Image.Resampling.LANCZOS), name)


def icon(name, lines=(), polygons=(), ellipses=()):
    im = Image.new("RGBA", (256, 256))
    d = ImageDraw.Draw(im)
    for points in lines:
        d.line([(x * 4, y * 4) for x, y in points], fill="white", width=8, joint="curve")
    for points in polygons:
        d.polygon([(x * 4, y * 4) for x, y in points], fill="white")
    for box in ellipses:
        d.ellipse(tuple(v * 4 for v in box), outline="white", width=8)
    save(im.resize((64, 64), Image.Resampling.LANCZOS), "icon-" + name)


def map_assets():
    # Original flat location glyphs. Neither glyph represents a character.
    im=Image.new('RGBA',(128,128));d=ImageDraw.Draw(im)
    d.ellipse((12,12,116,116),fill='#141410',outline=GOLD,width=4)
    d.ellipse((30,30,98,98),fill=CREAM)
    d.ellipse((52,52,76,76),fill=GOLD)
    save(im.resize((64,64),Image.Resampling.LANCZOS),'icon-player')
    im=Image.new('RGBA',(128,128));d=ImageDraw.Draw(im)
    d.polygon([(23,66),(64,119),(105,66)],fill='#141410')
    d.ellipse((15,7,113,105),fill='#141410')
    d.polygon([(29,62),(64,110),(99,62)],fill=GOLD)
    d.ellipse((25,17,103,95),fill=GOLD)
    d.ellipse((49,41,79,71),fill='#141410')
    save(im.resize((64,64),Image.Resampling.LANCZOS),'icon-map-pin')
    save(Image.new('RGBA',(16,8),'#141410'),'map-background')
    marker_assets()


def marker_assets():
    """Small, distinct symbols stay readable above both bright and dark terrain."""
    for kind,ink in (('quest',GOLD),('portal',GOLD),('waypoint','#91BABD'),
                     ('shrine',CREAM),('pylon',GOLD),('goblin','#ACC77C')):
        im=Image.new('RGBA',(128,128));d=ImageDraw.Draw(im)
        d.ellipse((5,5,123,123),fill='#141410',outline=ink,width=5)
        if kind=='quest':
            d.polygon([(64,23),(103,64),(64,105),(25,64)],outline=ink,width=4)
            d.line([(64,42),(64,68)],fill=ink,width=8)
            d.ellipse((59,78,69,88),fill=ink)
        elif kind=='portal':
            d.arc((35,25,93,83),180,360,fill=ink,width=8)
            d.line([(35,54),(35,96),(93,96),(93,54)],fill=ink,width=8)
            d.line([(24,101),(104,101)],fill=ink,width=5)
        elif kind=='waypoint':
            d.polygon([(64,23),(98,64),(64,105),(30,64)],outline=ink,width=5)
            d.polygon([(64,44),(82,64),(64,84),(46,64)],fill=ink)
            d.line([(18,64),(29,64)],fill=ink,width=4)
            d.line([(99,64),(110,64)],fill=ink,width=4)
        elif kind=='shrine':
            d.line([(35,96),(93,96)],fill=ink,width=7)
            d.line([(45,88),(45,64),(83,64),(83,88)],fill=ink,width=6)
            d.polygon([(64,23),(80,45),(64,57),(50,45)],fill=ink)
        elif kind=='pylon':
            d.polygon([(68,21),(39,69),(61,69),(53,107),(91,56),(69,56)],fill=ink)
        else:
            d.polygon([(42,27),(86,27),(78,46),(50,46)],outline=ink,width=5)
            d.ellipse((31,42,97,102),outline=ink,width=6)
            d.line([(44,47),(84,47)],fill=ink,width=6)
            d.polygon([(64,59),(79,75),(64,91),(49,75)],fill=ink)
        save(im.resize((64,64),Image.Resampling.LANCZOS),'icon-poi-'+kind)
    im=Image.new('RGBA',(128,128));d=ImageDraw.Draw(im)
    d.polygon([(64,14),(101,74),(72,63),(72,111),(56,111),(56,63),(27,74)],fill=GOLD)
    save(im.resize((64,64),Image.Resampling.LANCZOS),'icon-direction')

def font():
    face = ImageFont.truetype(str(FONTS / "SourceSans3-Regular.otf"), 96,
                              layout_engine=ImageFont.Layout.BASIC)
    atlas = Image.new("RGBA", (2048, 1024))
    records = []
    for i, code in enumerate(range(32, 127)):
        mask, offset = face.getmask2(chr(code), mode="L", anchor="ls")
        alpha = Image.frombytes("L", mask.size, bytes(mask)) if all(mask.size) else None
        box = alpha.getbbox() if alpha else None
        x, y = i % 16 * 128 + 2, i // 16 * 164 + 2
        advance = round(face.getlength(chr(code)))
        if box:
            crop = alpha.crop(box)
            w, h = crop.size
            bx, by = offset[0] + box[0], -(offset[1] + box[1])
            glyph = Image.new("RGBA", crop.size, "white")
            glyph.putalpha(crop)
            atlas.paste(glyph, (x, y))
        else:
            w = h = bx = by = 0
        assert w < 124 and h < 160 and 0 < advance < 128
        records.append((x, y, w, h, bx, by, advance))
    header = bytearray(0x60)
    header[:4] = b"MFNT"
    struct.pack_into("<I", header, 0x1C, records[ord("H") - 32][3])
    struct.pack_into("<I", header, 0x20, len(records) + 1)
    struct.pack_into("<I", header, 0x28, len(header))
    (OUT / "duo-sans.mfnt").write_bytes(header + bytes(14) + b"".join(
        struct.pack("<4H2hH", *r) for r in records))
    save(atlas, "duo-sans")


def main():
    map_assets()
    OUT.mkdir(exist_ok=True)
    font()
    for i, title in enumerate(("Character", "Demon Hunter", "Barbarian", "Wizard",
                               "Witch Doctor", "Monk", "Crusader", "Necromancer")):
        heading("class-" + str(i), title, 820, 54)
    heading("brand", "DIABLO III DUO", 500, 29, GOLD)
    for name, title in (("equipment", "Equipment"), ("skills", "Skills & runes"),
                        ("map", "Exploration")):
        heading("title-" + name, title, 820, 48)
    heading("section-combat", "Combat details", 540, 29, GOLD)
    icon("character", [[(17, 7), (47, 7), (47, 57), (17, 57), (17, 7)],
                        [(24, 20), (40, 20)], [(24, 31), (40, 31)], [(24, 42), (36, 42)]])
    icon("sword", [[(13, 52), (46, 19)], [(15, 37), (28, 50)], [(9, 48), (17, 56)]],
         [[(30, 28), (50, 10), (47, 23), (35, 35)]])
    icon("shield", [[(11, 12), (32, 6), (53, 12), (51, 36), (44, 47), (32, 57),
                     (20, 47), (13, 36), (11, 12)],
                    [(19, 18), (32, 14), (45, 18), (44, 33), (38, 42), (32, 48)]])
    icon("movement", [[(24, 8), (24, 35), (17, 42), (7, 43), (7, 53), (52, 53),
                       (52, 45), (43, 41), (38, 30), (38, 8), (24, 8)],
                      [(10, 58), (49, 58)]])
    icon("map", [[(7, 15), (23, 9), (41, 15), (57, 9), (57, 49), (41, 55),
                  (23, 49), (7, 55), (7, 15)], [(23, 9), (23, 49)], [(41, 15), (41, 55)]])
    icon("skills", [[(11, 10), (51, 54)], [(53, 10), (13, 54)],
                     [(9, 39), (26, 53)], [(38, 53), (55, 39)]])
    icon("gear", [[(22, 8), (9, 18), (5, 34), (18, 39), (18, 55), (46, 55),
                   (46, 39), (59, 34), (55, 18), (42, 8)], [(22, 8), (27, 18), (37, 18), (42, 8)]])
    icon("helm", [[(13, 48), (13, 25), (19, 12), (32, 7), (45, 12), (51, 25),
                   (51, 48), (41, 55), (41, 33), (23, 33), (23, 55), (13, 48)],
                  [(32, 8), (32, 25)]])
    icon("hands", [[(18, 53), (9, 35), (11, 29), (16, 29), (22, 37), (20, 13),
                    (25, 11), (29, 29), (29, 8), (34, 8), (37, 29), (38, 11),
                    (43, 13), (43, 32), (46, 20), (51, 22), (49, 45), (43, 54), (18, 53)]])
    icon("belt", [[(8, 21), (56, 21), (56, 44), (8, 44), (8, 21)],
                   [(24, 17), (40, 17), (40, 48), (24, 48), (24, 17)], [(32, 31), (45, 31)]])
    icon("legs", [[(18, 7), (46, 7), (51, 56), (35, 56), (32, 28), (29, 56),
                   (13, 56), (18, 7)], [(18, 16), (46, 16)]])
    icon("shoulders", [[(5, 39), (9, 22), (22, 15), (32, 23), (42, 15), (55, 22),
                        (59, 39), (43, 45), (39, 28), (25, 28), (21, 45), (5, 39)]])
    icon("bracers", [[(22, 8), (45, 15), (37, 54), (13, 47), (22, 8)],
                      [(20, 19), (42, 25)], [(17, 32), (40, 38)]])
    icon("ring", ellipses=[(14, 21, 50, 56)], polygons=[[(32, 5), (40, 13), (32, 24), (24, 13)]])
    icon("neck", [[(10, 8), (12, 27), (21, 39), (32, 46), (43, 39), (52, 27), (54, 8)]],
         polygons=[[(32, 37), (40, 47), (32, 59), (24, 47)]])
    icon("pin", [[(32, 8), (43, 12), (49, 24), (47, 34), (32, 57), (17, 34),
                  (15, 24), (21, 12), (32, 8)]], ellipses=[(26, 20, 38, 32)])
    icon("recenter", [[(32, 5), (32, 20)], [(32, 44), (32, 59)],
                       [(5, 32), (20, 32)], [(44, 32), (59, 32)]], ellipses=[(13, 13, 51, 51), (27, 27, 37, 37)])
    (OUT / "OFL.txt").write_text((FONTS / "SourceSans-OFL.txt").read_text() + "\n\n" +
                                 (FONTS / "Cinzel-OFL.txt").read_text())
    (OUT / "NOTICE.txt").write_text(
        "Duo Sans is an ASCII bitmap derivative of Source Sans 3 Regular.\n"
        "Copyright 2010-2024 Adobe. OFL-1.1. Modified font name: Duo Sans.\n"
        "Cinzel lettering: Copyright 2020 The Cinzel Project Authors. OFL-1.1.\n"
        "Line icons are original code-drawn graphics. No character or game art.\n")
    inventory = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(OUT.iterdir())}
    (ROOT / "ASSETS.json").write_text(json.dumps(inventory, indent=2) + "\n")


if __name__ == "__main__":
    main()
