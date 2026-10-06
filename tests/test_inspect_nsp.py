"""Read-only NSP intake tests; all PFS0 and XML data below are synthetic."""

from __future__ import annotations

import contextlib
import errno
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "inspect_nsp.py"
XML = (b'<ContentMeta><Type>Application</Type><Id>0x010000000000abcd</Id>'
       b'<Version>65536</Version><PatchId>0x0100000000000800</PatchId>'
       b'<Content><Type>Program</Type><Id>PRIVATE-NESTED-TEXT</Id></Content>'
       b'<Digest>PRIVATE-DIGEST</Digest></ContentMeta>')


def pfs0(entries):
    """Build a conventional synthetic container and independently track ranges."""
    strings, table, payload = bytearray(), bytearray(), bytearray()
    ranges = []
    for name, contents in entries:
        encoded = name.encode("utf-8") if isinstance(name, str) else name
        table += struct.pack("<QQII", len(payload), len(contents), len(strings), 0)
        strings += encoded + b"\0"
        ranges.append((len(payload), len(payload) + len(contents)))
        payload += contents
    directory = struct.pack("<4sIII", b"PFS0", len(entries), len(strings), 0) + table + strings
    return bytes(directory + payload), [(a + len(directory), b + len(directory)) for a, b in ranges]


class InspectNspTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SCRIPT.is_file(), "the separate NSP metadata inspector is not implemented")
        spec = importlib.util.spec_from_file_location("duo_inspect_nsp", SCRIPT)
        self.intake = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.intake)
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.nsp = self.root / "synthetic.nsp"
        self.entries = [("secret.tik", b"TICKET-NEVER-READ"),
                        ("secret.cert", b"CERTIFICATE-NEVER-READ"),
                        ("secret.nca", b"NCA-NEVER-READ" * 100),
                        ("example.cnmt.xml", XML),
                        ("other.xml", b"OTHER-XML-NEVER-READ")]
        self.contents, self.ranges = pfs0(self.entries)
        self.nsp.write_bytes(self.contents)

    def cli(self, *arguments):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            status = self.intake.main(["--nsp", str(self.nsp), *arguments])
        return status, stdout.getvalue(), stderr.getvalue()

    def reject(self, contents):
        self.nsp.write_bytes(contents)
        with self.assertRaises(self.intake.InspectionError):
            self.intake.inspect_nsp(self.nsp)

    def test_returns_only_allowlisted_advisory_metadata(self):
        report = self.intake.inspect_nsp(self.nsp)
        self.assertEqual(report["nsp"], {"filename": "synthetic.nsp", "size_bytes": len(self.contents),
                                         "format": "PFS0"})
        self.assertEqual(report["entries"], [
            {"filename": name, "size_bytes": len(contents)} for name, contents in self.entries])
        self.assertEqual(report["advisory_cnmt"], [{
            "filename": "example.cnmt.xml", "type": "Application",
            "title_id": "010000000000ABCD", "content_version": 65536,
            "patch_id": "0100000000000800"}])
        self.assertEqual(report["metadata_trust"], "unauthenticated_advisory")
        self.assertEqual(report["support_state"], "unverified")
        self.assertIsNone(report["build_id_full"])
        self.assertIsNone(report["installed_display_version"])
        self.assertFalse(report["title_id_verified"])
        self.assertFalse(report["live_memory_verified"])
        self.assertFalse(report["thor_tested"])
        rendered = json.dumps(report)
        for secret in (str(self.root), "PRIVATE", "NEVER-READ", "<ContentMeta>"):
            self.assertNotIn(secret, rendered)

    def test_reads_only_directory_and_selected_xml_with_short_reads(self):
        original_read = os.read
        directory_end = self.ranges[0][0]
        xml_start, xml_end = self.ranges[3]
        bytes_read = 0

        def checked_read(descriptor, size):
            nonlocal bytes_read
            start = os.lseek(descriptor, 0, os.SEEK_CUR)
            self.assertTrue((0 <= start <= start + size <= directory_end) or
                            (xml_start <= start <= start + size <= xml_end),
                            "inspector attempted to read a forbidden payload")
            result = original_read(descriptor, min(size, 7))
            bytes_read += len(result)
            return result

        with mock.patch.object(self.intake.os, "read", side_effect=checked_read):
            report = self.intake.inspect_nsp(self.nsp)
        self.assertEqual(bytes_read, directory_end + len(XML))
        self.assertEqual(report["advisory_cnmt"][0]["content_version"], 65536)

    def test_source_bytes_and_mtime_remain_unchanged(self):
        before = self.nsp.stat()
        self.intake.inspect_nsp(self.nsp)
        self.assertEqual(self.nsp.read_bytes(), self.contents)
        self.assertEqual(self.nsp.stat().st_mtime_ns, before.st_mtime_ns)

    def test_no_xml_and_missing_fields_remain_unavailable(self):
        for entries, expected in (([("only.nca", b"not read")], []),
                                  ([("empty.cnmt.xml", b"<ContentMeta/>")], [{
                                      "filename": "empty.cnmt.xml", "type": None,
                                      "title_id": None, "content_version": None, "patch_id": None}])):
            with self.subTest(entries=entries):
                self.nsp.write_bytes(pfs0(entries)[0])
                self.assertEqual(self.intake.inspect_nsp(self.nsp)["advisory_cnmt"], expected)

    def test_rejects_bad_magic_truncated_header_and_directory(self):
        for contents in (b"", b"PFS0", b"XCI0" + self.contents[4:], self.contents[:30]):
            with self.subTest(size=len(contents)):
                self.reject(contents)

    def test_rejects_unbounded_entry_and_string_counts_before_table_read(self):
        original_read = os.read
        for count, strings in ((0xFFFFFFFF, 1), (1, 0xFFFFFFFF)):
            self.nsp.write_bytes(struct.pack("<4sIII", b"PFS0", count, strings, 0) + bytes(128))

            def header_only(descriptor, size):
                self.assertLessEqual(os.lseek(descriptor, 0, os.SEEK_CUR) + size, 16)
                return original_read(descriptor, size)

            with self.subTest(count=count, strings=strings), \
                 mock.patch.object(self.intake.os, "read", side_effect=header_only):
                with self.assertRaises(self.intake.InspectionError):
                    self.intake.inspect_nsp(self.nsp)

    def test_rejects_reserved_header_and_entry_fields(self):
        for position in (12, 36):
            contents = bytearray(self.contents)
            struct.pack_into("<I", contents, position, 1)
            with self.subTest(position=position):
                self.reject(contents)

    def test_rejects_out_of_bounds_and_overlapping_payload_ranges(self):
        for offset, size in ((len(self.contents), 1), (0, 2**64 - 1),
                             (2**64 - 1, 2), (0, 16)):
            contents = bytearray(self.contents)
            struct.pack_into("<QQ", contents, 40, offset, size)
            with self.subTest(offset=offset, size=size):
                self.reject(contents)

    def test_validates_all_ranges_before_any_xml_read(self):
        contents, ranges = pfs0([("first.cnmt.xml", XML), ("bad.nca", b"body")])
        contents = bytearray(contents)
        struct.pack_into("<Q", contents, 40, 0)
        self.nsp.write_bytes(contents)
        original_read = os.read

        def directory_only(descriptor, size):
            self.assertLessEqual(os.lseek(descriptor, 0, os.SEEK_CUR) + size, ranges[0][0])
            return original_read(descriptor, size)

        with mock.patch.object(self.intake.os, "read", side_effect=directory_only):
            with self.assertRaises(self.intake.InspectionError):
                self.intake.inspect_nsp(self.nsp)

    def test_rejects_unsafe_and_duplicate_names(self):
        for name in ("../secret.nca", "/secret.nca", "dir/secret.nca", "dir\\secret.nca",
                     "..", "", "line\nname.nca", "\x1b[31m.nca", "a" * 256, b"\xff.nca"):
            with self.subTest(name=repr(name)):
                self.reject(pfs0([(name, b"body")])[0])
        for names in (("same.nca", "same.nca"), ("SAME.nca", "same.nca")):
            with self.subTest(names=names):
                self.reject(pfs0([(name, b"body") for name in names])[0])

    def test_rejects_unterminated_out_of_bounds_and_mid_string_names(self):
        base, _ = pfs0([("example.nca", b"body")])
        unterminated = bytearray(base)
        unterminated[51] = ord("x")
        self.reject(unterminated)
        for string_offset in (1, 12, 0xFFFFFFFF):
            contents = bytearray(base)
            struct.pack_into("<I", contents, 32, string_offset)
            with self.subTest(string_offset=string_offset):
                self.reject(contents)

    def test_rejects_oversized_xml_before_payload_read(self):
        contents, ranges = pfs0([("large.cnmt.xml", b" " * (64 * 1024 + 1))])
        self.nsp.write_bytes(contents)
        original_read = os.read

        def directory_only(descriptor, size):
            self.assertLessEqual(os.lseek(descriptor, 0, os.SEEK_CUR) + size, ranges[0][0])
            return original_read(descriptor, size)

        with mock.patch.object(self.intake.os, "read", side_effect=directory_only):
            with self.assertRaises(self.intake.InspectionError):
                self.intake.inspect_nsp(self.nsp)

    def test_rejects_excessive_xml_total_and_count(self):
        valid_large_xml = b"<ContentMeta/>" + b" " * (60000 - 14)
        for entries in ([(f"{i}.cnmt.xml", valid_large_xml) for i in range(5)],
                        [(f"{i}.cnmt.xml", b"<ContentMeta/>") for i in range(17)]):
            with self.subTest(count=len(entries)):
                self.reject(pfs0(entries)[0])

    def test_rejects_dtd_entities_and_malformed_xml(self):
        documents = [b"<ContentMeta>", b"<Other/>",
                     b'<!DOCTYPE ContentMeta [<!ENTITY x "SECRET">]><ContentMeta><Type>&x;</Type></ContentMeta>',
                     b'<!DOCTYPE ContentMeta SYSTEM "file:///private/secret"><ContentMeta/>',
                     b'<!DOCTYPE ContentMeta><ContentMeta/>',
                     b'<ContentMeta><Type>&unknown;</Type></ContentMeta>',
                     '<!DOCTYPE ContentMeta [<!ENTITY x "SECRET">]><ContentMeta/>'.encode("utf-16")]
        for xml in documents:
            with self.subTest(xml=repr(xml[:80])):
                self.reject(pfs0([("bad.cnmt.xml", xml)])[0])

    def test_rejects_invalid_duplicate_or_nested_allowlisted_fields(self):
        for field in ("<Type>UNTRUSTED-TEXT</Type>", "<Id>garbage</Id>",
                      "<Version>-1</Version>", "<Version>4294967296</Version>",
                      "<Version>1.2.3</Version>", "<Version>1e6</Version>",
                      "<PatchId>/private/path</PatchId>", "<Id/><Id/>",
                      "<Type><Part>Application</Part></Type>",
                      '<Type source="Application">Application</Type>'):
            with self.subTest(field=field):
                self.reject(pfs0([("bad.cnmt.xml", f"<ContentMeta>{field}</ContentMeta>".encode())])[0])

    def test_rejects_excessive_xml_depth(self):
        xml = b"<ContentMeta>" + b"<X>" * 40 + b"</X>" * 40 + b"</ContentMeta>"
        self.reject(pfs0([("deep.cnmt.xml", xml)])[0])

    def test_rejects_missing_directory_symlink_and_fifo_inputs(self):
        paths = [self.root, self.root / "missing"]
        alias = self.root / "alias.nsp"
        alias.symlink_to(self.nsp)
        paths.append(alias)
        if hasattr(os, "mkfifo"):
            fifo = self.root / "pipe.nsp"
            os.mkfifo(fifo)
            paths.append(fifo)
        for path in paths:
            with self.subTest(path=path.name):
                with self.assertRaises(self.intake.InspectionError):
                    self.intake.inspect_nsp(path)

    def test_rejects_file_replaced_between_stat_and_open(self):
        replacement = self.root / "replacement.nsp"
        replacement.write_bytes(self.contents)
        original_open = os.open

        def replaced_open(path, flags, *args, **kwargs):
            os.replace(replacement, self.nsp)
            return original_open(path, flags, *args, **kwargs)

        with mock.patch.object(self.intake.os, "open", side_effect=replaced_open):
            with self.assertRaises(self.intake.InspectionError):
                self.intake.inspect_nsp(self.nsp)

    def test_rejects_source_changed_during_inspection(self):
        original_read = os.read
        changed = False

        def changing_read(descriptor, size):
            nonlocal changed
            result = original_read(descriptor, size)
            if not changed:
                changed = True
                with self.nsp.open("ab") as destination:
                    destination.write(b"concurrent edit")
            return result

        with mock.patch.object(self.intake.os, "read", side_effect=changing_read):
            with self.assertRaises(self.intake.InspectionError):
                self.intake.inspect_nsp(self.nsp)

    def test_cli_emits_json_or_exclusively_creates_output(self):
        status, stdout, stderr = self.cli()
        self.assertEqual((status, stderr), (0, ""))
        self.assertEqual(json.loads(stdout)["support_state"], "unverified")
        output = self.root / "report.json"
        self.assertEqual(self.cli("--output", str(output)), (0, "", ""))
        original = output.read_bytes()
        status, stdout, stderr = self.cli("--output", str(output))
        self.assertEqual((status, stdout), (2, ""))
        self.assertIn("already exists", stderr)
        self.assertEqual(output.read_bytes(), original)

    def test_cli_cannot_overwrite_input_or_output_symlink(self):
        alias = self.root / "report.json"
        alias.symlink_to(self.nsp)
        for output in (self.nsp, alias):
            with self.subTest(output=output.name):
                self.assertEqual(self.cli("--output", str(output))[0], 2)
                self.assertEqual(self.nsp.read_bytes(), self.contents)

    def test_invalid_input_produces_no_json_or_output_and_no_private_text(self):
        xml = b"<ContentMeta><Id>/secret/home/PRIVATE-VALUE</Id></ContentMeta>"
        self.nsp.write_bytes(pfs0([("bad.cnmt.xml", xml)])[0])
        output = self.root / "must-not-exist.json"
        status, stdout, stderr = self.cli("--output", str(output))
        self.assertEqual((status, stdout), (2, ""))
        for value in (str(self.root), "PRIVATE-VALUE", "Traceback"):
            self.assertNotIn(value, stderr)
        self.assertFalse(output.exists())

    def test_cli_rejects_unsupported_xml_encoding_without_leaking_it(self):
        for encoding in ("PRIVATE-ENCODING", "utf-32"):
            with self.subTest(encoding=encoding):
                xml = f'<?xml version="1.0" encoding="{encoding}"?><ContentMeta/>'.encode()
                self.nsp.write_bytes(pfs0([("bad.cnmt.xml", xml)])[0])
                output = self.root / "must-not-exist.json"
                result = subprocess.run([sys.executable, str(SCRIPT), "--nsp", str(self.nsp),
                                         "--output", str(output)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertNotIn(encoding, result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(output.exists())

    def test_partial_output_write_failure_leaves_no_report_or_temporary_file(self):
        output = self.root / "report.json"
        original_open = io.open

        class FailAfterPartialWrite:
            def __init__(self, stream):
                self.stream = stream

            def __enter__(self):
                return self

            def __exit__(self, *args):
                return self.stream.__exit__(*args)

            def __getattr__(self, name):
                return getattr(self.stream, name)

            def write(self, value):
                self.stream.write(value[:40])
                self.stream.flush()
                raise OSError(errno.ENOSPC, "simulated disk full")

        def fail_writing(file, mode="r", *args, **kwargs):
            stream = original_open(file, mode, *args, **kwargs)
            return FailAfterPartialWrite(stream) if "x" in mode or "w" in mode else stream

        before = self.nsp.stat()
        with mock.patch.object(io, "open", side_effect=fail_writing):
            status, stdout, stderr = self.cli("--output", str(output))
        self.assertEqual((status, stdout), (2, ""))
        self.assertNotIn("Traceback", stderr)
        self.assertFalse(output.exists())
        self.assertEqual(set(self.root.iterdir()), {self.nsp})
        self.assertEqual(self.nsp.read_bytes(), self.contents)
        self.assertEqual(self.nsp.stat().st_mtime_ns, before.st_mtime_ns)

    def test_concurrent_output_creation_is_preserved_when_report_publishes(self):
        output = self.root / "report.json"
        original_link = os.link

        def create_competing_report(source, destination, *args, **kwargs):
            output.write_bytes(b"a different process owns this report")
            return original_link(source, destination, *args, **kwargs)

        with mock.patch.object(self.intake.os, "link", side_effect=create_competing_report):
            status, stdout, stderr = self.cli("--output", str(output))
        self.assertEqual((status, stdout), (2, ""))
        self.assertIn("already exists", stderr)
        self.assertEqual(output.read_bytes(), b"a different process owns this report")
        self.assertEqual(set(self.root.iterdir()), {self.nsp, output})
        self.assertEqual(self.nsp.read_bytes(), self.contents)

    def test_publication_failure_leaves_no_report_or_temporary_file(self):
        output = self.root / "report.json"
        with mock.patch.object(self.intake.os, "link", side_effect=OSError(errno.EIO, "simulated I/O failure")):
            status, stdout, stderr = self.cli("--output", str(output))
        self.assertEqual((status, stdout), (2, ""))
        self.assertNotIn("Traceback", stderr)
        self.assertFalse(output.exists())
        self.assertEqual(set(self.root.iterdir()), {self.nsp})
        self.assertEqual(self.nsp.read_bytes(), self.contents)


if __name__ == "__main__":
    unittest.main()
