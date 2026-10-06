#!/usr/bin/env python3
"""Validate and package the development-only Eden Duo companion, offline."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOP_KEYS = {"format", "name", "title_id", "min_runtime", "canvas_w", "canvas_h",
            "background", "nav", "requires_module", "pages"}
PAGE_KEYS = {"id", "title", "widgets"}
WIDGET_KEYS = {"type", "rect", "text", "text_scale", "color", "bg", "frame",
               "text_center_h", "fit_text", "text_min_scale", "wrap_width", "max_lines"}
COLOR_KEYS = {"color", "bg", "background"}


class DevelopmentError(ValueError):
    pass


def read_object(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise DevelopmentError(f"Cannot read {path.name}: {exc}") from exc
    if not isinstance(value, dict):
        raise DevelopmentError(f"{path.name} must contain a JSON object")
    return value


def known_keys(value: dict, allowed: set[str], where: str) -> None:
    unknown = set(value) - allowed
    if unknown:
        raise DevelopmentError(f"Unsupported development fields in {where}: {sorted(unknown)}")
    for key in COLOR_KEYS & set(value):
        color = value[key]
        if not ((type(color) is int and 0 <= color <= 0xFFFFFFFF)
                or (isinstance(color, str) and re.fullmatch(r"#[0-9A-Fa-f]{8}", color))):
            raise DevelopmentError(f"{where}.{key} must be #AARRGGBB or an unsigned 32-bit color")


def validate_development(root: Path = ROOT) -> dict:
    """Narrow policy for this first static build, not a general Eden validator."""
    project = read_object(root / "project.json")
    if (project.get("stage") != "development-ui-only"
            or project.get("supported_builds") != []
            or project.get("verified_on_thor") is not False
            or project.get("live_data_available") is not False):
        raise DevelopmentError("This builder only ships the unverified development UI; live support needs a new validation stage.")
    package = root / "package"
    for path in package.rglob("*"):
        if path.is_symlink():
            raise DevelopmentError("Package source cannot contain symbolic links")
        if path.is_file() and path.relative_to(package).as_posix() != "dualscreen/manifest.json":
            raise DevelopmentError("Development package must contain only dualscreen/manifest.json")
    manifest = read_object(package / "dualscreen/manifest.json")
    known_keys(manifest, TOP_KEYS, "manifest")
    if manifest.get("format") != 1:
        raise DevelopmentError("Manifest format must be 1")
    if manifest.get("title_id") != project.get("target_title_id"):
        raise DevelopmentError("Project and manifest title IDs must match")
    if not re.fullmatch(r"[0-9A-F]{16}", str(manifest.get("title_id", ""))):
        raise DevelopmentError("Title ID must contain 16 uppercase hexadecimal characters")
    if (manifest.get("min_runtime") != project.get("minimum_runtime")
            or manifest.get("nav") is not False
            or manifest.get("requires_module") is not False):
        raise DevelopmentError("Runtime must match; navigation and module requirement must be explicitly disabled")
    if (manifest.get("canvas_w"), manifest.get("canvas_h")) != (1240, 1080):
        raise DevelopmentError("Development canvas must be 1240 x 1080")
    pages = manifest.get("pages")
    if not isinstance(pages, list) or len(pages) != 1:
        raise DevelopmentError("This first development build must have exactly one status page")
    labels = []
    page = pages[0]
    if not isinstance(page, dict):
        raise DevelopmentError("Page must be an object")
    known_keys(page, PAGE_KEYS, "page")
    if not isinstance(page.get("id"), str) or not page["id"] or page["id"].startswith("@"):
        raise DevelopmentError("Page needs a non-reserved ID")
    widgets = page.get("widgets")
    if not isinstance(widgets, list) or not widgets:
        raise DevelopmentError("Status page has no widgets")
    for number, widget in enumerate(widgets):
        if not isinstance(widget, dict):
            raise DevelopmentError("Widget must be an object")
        known_keys(widget, WIDGET_KEYS, f"widget {number}")
        if widget.get("type") not in {"label", "rect"}:
            raise DevelopmentError("Development widgets must be static labels or rectangles")
        rect = widget.get("rect")
        if not isinstance(rect, list) or len(rect) != 4 or any(type(v) is not int for v in rect):
            raise DevelopmentError("Widget rectangles must have four integer coordinates")
        x, y, width, height = rect
        if (x < 0 or y < 0 or x >= 1240 or y >= 1080 or width < 0 or height < 0
                or x + width > 1240 or y + height > 1080
                or (widget["type"] == "rect" and (width == 0 or height == 0))):
            raise DevelopmentError(f"Widget {number} is outside the canvas")
        if widget["type"] == "label":
            if not isinstance(widget.get("text"), str):
                raise DevelopmentError("Every label needs literal text")
            scale = widget.get("text_scale", 1)
            if type(scale) is not int or not 1 <= scale <= 16:
                raise DevelopmentError("Text scale must be an integer between 1 and 16")
            # Eden accepts [x,y,0,0] for labels: text layout determines their size.
            # Check the declared wrapping/centering limits without pretending to
            # reproduce the emulator's actual font renderer.
            for field, available in (("wrap_width", 1240 - x), ("text_center_h", 1080 - y)):
                if field in widget and (type(widget[field]) is not int or not 0 <= widget[field] <= available):
                    raise DevelopmentError(f"Widget {number} has an invalid {field}")
            labels.append(widget["text"])
    for required in ("DEVELOPMENT PREVIEW", "Live game data is not connected"):
        if required not in labels:
            raise DevelopmentError(f"Visible status is required: {required}")
    return project


def build(root: Path = ROOT, output: Path | None = None) -> Path:
    project = validate_development(root)
    pin = read_object(root / "vendor/UPSTREAM.json")
    helper_path = root / "vendor/build_dualscreen_package.py"
    expected = pin["files"]["vendor/build_dualscreen_package.py"]["sha256"]
    if hashlib.sha256(helper_path.read_bytes()).hexdigest() != expected:
        raise DevelopmentError("Vendored upstream builder has changed; inspect and update its provenance before using it")
    spec = importlib.util.spec_from_file_location("eden_duo_package_builder", helper_path)
    if spec is None or spec.loader is None:
        raise DevelopmentError("Cannot load the vendored package builder")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.build_package(
        package=root / "package", output=output or root / "dist",
        version=project["version"], name="Diablo III Duo - Development UI",
        min_runtime=project["minimum_runtime"],
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Output directory (default: project dist/)")
    parser.add_argument("--check", action="store_true", help="Validate development source without building")
    args = parser.parse_args()
    try:
        if args.check:
            validate_development()
            print("Development manifest validated. Live support: unavailable. Thor testing: pending.")
        else:
            archive = build(output=args.output)
            print(f"Built: {archive}")
            print("Development UI only. No game-memory access. No tested Diablo III builds.")
    except (DevelopmentError, OSError, ValueError, KeyError) as exc:
        print(f"Build stopped: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
