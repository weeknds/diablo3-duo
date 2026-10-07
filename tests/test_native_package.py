"""Behavioral contract for the live skin and native package boundary."""
import copy
import importlib.util
import json
import struct
import shutil
import tempfile
from pathlib import Path
import unittest

REPO = Path(__file__).resolve().parents[1]
ROOT = REPO / 'native'
spec = importlib.util.spec_from_file_location('live_ui', ROOT / 'live_ui.py')
adapter = importlib.util.module_from_spec(spec) if spec and spec.loader and (ROOT / 'live_ui.py').exists() else None
if adapter:
    spec.loader.exec_module(adapter)


class LiveUiTests(unittest.TestCase):
    def manifest(self):
        self.assertIsNotNone(adapter, 'Live adapter must connect the reviewed UI to the real reader')
        return adapter.make_manifest()

    def test_packaged_navigation_reaches_only_working_pages(self):
        m = self.manifest()
        self.assertEqual({p['id'] for p in m['pages']}, {'character', 'combat'})
        self.assertEqual({a['page'] for a in m['actions'].values()}, {'character', 'combat'})
        for p in m['pages']:
            for w in p['widgets']:
                if 'on_tap' in w:
                    self.assertIn(w['on_tap'], m['actions'])

    def test_each_live_field_is_gated_and_has_an_offline_fallback(self):
        m = self.manifest()
        for p in m['pages']:
            for w in p['widgets']:
                if 'bind_text' not in w:
                    continue
                self.assertEqual(w['need_bind'], 'module_ready')
                self.assertEqual(w['hide_bind'], 'module_error')
                self.assertEqual(w['hide_eq'], 1)
                fallbacks = [x for x in p['widgets'] if x.get('id') == w.get('id') + '_offline']
                self.assertEqual(len(fallbacks), 1)
                self.assertEqual(fallbacks[0]['need_bind'], '!module_ready')

    def test_ui_contract_uses_native_keys_and_level_is_a_full_label(self):
        m = self.manifest()
        widgets = m['pages'][0]['widgets']
        expected = {'level':'details.level','aps':'stats.0.value','cdr':'stats.1.value','armor':'stats.2.value','movement':'stats.3.value'}
        for ident, key in expected.items():
            w = next(w for w in widgets if w.get('id') == ident)
            self.assertEqual(w['bind_text'], key)
        self.assertGreaterEqual(next(w for w in widgets if w.get('id') == 'level')['wrap_width'], 320)
        self.assertFalse(any(w.get('text') == 'Level' for w in widgets))

    def test_exact_identity_and_read_only_lifecycle_survive_skin_change(self):
        m = self.manifest()
        self.assertTrue(m['requires_module'])
        self.assertFalse(m['module_tick_hidden'])
        self.assertFalse(m['nav'])
        self.assertEqual(m['module']['build_ids'], ['2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000'])
        self.assertFalse({'writes','patches','spies','calls','points'} & set(m))
        self.assertTrue(all(a['kind'] == 'page' for a in m['actions'].values()))

    def test_all_skill_slots_are_live_on_both_pages(self):
        m = self.manifest()
        for p in m['pages']:
            bindings = {w.get('bind_text') for w in p['widgets']}
            self.assertTrue({f'skills.{i}.name' for i in range(6)} <= bindings)
            self.assertTrue(any(w.get('need_bind') == 'module_error' for w in p['widgets']))

    def test_native_numeric_buffer_cannot_be_truncated_into_a_different_value(self):
        m = self.manifest()
        metrics = (REPO / 'package/dualscreen/assets/duo-sans.mfnt').read_bytes()
        cap = struct.unpack_from('<I', metrics, 0x1C)[0]
        count = struct.unpack_from('<I', metrics, 0x20)[0]
        start = struct.unpack_from('<I', metrics, 0x28)[0]
        glyphs = {i+31:struct.unpack_from('<4H2hH', metrics, start+14*i) for i in range(1,count)}
        for w in m['pages'][0]['widgets']:
            if w.get('id') not in ('aps','cdr','armor','movement'):
                continue
            for value in ('100000000000000000000.00', '888888888888888888888888888', '+88888888888888888888888.88%'):
                # Native MFNT labels use five pixels per scale unit divided by cap height.
                # Every complete numeric buffer must fit at an allowed scale; ellipses
                # would otherwise turn a large correct value into misleading digits.
                advance = sum(glyphs[ord(c)][-1] for c in value)
                self.assertLessEqual(advance * w['text_min_scale'] * 5, w['wrap_width'] * cap,
                                     (w['id'], value))
                self.assertTrue(w['fit_text'])



class NativeBuildBoundaryTests(unittest.TestCase):
    def setUp(self):
        path = ROOT / 'build.py'
        self.assertTrue(path.is_file(), 'Public native builder must validate its inputs')
        spec = importlib.util.spec_from_file_location('native_build', path)
        self.builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.builder)
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.temp = Path(self.temporary.name).resolve()

    def copy_source(self):
        target = self.temp / 'native'
        shutil.copytree(ROOT, target, ignore=shutil.ignore_patterns('__pycache__'))
        return target

    def test_pinned_source_inventory_accepts_preserved_reader(self):
        self.builder.validate_sources(ROOT)

    def test_source_mutation_is_rejected(self):
        target = self.copy_source()
        (target / 'src/module.c').write_text('int replaced;')
        with self.assertRaisesRegex(ValueError, 'hash'):
            self.builder.validate_sources(target)

    def test_extra_source_is_rejected_even_if_added_to_inventory(self):
        target = self.copy_source()
        (target / 'src/unreviewed.c').write_text('int extra;')
        with self.assertRaises(ValueError):
            self.builder.validate_sources(target)
        inventory = json.loads((target / 'SOURCES.json').read_text())
        inventory['src/unreviewed.c'] = '0' * 64
        (target / 'SOURCES.json').write_text(json.dumps(inventory))
        with self.assertRaises(ValueError):
            self.builder.validate_sources(target)

    def test_missing_source_is_rejected(self):
        target = self.copy_source()
        (target / 'src/module.c').unlink()
        with self.assertRaises(ValueError):
            self.builder.validate_sources(target)

    def test_source_symlink_is_rejected(self):
        target = self.copy_source()
        p = target / 'src/module.c'
        p.unlink()
        p.symlink_to(ROOT / 'src/module.c')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.builder.validate_sources(target)

    def test_stage_rejects_extra_files_and_symlinks_without_deleting(self):
        stage = self.temp / 'stage'
        stage.mkdir()
        extra = stage / 'game.nsp'
        extra.write_bytes(b'private sentinel')
        with self.assertRaises(ValueError):
            self.builder.validate_tree(stage, {'manifest.json'})
        self.assertEqual(extra.read_bytes(), b'private sentinel')
        extra.unlink()
        link = stage / 'manifest.json'
        link.symlink_to(ROOT / 'SOURCES.json')
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.builder.validate_tree(stage, {'manifest.json'})

    def test_stage_rejects_linked_parent(self):
        actual = self.temp / 'actual'
        actual.mkdir()
        linked = self.temp / 'linked'
        linked.symlink_to(actual, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.builder.validate_tree(linked / 'stage', set())

    def test_unpinned_native_binary_is_rejected_before_staging(self):
        module = self.temp / 'wrong.so'
        module.write_bytes(b'ELF is not the verified module')
        stage = self.temp / 'stage'
        with self.assertRaisesRegex(ValueError, 'module'):
            self.builder.stage_package(module, stage)
        self.assertFalse(stage.exists())

    def test_staging_paths_cannot_escape_the_package(self):
        with self.assertRaises(ValueError):
            self.builder.validate_tree(self.temp, {'../escape.txt'})


if __name__ == '__main__':
    unittest.main()
