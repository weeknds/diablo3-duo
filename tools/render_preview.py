#!/usr/bin/env python3
"""Offline visualization of the native manifest; not an Eden/device screenshot.

Sample and stress values change only this in-memory render. The installable
manifest is never modified. Requires Pillow; normal packaging does not.
"""
import argparse
import json
import struct
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "package/dualscreen"
SAMPLE = {"preview_badge": "SAMPLE DATA", "level": "70", "aps": "1.69", "cdr": "38%",
          "armor": "8,472", "movement": "+25%",
          "skill_0": "Multishot", "skill_1": "Evasive Fire", "skill_2": "Vault",
          "skill_3": "Vengeance", "skill_4": "Companion", "skill_5": "Preparation",
          "rune_0": "Arsenal", "rune_1": "Focus", "rune_2": "Tumble",
          "rune_3": "Seethe", "rune_4": "Wolf Companion", "rune_5": "Invigoration",
          "skill_detail": "Skill information preview",
          "skill_note": "Sample build shown; live skill and rune data remain unavailable.",
          "connection": "Illustrative values only. Not connected to the game."}
STRESS = {"preview_badge": "LAYOUT TEST", "level": "9999", "aps": "99.99", "cdr": "100%",
          "armor": "1,234,567", "movement": "+100%",
          "skill_0": "Hammer of the Ancients",
          "skill_1": "Synthetic Skill Name For Width Testing",
          "skill_2": "Synthetic Extremely Long Skill Name For Ellipsis Verification",
          "skill_3": "SyntheticSkillWithoutSpacesForCharacterWrappingVerification",
          "skill_4": "Synthetic Skill Four", "skill_5": "Synthetic Skill Five",
          "rune_0": "Long synthetic rune selection for layout testing",
          "rune_1": "Synthetic rune one", "rune_2": "Synthetic rune two",
          "rune_3": "Synthetic rune three", "rune_4": "Synthetic rune four",
          "rune_5": "Synthetic rune five", "skill_detail": "Layout test: text fitting",
          "skill_note": "Synthetic labels exercise fitting and truncation only.",
          "connection": "Layout test only. Not live game data."}


def rgba(value):
    if isinstance(value, str):
        value = int(value.lstrip("#"), 16)
    return ((value >> 16) & 255, (value >> 8) & 255, value & 255, (value >> 24) & 255)


def measure_text(text, scale, cap, glyphs):
    """Eden MeasureText truncates the accumulated glyph advances to an integer."""
    ratio = scale * 5 / cap
    return int(sum(glyphs[ord(char)][6] * ratio for char in text))


def layout_label(text, widget, cap, glyphs):
    """Mirror the used single-line subset of DrawLabel and Canvas::LayoutLines.

    Source: Eden 5410366938b8aa0aca2c9c61f4e93038b71db90c, mod_ui.cpp
    and mod_ui_text.cpp. This intentionally rejects unsupported preview fields
    instead of producing plausible pixels for semantics it does not implement.
    """
    if (widget.get("max_lines", 1) != 1 or "\n" in text
            or widget.get("align", "left") != "left" or widget.get("text_center_h", 0)):
        raise ValueError("Preview supports left-aligned single-line labels only")
    missing = sorted(set(text) - {chr(cp) for cp in glyphs})
    if missing:
        raise ValueError(f"Preview font lacks glyphs: {missing!r}")
    scale = max(1, widget["text_scale"])
    limit = widget.get("wrap_width", 0)
    width = lambda value, size=scale: measure_text(value, size, cap, glyphs)
    if widget.get("fit_text") and limit > 0:
        floor = min(scale, max(1, widget.get("text_min_scale", 1)))
        if scale > floor and width(text) > limit:
            low, high = floor, scale - 1
            while low < high:
                mid = low + (high - low + 1) // 2
                if width(text, mid) <= limit:
                    low = mid
                else:
                    high = mid - 1
            scale = low
    width = lambda value: measure_text(value, scale, cap, glyphs)
    if limit <= 0:
        return text, scale, False

    # Preserve Eden's word boundary before applying max_lines. Simply clipping
    # the full string to the available width would produce a different result.
    lines, current = [], None
    for word in text.split(" "):
        if current is not None:
            candidate = current + " " + word
            if width(candidate) <= limit:
                current = candidate
                continue
            lines.append(current)
            current = None
        if width(word) <= limit:
            current = word
        else:
            piece = ""
            for char in word:
                candidate = piece + char
                if piece and width(candidate) > limit:
                    lines.append(piece)
                    piece = char
                else:
                    piece = candidate
            current = piece
    lines.append(current or "")
    rendered = lines[0].rstrip(" ")
    truncated = len(lines) > 1
    if truncated:
        ellipsis = "\u2026" if glyphs.get(0x2026, (0,) * 7)[6] > 0 else "..."
        while rendered and width(rendered + ellipsis) > limit:
            rendered = rendered[:-1].rstrip(" ")
        rendered += ellipsis
    if width(rendered) > limit:
        raise ValueError(f"Label cannot fit within {limit}px even after native fitting: {text!r}")
    return rendered, scale, truncated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--sample", action="store_true", help="Labelled sample PNGs only, never package data")
    mode.add_argument("--stress", action="store_true", help="Labelled synthetic layout-test PNGs only")
    args = parser.parse_args()
    replacements = STRESS if args.stress else SAMPLE if args.sample else {}
    suffix = "stress" if args.stress else "sample" if args.sample else "unavailable"
    manifest = json.loads((PACKAGE / "manifest.json").read_text())
    metrics = (PACKAGE / "assets/duo-sans.mfnt").read_bytes()
    cap = struct.unpack_from("<I", metrics, 0x1C)[0]
    count = struct.unpack_from("<I", metrics, 0x20)[0]
    first = struct.unpack_from("<I", metrics, 0x28)[0]
    glyphs = {i + 31: struct.unpack_from("<4H2hH", metrics, first + 14 * i) for i in range(1, count)}
    atlas = Image.open(PACKAGE / "assets/duo-sans.png").convert("RGBA")
    out = ROOT / ("dist/design" if args.stress else "docs/images")
    out.mkdir(parents=True, exist_ok=True)
    report = []
    for page in manifest["pages"]:
        if replacements and page["id"] == "map":
            continue  # No invented map data, even in the offline example.
        canvas = Image.new("RGBA", (1240, 1080), rgba(manifest["background"]))
        draw = ImageDraw.Draw(canvas)
        for widget in page["widgets"]:
            x, y, w, h = widget["rect"]
            if widget["type"] == "rect":
                frame = widget.get("frame", 0)
                draw.rectangle((x, y, x + w - 1, y + h - 1), fill=rgba(widget["bg"]),
                               outline=rgba(widget["color"]) if frame else None, width=frame)
            elif widget["type"] == "image":
                src = PACKAGE / widget["src"].removeprefix("file:")
                art = Image.open(src).convert("RGBA").resize((w, h), Image.Resampling.LANCZOS)
                if "tint" in widget:
                    art = ImageChops.multiply(art, Image.new("RGBA", art.size, rgba(widget["tint"])))
                canvas.alpha_composite(art, (x, y))
            else:
                text = replacements.get(widget.get("id"), widget["text"])
                rendered, scale, truncated = layout_label(text, widget, cap, glyphs)
                ratio = scale * 5 / cap
                width = measure_text(rendered, scale, cap, glyphs)
                limit = widget["wrap_width"]
                pen, baseline = float(x), y + scale * 5
                ink_bounds = None
                for char in rendered:
                    gx, gy, gw, gh, bx, by, advance = glyphs[ord(char)]
                    if gw and gh:
                        if gx < 0 or gy < 0 or gx + gw > atlas.width or gy + gh > atlas.height:
                            raise ValueError(f"Glyph outside atlas: {char!r}")
                        tile = atlas.crop((gx, gy, gx + gw, gy + gh))
                        size = (max(1, int(gw * ratio)), max(1, int(gh * ratio)))
                        tile = tile.resize(size, Image.Resampling.BILINEAR)
                        tile = ImageChops.multiply(tile, Image.new("RGBA", size, rgba(widget["color"])))
                        tx, ty = int(pen + bx * ratio), int(baseline - by * ratio)
                        ink = tile.getchannel("A").getbbox()
                        if ink:
                            bounds = [tx + ink[0], ty + ink[1], tx + ink[2], ty + ink[3]]
                            if (bounds[0] < 0 or bounds[1] < 0
                                    or bounds[2] > canvas.width or bounds[3] > canvas.height):
                                raise ValueError(f"{page['id']} glyph ink outside canvas: {text!r}, {bounds}")
                            ink_bounds = bounds if ink_bounds is None else [
                                min(ink_bounds[0], bounds[0]), min(ink_bounds[1], bounds[1]),
                                max(ink_bounds[2], bounds[2]), max(ink_bounds[3], bounds[3])]
                        canvas.alpha_composite(tile, (tx, ty))
                    pen += advance * ratio
                report.append({"page": page["id"], "id": widget.get("id"), "text": text,
                               "rendered_text": rendered, "requested_scale": widget["text_scale"],
                               "effective_scale": scale, "fitted": scale != widget["text_scale"],
                               "truncated": truncated, "width": width, "limit": limit,
                               "ink_bounds": ink_bounds})
        path = out / f"design-{page['id']}-{suffix}.png"
        canvas.convert("RGB").save(path)
        print(path.relative_to(ROOT))
    evidence = ROOT / "dist/design"
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / f"bounds-{suffix}.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
