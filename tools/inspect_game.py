#!/usr/bin/env python3
"""Inspect an already decrypted ExeFS main without decrypting or uploading anything.

Only the 256-byte NSO header is read. Optional ROM input contributes its basename
and file size, never its contents. An NSO build ID identifies an executable; it
does not establish its title, version, compatibility, or live-memory layout.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import stat
import sys
from typing import Any


DEFAULT_TITLE_ID = "01001B300B9BE000"
NSO_HEADER_SIZE = 0x100
BUILD_ID_OFFSET = 0x40
BUILD_ID_SIZE = 32


class InspectionError(ValueError):
    """A local input cannot provide trustworthy executable metadata."""


def _regular_file(path: Path, label: str) -> os.stat_result:
    try:
        info = path.lstat()
    except OSError as exc:
        raise InspectionError(f"Cannot access {label} file {path.name!r}.") from exc
    if stat.S_ISLNK(info.st_mode):
        raise InspectionError(f"The {label} file must not be a symbolic link.")
    if not stat.S_ISREG(info.st_mode):
        raise InspectionError(f"The {label} input must be a regular file.")
    return info


def _read_main(path: Path) -> dict[str, Any]:
    before = _regular_file(path, "main")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as exc:
        raise InspectionError(f"Cannot read main file {path.name!r}.") from exc
    try:
        opened = os.fstat(descriptor)
        if not stat.S_ISREG(opened.st_mode) or (
            before.st_dev, before.st_ino
        ) != (opened.st_dev, opened.st_ino):
            raise InspectionError("The main file changed while opening it; try again.")
        if opened.st_size < NSO_HEADER_SIZE:
            raise InspectionError("Truncated main: a complete NSO header needs 256 bytes.")
        chunks = bytearray()
        while len(chunks) < NSO_HEADER_SIZE:
            block = os.read(descriptor, NSO_HEADER_SIZE - len(chunks))
            if not block:
                raise InspectionError("Truncated main: could not read its complete NSO header.")
            chunks.extend(block)
        if chunks[:4] != b"NSO0":
            raise InspectionError("Wrong file format: expected an already decrypted NSO0 main.")
        build_id = bytes(chunks[BUILD_ID_OFFSET:BUILD_ID_OFFSET + BUILD_ID_SIZE])
        if not any(build_id):
            raise InspectionError("The NSO header has an all-zero build ID; cannot identify this build.")
        return {
            "filename": path.name,
            "size_bytes": opened.st_size,
            "format": "NSO0",
            "header_bytes_read": NSO_HEADER_SIZE,
            "build_id_full": build_id.hex().upper(),
            "build_id_short": build_id[:8].hex().upper(),
        }
    except OSError as exc:
        raise InspectionError(f"Cannot inspect main file {path.name!r}.") from exc
    finally:
        os.close(descriptor)


def _title_id(value: str) -> str:
    if not re.fullmatch(r"[0-9a-fA-F]{16}", value):
        raise InspectionError("title ID must contain exactly 16 hexadecimal characters.")
    return value.upper()


def inspect_game(
    main: Path,
    *,
    rom: Path | None = None,
    game_version: str | None = None,
    title_id: str | None = None,
) -> dict[str, Any]:
    """Return shareable metadata without recording absolute paths or game bytes."""
    selected_title = _title_id(title_id if title_id is not None else DEFAULT_TITLE_ID)
    if game_version is not None and not game_version.strip():
        raise InspectionError("game version must not be blank when provided.")
    result: dict[str, Any] = {
        "schema_version": 1,
        "support_state": "unverified",
        "title_id": selected_title,
        "title_id_source": "user_provided" if title_id is not None else "project_default",
        "title_id_verified_against_main": False,
        "title_id_note": "An NSO header does not establish the game title ID.",
        "game_version": game_version.strip() if game_version is not None else None,
        "game_version_source": "user_provided" if game_version is not None else None,
        "main": _read_main(Path(main)),
        "rom": None,
        "live_memory_verified": False,
        "thor_tested": False,
    }
    if rom is not None:
        rom = Path(rom)
        info = _regular_file(rom, "ROM")
        result["rom"] = {
            "filename": rom.name,
            "size_bytes": info.st_size,
            "contents_read": False,
        }
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--main", required=True, type=Path,
                        help="already decrypted ExeFS main; only its NSO header is read")
    parser.add_argument("--rom", type=Path,
                        help="optional ROM file; record its basename and size only")
    parser.add_argument("--game-version", help="version label supplied by you, not inferred")
    parser.add_argument("--title-id",
                        help=f"unverified title ID (default: {DEFAULT_TITLE_ID}); not read from NSO")
    parser.add_argument("--output", type=Path,
                        help="create a new JSON file; existing files are never overwritten")
    args = parser.parse_args(argv)
    try:
        result = inspect_game(args.main, rom=args.rom, game_version=args.game_version,
                              title_id=args.title_id)
        rendered = json.dumps(result, indent=2, ensure_ascii=True) + "\n"
        if args.output is None:
            sys.stdout.write(rendered)
        else:
            try:
                with args.output.open("x", encoding="utf-8", newline="\n") as destination:
                    destination.write(rendered)
            except FileExistsError as exc:
                raise InspectionError("Output already exists; choose a new filename.") from exc
            except OSError as exc:
                raise InspectionError("Cannot create output; choose a writable, existing folder.") from exc
    except InspectionError as exc:
        print(f"inspect_game: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
