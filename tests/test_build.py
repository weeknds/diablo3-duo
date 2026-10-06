import importlib.util
import json
import shutil
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("development_builder", ROOT / "tools/build.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class DevelopmentPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / "project"
        self.root.mkdir()
        for name in ("package", "vendor"):
            shutil.copytree(ROOT / name, self.root / name)
        shutil.copyfile(ROOT / "project.json", self.root / "project.json")

    def tearDown(self):
        self.temp.cleanup()

    def test_reproducible_archive_contains_only_static_manifest(self):
        first = builder.build(self.root, self.root / "first").read_bytes()
        second_path = builder.build(self.root, self.root / "second")
        self.assertEqual(first, second_path.read_bytes())
        with zipfile.ZipFile(second_path) as archive:
            self.assertEqual(sorted(archive.namelist()), ["dualscreen/manifest.json", "package.json"])
            identity = json.loads(archive.read("package.json"))
            self.assertEqual(identity["title_id"], "01001B300B9BE000")
            self.assertEqual(identity["version"], "0.1.0-dev")
            self.assertNotIn("module", identity)

    def test_refuses_game_file_added_to_package(self):
        (self.root / "package/dualscreen/game.nsp").write_bytes(b"private test content")
        with self.assertRaises(builder.DevelopmentError):
            builder.build(self.root)

    def test_refuses_unverified_memory_action(self):
        path = self.root / "package/dualscreen/manifest.json"
        manifest = json.loads(path.read_text())
        manifest["actions"] = {"fake": {"kind": "write", "address": "0x123456"}}
        path.write_text(json.dumps(manifest))
        with self.assertRaises(builder.DevelopmentError):
            builder.build(self.root)

    def test_refuses_claimed_live_support(self):
        path = self.root / "project.json"
        project = json.loads(path.read_text())
        project["supported_builds"] = ["UNVERIFIED"]
        path.write_text(json.dumps(project))
        with self.assertRaises(builder.DevelopmentError):
            builder.build(self.root)


if __name__ == "__main__":
    unittest.main()
