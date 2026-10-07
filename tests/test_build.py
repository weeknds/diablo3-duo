import importlib.util
import hashlib
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
        shutil.copytree(ROOT / "vendor", self.root / "vendor")
        (self.root / "package/dualscreen").mkdir(parents=True)
        shutil.copyfile(ROOT / "project.json", self.root / "project.json")
        self.manifest = {
            "format": 1, "name": "Static test", "title_id": "01001B300B9BE000",
            "min_runtime": 18, "canvas_w": 1240, "canvas_h": 1080,
            "background": "#FF11100F", "nav": False, "requires_module": False,
            "pages": [self.page("dashboard")],
        }
        self.assets = {}
        self.save()

    @staticmethod
    def page(page_id):
        return {"id": page_id, "title": page_id, "widgets": [
            {"type": "label", "rect": [40, 900, 0, 0], "text": "DESIGN PREVIEW",
             "text_scale": 3, "color": "#FFFFFFFF"},
            {"type": "label", "rect": [40, 960, 0, 0],
             "text": "Live game data is not connected", "text_scale": 3,
             "color": "#FFFFFFFF"},
        ]}

    def save(self):
        (self.root / "package/dualscreen/manifest.json").write_text(json.dumps(self.manifest))
        (self.root / "package-assets.json").write_text(json.dumps(self.assets))

    def asset(self, name="art.png", data=b"synthetic image bytes"):
        relative = "dualscreen/assets/" + name
        path = self.root / "package" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        self.assets[relative] = hashlib.sha256(data).hexdigest()
        self.save()
        return path

    def image(self, **overrides):
        widget = {"type": "image", "rect": [40, 40, 100, 100],
                  "src": "file:assets/art.png"}
        widget.update(overrides)
        self.manifest["pages"][0]["widgets"].insert(0, widget)
        self.save()

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
            project = json.loads((self.root / "project.json").read_text())
            self.assertEqual(identity["version"], project["version"])
            self.assertNotIn("module", identity)

    def test_reproducible_multipage_archive_retains_exact_hashed_assets(self):
        self.asset()
        self.asset("font.mfnt", b"synthetic font metrics")
        self.manifest["font"] = "file:assets/font.mfnt"
        self.manifest["font_atlas"] = "file:assets/art.png"
        self.manifest["pages"].append(self.page("inventory"))
        self.manifest["actions"] = {"open_inventory": {"kind": "page", "page": "inventory"}}
        self.image(id="inventory_tab", on_tap="open_inventory")
        first = builder.build(self.root, self.root / "first").read_bytes()
        second = builder.build(self.root, self.root / "second")
        self.assertEqual(first, second.read_bytes())
        with zipfile.ZipFile(second) as archive:
            self.assertEqual(sorted(archive.namelist()), ["dualscreen/assets/art.png",
                "dualscreen/assets/font.mfnt", "dualscreen/manifest.json", "package.json"])
            self.assertEqual(archive.read("dualscreen/assets/art.png"), b"synthetic image bytes")
            manifest = json.loads(archive.read("dualscreen/manifest.json"))
            self.assertEqual(manifest["actions"]["open_inventory"]["page"], "inventory")

    def test_accepts_four_static_pages(self):
        self.manifest["pages"] = [self.page(name) for name in ("one", "two", "three", "four")]
        self.save()
        builder.validate_development(self.root)

    def test_packages_allowlisted_font_license_and_notice(self):
        self.asset("OFL.txt", b"Font license fixture")
        self.asset("NOTICE.txt", b"Original interface artwork fixture")
        archive_path = builder.build(self.root)
        with zipfile.ZipFile(archive_path) as archive:
            self.assertEqual(archive.read("dualscreen/assets/OFL.txt"), b"Font license fixture")
            self.assertEqual(archive.read("dualscreen/assets/NOTICE.txt"), b"Original interface artwork fixture")

    def test_refuses_oversized_license_text(self):
        self.asset("OFL.txt", b"x" * (128 * 1024 + 1))
        with self.assertRaisesRegex(builder.DevelopmentError, "128 KiB"):
            builder.validate_development(self.root)

    def test_refuses_changed_asset(self):
        path = self.asset()
        path.write_bytes(b"changed after approval")
        with self.assertRaisesRegex(builder.DevelopmentError, "hash|SHA256"):
            builder.validate_development(self.root)

    def test_refuses_missing_asset(self):
        self.asset().unlink()
        with self.assertRaisesRegex(builder.DevelopmentError, "missing"):
            builder.validate_development(self.root)

    def test_refuses_unlisted_asset(self):
        self.asset()
        (self.root / "package/dualscreen/assets/extra.png").write_bytes(b"extra")
        with self.assertRaisesRegex(builder.DevelopmentError, "unlisted"):
            builder.validate_development(self.root)

    def test_refuses_symlink_asset(self):
        path = self.asset()
        path.unlink()
        path.symlink_to(self.root / "project.json")
        with self.assertRaisesRegex(builder.DevelopmentError, "symbolic"):
            builder.validate_development(self.root)

    def test_refuses_symlink_package_directory(self):
        package = self.root / "package"
        package.rename(self.root / "real-package")
        package.symlink_to(self.root / "real-package", target_is_directory=True)
        with self.assertRaisesRegex(builder.DevelopmentError, "symbolic"):
            builder.validate_development(self.root)

    def test_refuses_unsafe_asset_paths_and_types(self):
        for path in ("../escape.png", "/escape.png", "dualscreen/../escape.png",
                     "dualscreen//art.png", "dualscreen/./art.png", "dualscreen\\art.png",
                     "dualscreen/game.nsp", "elsewhere/art.png", "dualscreen/manifest.json"):
            with self.subTest(path=path):
                self.assets = {path: "0" * 64}
                self.save()
                with self.assertRaises(builder.DevelopmentError):
                    builder.validate_development(self.root)

    def test_refuses_missing_or_unsafe_image_references(self):
        self.asset()
        for source in ("file:assets/missing.png", "file:../project.json", "file:/assets/art.png",
                       "file:assets/./art.png", "romfs:/game.png", "module:art", "https://example.com/x.png"):
            with self.subTest(source=source):
                self.manifest["pages"][0] = self.page("dashboard")
                self.image(src=source)
                with self.assertRaises(builder.DevelopmentError):
                    builder.validate_development(self.root)

    def test_refuses_zero_size_image(self):
        self.asset()
        self.image(rect=[40, 40, 0, 100])
        with self.assertRaises(builder.DevelopmentError):
            builder.validate_development(self.root)

    def test_accepts_literal_image_tint(self):
        self.asset()
        self.image(tint="#FFD7B574")
        archive_path = builder.build(self.root)
        with zipfile.ZipFile(archive_path) as archive:
            manifest = json.loads(archive.read("dualscreen/manifest.json"))
            self.assertEqual(manifest["pages"][0]["widgets"][0]["tint"], "#FFD7B574")

    def test_refuses_invalid_image_tint(self):
        self.asset()
        for tint in ("gold", "#D7B574", -1, 0x100000000, True, [255, 255, 255]):
            with self.subTest(tint=tint):
                self.manifest["pages"][0] = self.page("dashboard")
                self.image(tint=tint)
                with self.assertRaises(builder.DevelopmentError):
                    builder.validate_development(self.root)

    def test_refuses_bound_image_tint(self):
        self.asset()
        self.image(tint_bind="memory.health")
        with self.assertRaises(builder.DevelopmentError):
            builder.validate_development(self.root)

    def test_refuses_nonpage_actions(self):
        for kind in ("write", "module", "call", "button", "flag", "sequence"):
            with self.subTest(kind=kind):
                self.manifest["actions"] = {"unsafe": {"kind": kind, "page": "dashboard"}}
                self.save()
                with self.assertRaisesRegex(builder.DevelopmentError, "page"):
                    builder.validate_development(self.root)

    def test_refuses_dangling_action_target(self):
        self.manifest["actions"] = {"open_missing": {"kind": "page", "page": "missing"}}
        self.save()
        with self.assertRaisesRegex(builder.DevelopmentError, "target|page"):
            builder.validate_development(self.root)

    def test_refuses_dangling_tap_action(self):
        self.manifest["pages"][0]["widgets"][0]["on_tap"] = "missing"
        self.save()
        with self.assertRaisesRegex(builder.DevelopmentError, "on_tap|action"):
            builder.validate_development(self.root)

    def test_refuses_duplicate_page_ids(self):
        self.manifest["pages"].append(self.page("dashboard"))
        self.save()
        with self.assertRaisesRegex(builder.DevelopmentError, "unique|Duplicate"):
            builder.validate_development(self.root)

    def test_refuses_reserved_page_id(self):
        self.manifest["pages"][0]["id"] = "@settings"
        self.save()
        with self.assertRaises(builder.DevelopmentError):
            builder.validate_development(self.root)

    def test_refuses_more_than_four_pages(self):
        self.manifest["pages"] = [self.page(str(i)) for i in range(5)]
        self.save()
        with self.assertRaises(builder.DevelopmentError):
            builder.validate_development(self.root)

    def test_requires_visible_status_on_every_page(self):
        for change in ("missing", "transparent"):
            with self.subTest(change=change):
                page = self.page("inventory")
                if change == "missing":
                    page["widgets"].pop()
                else:
                    page["widgets"][0]["color"] = "#00FFFFFF"
                self.manifest["pages"] = [self.page("dashboard"), page]
                self.save()
                with self.assertRaisesRegex(builder.DevelopmentError, "Visible status"):
                    builder.validate_development(self.root)

    def test_refuses_bind_fields(self):
        self.manifest["pages"][0]["widgets"][0]["bind"] = "memory.health"
        self.save()
        with self.assertRaises(builder.DevelopmentError):
            builder.validate_development(self.root)

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

    def test_preserves_static_runtime_guards(self):
        for key, value in (("nav", True), ("requires_module", True), ("min_runtime", 19)):
            with self.subTest(key=key):
                original = self.manifest[key]
                self.manifest[key] = value
                self.save()
                with self.assertRaises(builder.DevelopmentError):
                    builder.validate_development(self.root)
                self.manifest[key] = original


if __name__ == "__main__":
    unittest.main()
