#!/usr/bin/env python3
"""Read bounded PFS0 directory and advisory *.cnmt.xml metadata from a local NSP.

No other payloads are read, extracted, decrypted or authenticated. In particular,
ticket, certificate and NCA contents are never read. Embedded XML cannot verify
the game identity, installed display version, executable build or compatibility.
Use a private copy of your own file. No third-party dependencies are required.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import stat
import struct
import sys
import tempfile
from typing import Any
from xml.parsers import expat


# PFS0 layout: https://github.com/SciresM/hactool/blob/master/pfs0.h
# CNMT XML fields: https://github.com/DarkMatterCore/nxdumptool/blob/rewrite/source/core/cnmt.c
HEADER = struct.Struct("<4sIII")
ENTRY = struct.Struct("<QQII")
MAX_ENTRIES = 1024
MAX_STRING_BYTES = 256 * 1024
MAX_XML_BYTES = 64 * 1024
MAX_XML_TOTAL_BYTES = 256 * 1024
MAX_XML_FILES = 16
MAX_NAME_BYTES = 255
MAX_XML_DEPTH = 32
MAX_XML_ELEMENTS = 4096
CNMT_FIELDS = {"Type": "type", "Id": "title_id", "Version": "content_version", "PatchId": "patch_id"}
CNMT_TYPES = frozenset({"SystemProgram", "SystemData", "SystemUpdate", "BootImagePackage",
                        "BootImagePackageSafe", "Application", "Patch", "AddOnContent", "Delta", "DataPatch"})


class InspectionError(ValueError):
    """The input cannot safely provide the limited advisory metadata requested."""


def _stamp(info: os.stat_result) -> tuple[int, ...]:
    return (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns, info.st_ctime_ns)


def _read_exact(descriptor: int, offset: int, size: int) -> bytes:
    """Read only a previously bounded range, without buffered read-ahead."""
    os.lseek(descriptor, offset, os.SEEK_SET)
    result = bytearray()
    while len(result) < size:
        block = os.read(descriptor, min(size - len(result), 64 * 1024))
        if not block:
            raise InspectionError("Truncated NSP: a declared metadata range is incomplete.")
        result.extend(block)
    return bytes(result)


def _entry_name(strings: bytes, offset: int) -> str:
    if offset >= len(strings) or (offset and strings[offset - 1] != 0):
        raise InspectionError("Invalid PFS0 name offset.")
    end = strings.find(b"\0", offset, offset + MAX_NAME_BYTES + 1)
    if end < 0:
        raise InspectionError("PFS0 entry name is unterminated or too long.")
    raw = strings[offset:end]
    # Deliberately accept only uncomplicated ASCII basenames; never normalize a
    # traversal path or control sequence into an apparently trustworthy name.
    if not re.fullmatch(rb"[A-Za-z0-9][A-Za-z0-9._ -]*", raw) or raw.endswith((b" ", b".")):
        raise InspectionError("PFS0 entry name must be a safe ASCII basename.")
    return raw.decode("ascii")


def _directory(descriptor: int, file_size: int) -> tuple[int, list[tuple[str, int, int]]]:
    if file_size < HEADER.size:
        raise InspectionError("Truncated NSP: a PFS0 header needs 16 bytes.")
    magic, count, string_size, reserved = HEADER.unpack(_read_exact(descriptor, 0, HEADER.size))
    if magic != b"PFS0":
        raise InspectionError("Wrong file format: expected a PFS0 NSP container.")
    if reserved or not 1 <= count <= MAX_ENTRIES or not 1 <= string_size <= MAX_STRING_BYTES:
        raise InspectionError("Invalid or excessive PFS0 directory dimensions or reserved field.")
    table_size = count * ENTRY.size
    data_start = HEADER.size + table_size + string_size
    if data_start > file_size:
        raise InspectionError("Truncated NSP: the directory exceeds the file.")
    table = _read_exact(descriptor, HEADER.size, table_size)
    strings = _read_exact(descriptor, HEADER.size + table_size, string_size)
    data_size = file_size - data_start
    entries, names, ranges = [], set(), []
    xml_count = xml_total = 0
    for index in range(count):
        offset, size, name_offset, reserved = ENTRY.unpack_from(table, index * ENTRY.size)
        if reserved or offset > data_size or size > data_size - offset:
            raise InspectionError("Invalid PFS0 entry range or reserved field.")
        name = _entry_name(strings, name_offset)
        if name.casefold() in names:
            raise InspectionError("Duplicate or case-ambiguous PFS0 entry names.")
        names.add(name.casefold())
        entries.append((name, offset, size))
        if size:
            ranges.append((offset, offset + size))
        if name.lower().endswith(".cnmt.xml"):
            xml_count += 1
            xml_total += size
            if not 0 < size <= MAX_XML_BYTES:
                raise InspectionError("CNMT XML is empty or exceeds the 64 KiB per-file limit.")
    previous_end = 0
    for start, end in sorted(ranges):
        if start < previous_end:
            raise InspectionError("Overlapping PFS0 entry payload ranges.")
        previous_end = end
    if xml_count > MAX_XML_FILES or xml_total > MAX_XML_TOTAL_BYTES:
        raise InspectionError("CNMT XML exceeds the file-count or total-byte limit.")
    # Every entry is checked before any payload is read, including XML.
    return data_start, entries


def _cnmt_fields(contents: bytes) -> dict[str, Any]:
    """Parse untrusted XML with no DTD/entities and retain only root scalar fields."""
    parser = expat.ParserCreate()
    stack: list[str] = []
    fields: dict[str, list[str]] = {}
    elements = 0

    def forbidden(*_args: Any) -> None:
        raise InspectionError("DTD and entity declarations are not permitted in CNMT XML.")

    def start(name: str, attributes: dict[str, str]) -> None:
        nonlocal elements
        elements += 1
        if not stack and name != "ContentMeta":
            raise InspectionError("CNMT XML must have a ContentMeta root.")
        if len(stack) == 2 and stack[1] in CNMT_FIELDS:
            raise InspectionError("CNMT allowlisted fields must be plain scalar values.")
        stack.append(name)
        if len(stack) > MAX_XML_DEPTH or elements > MAX_XML_ELEMENTS:
            raise InspectionError("CNMT XML exceeds the structural limit.")
        if len(stack) == 2 and name in CNMT_FIELDS:
            if name in fields or attributes:
                raise InspectionError("CNMT allowlisted fields must be unique and have no attributes.")
            fields[name] = []

    def characters(value: str) -> None:
        if len(stack) == 2 and stack[1] in CNMT_FIELDS:
            fields[stack[1]].append(value)

    parser.StartDoctypeDeclHandler = forbidden
    parser.EntityDeclHandler = forbidden
    parser.ExternalEntityRefHandler = forbidden
    parser.StartElementHandler = start
    parser.EndElementHandler = lambda _name: stack.pop()
    parser.CharacterDataHandler = characters
    try:
        parser.Parse(contents, True)
    except InspectionError:
        raise
    except (expat.ExpatError, LookupError, ValueError):
        # Expat's Python codec bridge can also fail outside ExpatError. Never
        # expose an untrusted encoding declaration through its exception text.
        raise InspectionError("Malformed CNMT XML or unsupported encoding.") from None

    result: dict[str, Any] = dict.fromkeys(CNMT_FIELDS.values())
    for tag, chunks in fields.items():
        value = "".join(chunks).strip()
        if tag == "Type":
            if value not in CNMT_TYPES:
                raise InspectionError("Invalid CNMT content type.")
            result["type"] = value
        elif tag == "Version":
            if not re.fullmatch(r"[0-9]{1,10}", value) or int(value) > 0xFFFFFFFF:
                raise InspectionError("Invalid CNMT numeric content version.")
            result["content_version"] = int(value)
        else:
            if not re.fullmatch(r"(?:0[xX])?[0-9a-fA-F]{16}", value):
                raise InspectionError("Invalid CNMT title or patch ID.")
            result[CNMT_FIELDS[tag]] = value[-16:].upper()
    return result


def inspect_nsp(path: Path) -> dict[str, Any]:
    """Inspect one regular local file; never infer executable or device support."""
    path = Path(path)
    descriptor = None
    try:
        before = path.lstat()
        if stat.S_ISLNK(before.st_mode):
            raise InspectionError("The NSP input must not be a symbolic link.")
        if not stat.S_ISREG(before.st_mode):
            raise InspectionError("The NSP input must be a regular file.")
        if not hasattr(os, "O_NOFOLLOW"):
            raise InspectionError("This platform does not support safe no-follow input opening.")
        # NONBLOCK also prevents a replacement FIFO from blocking open().
        flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | getattr(os, "O_BINARY", 0)
        descriptor = os.open(path, flags)
        opened = os.fstat(descriptor)
        if not stat.S_ISREG(opened.st_mode) or _stamp(before) != _stamp(opened):
            raise InspectionError("The NSP input changed while opening it.")
        data_start, entries = _directory(descriptor, opened.st_size)
        advisory = []
        for name, offset, size in entries:
            if name.lower().endswith(".cnmt.xml"):
                advisory.append({"filename": name, **_cnmt_fields(
                    _read_exact(descriptor, data_start + offset, size))})
        if _stamp(os.fstat(descriptor)) != _stamp(opened):
            raise InspectionError("The NSP input changed during inspection.")
        return {
            "schema_version": 1,
            "support_state": "unverified",
            "metadata_trust": "unauthenticated_advisory",
            "metadata_note": "Embedded XML does not verify identity, installed version or compatibility. "
                             "Numeric content version is not the installed display version.",
            "nsp": {"filename": re.sub(r"[^A-Za-z0-9._ ()\[\]-]", "_", path.name)[:255],
                    "size_bytes": opened.st_size, "format": "PFS0"},
            "entries": [{"filename": name, "size_bytes": size} for name, _offset, size in entries],
            "advisory_cnmt": advisory,
            "build_id_full": None,
            "installed_display_version": None,
            "title_id_verified": False,
            "live_memory_verified": False,
            "thor_tested": False,
        }
    except OSError as exc:
        raise InspectionError("Cannot safely access or read the NSP input.") from exc
    finally:
        if descriptor is not None:
            os.close(descriptor)


def _write_report(path: Path, rendered: str) -> None:
    """Publish a complete report exclusively, without ever deleting the target."""
    try:
        # A private directory on the destination filesystem keeps incomplete
        # output inaccessible. Its cleanup cannot remove a competing report.
        with tempfile.TemporaryDirectory(prefix=".inspect-nsp-", dir=path.parent) as temporary:
            staged = Path(temporary) / "report.json"
            with staged.open("x", encoding="utf-8", newline="\n") as destination:
                os.fchmod(destination.fileno(), 0o600)
                destination.write(rendered)
                destination.flush()
                os.fsync(destination.fileno())
            # link() creates only a new name: unlike replace(), it cannot
            # overwrite a file or symlink another process created meanwhile.
            os.link(staged, path, follow_symlinks=False)
    except FileExistsError:
        raise InspectionError("Output already exists; choose a new filename.") from None
    except OSError:
        raise InspectionError("Cannot create complete output; choose a writable, existing folder.") from None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nsp", required=True, type=Path, help="local PFS0 NSP; only directory and bounded CNMT XML are read")
    parser.add_argument("--output", type=Path, help="create a new JSON report; never overwrite an existing file")
    args = parser.parse_args(argv)
    try:
        rendered = json.dumps(inspect_nsp(args.nsp), indent=2, ensure_ascii=True) + "\n"
        if args.output is None:
            sys.stdout.write(rendered)
        else:
            _write_report(args.output, rendered)
    except InspectionError as exc:
        print(f"inspect_nsp: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
