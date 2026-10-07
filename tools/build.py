#!/usr/bin/env python3
"""Validate and package the development-only Eden Duo companion, offline."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
TOP_KEYS = {"format", "name", "title_id", "min_runtime", "canvas_w", "canvas_h",
            "background", "nav", "requires_module", "pages", "actions", "font", "font_atlas"}
PAGE_KEYS = {"id", "title", "widgets"}
COMMON_WIDGET_KEYS = {"type", "rect", "id", "on_tap"}
WIDGET_KEYS = {
    "label": COMMON_WIDGET_KEYS | {"text", "text_scale", "color", "bg", "text_center_h",
                                   "fit_text", "text_min_scale", "wrap_width", "max_lines"},
    "rect": COMMON_WIDGET_KEYS | {"color", "bg", "frame"},
    "image": COMMON_WIDGET_KEYS | {"src", "tint"},
}
COLOR_KEYS = {"color", "bg", "background", "tint"}
ASSET_SUFFIXES = {".png", ".mfnt", ".rec", ".txt"}
# These directory names are omitted by the pinned upstream packager (or reserved
# for native modules). An approved asset must actually survive packaging.
EXCLUDED_ASSET_DIRS = {".git", "__pycache__", "build", "cache", "caches", "debug",
                       "dump", "dumps", "extract", "extracted", "tmp", "temp", "modules"}
ID_RE = re.compile(r"[A-Za-z0-9_-]{1,64}")


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


def asset_path(value: str) -> PurePosixPath:
    if (not isinstance(value, str) or "\\" in value or ":" in value or "\0" in value
            or any(part in {"", ".", ".."} for part in value.split("/"))):
        raise DevelopmentError(f"Invalid asset path: {value!r}")
    path = PurePosixPath(value)
    if (path.is_absolute() or len(path.parts) < 2 or path.parts[0] != "dualscreen"
            or path.suffix.lower() not in ASSET_SUFFIXES
            or set(part.lower() for part in path.parts[:-1]) & EXCLUDED_ASSET_DIRS):
        raise DevelopmentError(f"Unsupported asset path or type: {value!r}")
    return path


def validate_assets(root: Path) -> set[str]:
    package = root / "package"
    inventory_path = root / "package-assets.json"
    if package.is_symlink() or inventory_path.is_symlink():
        raise DevelopmentError("Package source cannot contain symbolic links")
    inventory = read_object(inventory_path)
    for relative, digest in inventory.items():
        asset_path(relative)
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise DevelopmentError(f"Invalid asset SHA256: {relative}")
    found = set()
    for path in package.rglob("*"):
        if path.is_symlink():
            raise DevelopmentError("Package source cannot contain symbolic links")
        if path.is_dir():
            continue
        if not path.is_file():
            raise DevelopmentError("Package source must contain only regular files")
        relative = path.relative_to(package).as_posix()
        if relative == "dualscreen/manifest.json":
            continue
        if relative not in inventory:
            raise DevelopmentError(f"Package contains an unlisted asset: {relative}")
        if path.suffix.lower() == ".txt" and path.stat().st_size > 128 * 1024:
            raise DevelopmentError(f"Asset license/notice text exceeds 128 KiB: {relative}")
        if hashlib.sha256(path.read_bytes()).hexdigest() != inventory[relative]:
            raise DevelopmentError(f"Asset SHA256 hash mismatch: {relative}")
        found.add(relative)
    missing = set(inventory) - found
    if missing:
        raise DevelopmentError(f"Listed assets are missing: {sorted(missing)}")
    return found


def validate_file_reference(value: object, assets: set[str], suffixes: set[str]) -> None:
    if not isinstance(value, str) or not value.startswith("file:"):
        raise DevelopmentError("Static assets must use a local file: reference")
    relative = "dualscreen/" + value[5:]
    path = asset_path(relative)
    if relative not in assets or path.suffix.lower() not in suffixes:
        raise DevelopmentError(f"Asset reference is missing or has an unsupported type: {value}")


def valid_id(value: object) -> bool:
    return isinstance(value, str) and ID_RE.fullmatch(value) is not None


def validate_development(root: Path = ROOT) -> dict:
    """Static design-preview policy, not a general Eden manifest validator."""
    project = read_object(root / "project.json")
    if (project.get("stage") != "development-ui-only"
            or project.get("supported_builds") != []
            or project.get("verified_on_thor") is not False
            or project.get("live_data_available") is not False):
        raise DevelopmentError("This builder only ships the unverified development UI; live support needs a new validation stage.")
    assets = validate_assets(root)
    manifest = read_object(root / "package/dualscreen/manifest.json")
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
    for key, suffixes in (("font", {".mfnt", ".rec"}), ("font_atlas", {".png"})):
        if key in manifest:
            validate_file_reference(manifest[key], assets, suffixes)
    pages = manifest.get("pages")
    if not isinstance(pages, list) or not 1 <= len(pages) <= 4:
        raise DevelopmentError("Design preview must have between one and four pages")
    page_ids = set()
    for page in pages:
        if not isinstance(page, dict):
            raise DevelopmentError("Page must be an object")
        known_keys(page, PAGE_KEYS, "page")
        if not valid_id(page.get("id")):
            raise DevelopmentError("Page needs a non-reserved alphanumeric ID")
        if page["id"] in page_ids:
            raise DevelopmentError("Page IDs must be unique")
        page_ids.add(page["id"])
    actions = manifest.get("actions", {})
    if not isinstance(actions, dict):
        raise DevelopmentError("Page actions must be an object")
    for name, action in actions.items():
        if not valid_id(name) or not isinstance(action, dict):
            raise DevelopmentError("Page actions need valid names and objects")
        known_keys(action, {"kind", "page"}, f"page action {name}")
        if action.get("kind") != "page" or not isinstance(action.get("page"), str) or action["page"] not in page_ids:
            raise DevelopmentError(f"Only page actions to existing page targets are permitted: {name}")
    for page in pages:
        labels = []
        widget_ids = set()
        widgets = page.get("widgets")
        if not isinstance(widgets, list) or not widgets:
            raise DevelopmentError("Status page has no widgets")
        for number, widget in enumerate(widgets):
            if not isinstance(widget, dict):
                raise DevelopmentError("Widget must be an object")
            kind = widget.get("type")
            if not isinstance(kind, str) or kind not in WIDGET_KEYS:
                raise DevelopmentError("Development widgets must be static labels, rectangles or images")
            known_keys(widget, WIDGET_KEYS[kind], f"widget {number}")
            if "id" in widget:
                if not valid_id(widget["id"]) or widget["id"] in widget_ids:
                    raise DevelopmentError("Widget IDs must be valid and unique within their page")
                widget_ids.add(widget["id"])
            if "on_tap" in widget and (not isinstance(widget["on_tap"], str) or widget["on_tap"] not in actions):
                raise DevelopmentError("Widget on_tap must name a declared page action")
            rect = widget.get("rect")
            if not isinstance(rect, list) or len(rect) != 4 or any(type(v) is not int for v in rect):
                raise DevelopmentError("Widget rectangles must have four integer coordinates")
            x, y, width, height = rect
            if (x < 0 or y < 0 or x >= 1240 or y >= 1080 or width < 0 or height < 0
                    or x + width > 1240 or y + height > 1080
                    or (kind in {"rect", "image"} and (width == 0 or height == 0))):
                raise DevelopmentError(f"Widget {number} is outside the canvas")
            if kind == "image":
                validate_file_reference(widget.get("src"), assets, {".png"})
            if kind == "label":
                if not isinstance(widget.get("text"), str):
                    raise DevelopmentError("Every label needs literal text")
                scale = widget.get("text_scale", 1)
                if type(scale) is not int or not 1 <= scale <= 16:
                    raise DevelopmentError("Text scale must be an integer between 1 and 16")
                # Labels may use zero-size rectangles; the native font renderer
                # determines their measured bounds. These checks do not emulate it.
                for field, available in (("wrap_width", 1240 - x), ("text_center_h", 1080 - y)):
                    if field in widget and (type(widget[field]) is not int or not 0 <= widget[field] <= available):
                        raise DevelopmentError(f"Widget {number} has an invalid {field}")
                color = widget.get("color", 0xFFE6ECF2)
                color = int(color[1:], 16) if isinstance(color, str) else color
                if color >> 24:
                    labels.append(widget["text"])
        for required in ("DESIGN PREVIEW", "Live game data is not connected"):
            if required not in labels:
                raise DevelopmentError(f"Visible status is required on page {page['id']}: {required}")
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
