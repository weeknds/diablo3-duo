"""Intake validation with synthetic NSO headers; no commercial game data."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "inspect_game.py"
SPEC = importlib.util.spec_from_file_location("duo_inspect_game", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
intake = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(intake)


class InspectGameTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.main_file = self.root / "main"
        self.build_id = bytes(range(1, 33))
        self.header = bytearray(0x100)
        self.header[:4] = b"NSO0"
        self.header[0x40:0x60] = self.build_id
        self.main_file.write_bytes(self.header + b"SYNTHETIC BODY - NOT GAME DATA")

    def test_known_build_id_and_sources_remain_unmodified(self):
        original = self.main_file.read_bytes()
        original_mtime = self.main_file.stat().st_mtime_ns
        report = intake.inspect_game(self.main_file, game_version="example-only")
        self.assertEqual(report["main"]["build_id_full"], self.build_id.hex().upper())
        self.assertEqual(report["main"]["build_id_short"], self.build_id[:8].hex().upper())
        self.assertEqual(report["main"]["header_bytes_read"], 256)
        self.assertEqual(report["main"]["filename"], "main")
        self.assertEqual(report["support_state"], "unverified")
        self.assertFalse(report["title_id_verified_against_main"])
        self.assertEqual(report["title_id_source"], "project_default")
        self.assertFalse(report["live_memory_verified"])
        self.assertFalse(report["thor_tested"])
        self.assertNotIn(str(self.root), json.dumps(report))
        self.assertEqual(self.main_file.read_bytes(), original)
        self.assertEqual(self.main_file.stat().st_mtime_ns, original_mtime)

    def test_only_the_header_is_read_and_rom_is_metadata_only(self):
        rom = self.root / "example.nsp"
        rom.write_bytes(b"arbitrary secret bytes which must not be read")
        original_open, original_read = os.open, os.read
        read_count = 0

        def open_main_only(path, flags, *args, **kwargs):
            self.assertEqual(Path(path), self.main_file)
            return original_open(path, flags, *args, **kwargs)

        def bounded_read(descriptor, length):
            nonlocal read_count
            data = original_read(descriptor, length)
            read_count += len(data)
            self.assertLessEqual(read_count, 0x100)
            return data

        with mock.patch.object(intake.os, "open", side_effect=open_main_only), \
             mock.patch.object(intake.os, "read", side_effect=bounded_read), \
             mock.patch.object(Path, "read_bytes", side_effect=AssertionError("full read forbidden")), \
             mock.patch.object(Path, "open", side_effect=AssertionError("ROM open forbidden")):
            report = intake.inspect_game(self.main_file, rom=rom)
        self.assertEqual(read_count, 0x100)
        self.assertEqual(report["rom"], {
            "filename": rom.name, "size_bytes": rom.stat().st_size, "contents_read": False,
        })
        self.assertNotIn("arbitrary secret", json.dumps(report))

    def test_rejects_truncated_wrong_magic_and_zero_id(self):
        bad_magic = bytearray(self.header)
        bad_magic[:4] = b"PFS0"
        zero_id = bytearray(self.header)
        zero_id[0x40:0x60] = bytes(32)
        cases = ((self.header[:0x60], "Truncated"),
                 (bad_magic, "NSO0"), (zero_id, "all-zero"))
        for contents, message in cases:
            with self.subTest(message=message):
                self.main_file.write_bytes(contents)
                with self.assertRaisesRegex(intake.InspectionError, message):
                    intake.inspect_game(self.main_file)

    def test_rejects_directory_missing_input_and_symbolic_link(self):
        for path in (self.root, self.root / "missing"):
            with self.subTest(path=path.name):
                with self.assertRaises(intake.InspectionError):
                    intake.inspect_game(path)
        alias = self.root / "linked-main"
        try:
            alias.symlink_to(self.main_file)
        except (OSError, NotImplementedError):
            self.skipTest("symbolic links unavailable")
        with self.assertRaisesRegex(intake.InspectionError, "symbolic link"):
            intake.inspect_game(alias)
        with self.assertRaisesRegex(intake.InspectionError, "symbolic link"):
            intake.inspect_game(self.main_file, rom=alias)

    def test_user_title_is_normalized_but_never_verified(self):
        report = intake.inspect_game(self.main_file, title_id="01001b300b9be000")
        self.assertEqual(report["title_id"], "01001B300B9BE000")
        self.assertEqual(report["title_id_source"], "user_provided")
        self.assertFalse(report["title_id_verified_against_main"])
        with self.assertRaisesRegex(intake.InspectionError, "16 hexadecimal"):
            intake.inspect_game(self.main_file, title_id="not-an-id")

    def test_cli_stdout_json_and_exclusive_output(self):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = intake.main(["--main", str(self.main_file)])
        self.assertEqual(status, 0)
        self.assertEqual(stderr.getvalue(), "")
        self.assertEqual(json.loads(stdout.getvalue())["support_state"], "unverified")
        output = self.root / "intake.json"
        self.assertEqual(intake.main(["--main", str(self.main_file), "--output", str(output)]), 0)
        preserved = output.read_bytes()
        with contextlib.redirect_stderr(stderr):
            status = intake.main(["--main", str(self.main_file), "--output", str(output)])
        self.assertEqual(status, 2)
        self.assertIn("Output already exists", stderr.getvalue())
        self.assertEqual(output.read_bytes(), preserved)

    def test_cli_failure_is_graceful_and_does_not_create_output(self):
        output = self.root / "must-not-exist.json"
        self.main_file.write_bytes(b"broken")
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = intake.main(["--main", str(self.main_file), "--output", str(output)])
        self.assertEqual(status, 2)
        self.assertEqual(stdout.getvalue(), "")
        self.assertIn("Truncated", stderr.getvalue())
        self.assertNotIn("Traceback", stderr.getvalue())
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
