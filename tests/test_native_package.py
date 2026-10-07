"""Behavioral contract for the live skin and native package boundary."""
import hashlib
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
    PAGE_IDS = {'character', 'equipment', 'combat', 'map'}
    LOCAL_MAP_ACTIONS = {'map_zoom_in', 'map_zoom_out', 'map_recenter', 'map_pin', 'map_clear_pin'}

    def manifest(self):
        self.assertIsNotNone(adapter, 'Live adapter must connect the reviewed UI to the real reader')
        return adapter.make_manifest()

    def pages(self):
        return {page['id']: page['widgets'] for page in self.manifest()['pages']}

    def metrics(self):
        manifest = self.manifest()
        self.assertEqual(manifest['font'], 'file:assets/duo-sans.mfnt')
        self.assertEqual(manifest['font_atlas'], 'file:assets/duo-sans.png')
        metrics = (ROOT / manifest['font'][5:]).read_bytes()
        cap = struct.unpack_from('<I', metrics, 0x1C)[0]
        count = struct.unpack_from('<I', metrics, 0x20)[0]
        start = struct.unpack_from('<I', metrics, 0x28)[0]
        glyphs = {i+31: struct.unpack_from('<4H2hH', metrics, start+14*i) for i in range(1, count)}
        self.assertGreater(cap, 0)
        return cap, glyphs

    def assert_fits(self, widget, values):
        cap, glyphs = self.metrics()
        self.assertTrue(widget['fit_text'])
        self.assertEqual(widget['max_lines'], 1)
        self.assertGreater(widget['text_min_scale'], 0)
        self.assertLessEqual(widget['text_min_scale'], widget['text_scale'])
        for value in values:
            # MFNT advance is converted by five pixels per scale unit / cap height.
            # Complete numbers must fit: truncation changes their meaning.
            advance = sum(glyphs[ord(c)][-1] for c in value)
            self.assertLessEqual(advance * widget['text_min_scale'] * 5,
                                 widget['wrap_width'] * cap, (widget['id'], value))

    def test_packaged_navigation_reaches_all_four_pages(self):
        manifest = self.manifest()
        pages = {page['id']: page['widgets'] for page in manifest['pages']}
        self.assertEqual(set(pages), self.PAGE_IDS)
        page_actions = {name: action['page'] for name, action in manifest['actions'].items()
                        if action['kind'] == 'page'}
        self.assertEqual(set(page_actions.values()), self.PAGE_IDS)
        edges = {}
        used = set()
        for page, widgets in pages.items():
            taps = {widget['on_tap'] for widget in widgets if 'on_tap' in widget}
            self.assertTrue(taps <= manifest['actions'].keys())
            used |= taps
            edges[page] = {page_actions[tap] for tap in taps if tap in page_actions}
            self.assertTrue({'character', 'combat', 'map'} <= edges[page])
        reachable, pending = set(), ['character']
        while pending:
            current = pending.pop()
            if current not in reachable:
                reachable.add(current)
                pending.extend(edges[current] - reachable)
        self.assertEqual(reachable, self.PAGE_IDS)
        self.assertEqual(used, set(manifest['actions']), 'Every action needs a usable control')

    def test_each_live_text_field_has_offline_and_error_handling(self):
        for page, widgets in self.pages().items():
            with self.subTest(page=page):
                errors = [w for w in widgets if w.get('need_bind') == 'module_error'
                          and 'unavailable' in w.get('text', '').lower()]
                self.assertEqual(len(errors), 1, 'Every page must explain module errors')
                for widget in widgets:
                    if 'bind_text' not in widget:
                        continue
                    self.assertEqual(widget['need_bind'], 'module_ready')
                    self.assertEqual(widget['hide_bind'], 'module_error')
                    self.assertEqual(widget['hide_eq'], 1)
                    fallback = [w for w in widgets if w.get('id') == widget['id'] + '_offline']
                    self.assertEqual(len(fallback), 1)
                    self.assertEqual(fallback[0]['need_bind'], '!module_ready')
                    self.assertNotIn('bind_text', fallback[0])
                    self.assertTrue(fallback[0]['text'])
                    self.assertEqual(fallback[0]['rect'], widget['rect'])

    def test_character_binds_all_ten_stats_and_complete_identity_labels(self):
        widgets = self.pages()['character']
        bindings = {w['bind_text']: w for w in widgets if 'bind_text' in w}
        self.assertTrue({'details.level', 'details.paragon', 'details.status'} <= bindings.keys())
        self.assertEqual({key for key in bindings if key.startswith('stats.')},
                         {f'stats.{i}.value' for i in range(10)})
        self.assertFalse(any(w.get('text') in ('Level', 'Paragon') for w in widgets))
        self.assert_fits(bindings['details.level'], ('Level 70', 'Level unavailable'))
        self.assert_fits(bindings['details.paragon'], ('Paragon 20000', 'Paragon unavailable'))

    def test_exact_identity_and_read_only_action_whitelist(self):
        manifest = self.manifest()
        self.assertTrue(manifest['requires_module'])
        self.assertFalse(manifest['module_tick_hidden'])
        self.assertFalse(manifest['nav'])
        self.assertEqual(manifest['title_id'], '01001B300B9BE000')
        self.assertEqual(manifest['module']['build_ids'],
                         ['2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000'])
        self.assertFalse({'writes', 'patches', 'spies', 'calls', 'points'} & manifest.keys())
        expected = {f'open_{page}': {'kind': 'page', 'page': page} for page in self.PAGE_IDS}
        expected.update({f'inspect_{i}': {'kind': 'module', 'action': 'inspect_equipment', 'argument': i}
                         for i in range(13)})
        expected.update({name: {'kind': 'module', 'action': name, 'argument': 0}
                         for name in self.LOCAL_MAP_ACTIONS})
        self.assertEqual(manifest['actions'], expected)

    def test_equipment_slots_have_bounded_inspection_controls(self):
        widgets = self.pages()['equipment']
        bindings = {w.get('bind_text') for w in widgets}
        self.assertTrue({f'gear.{i}.name' for i in range(13)} <= bindings)
        self.assertTrue({'gear.selected_slot', 'gear.selected_name', 'gear.inspect_note'} <= bindings)
        # Native left hand is the weapon; right hand is the shield in the
        # independently captured Inventory comparison on the target build.
        targets = {w.get('on_tap'): w for w in widgets if w.get('on_tap')}
        self.assertEqual(targets['inspect_3']['rect'][:2], [44, 310])
        self.assertEqual(targets['inspect_2']['rect'][:2], [632, 310])
        for i in range(13):
            targets = [w for w in widgets if w.get('on_tap') == f'inspect_{i}']
            self.assertEqual(len(targets), 1)
            self.assertGreaterEqual(targets[0]['rect'][2], 64)
            self.assertGreaterEqual(targets[0]['rect'][3], 64)
        for page in ('character', 'combat', 'map'):
            self.assertFalse(any(w.get('on_tap', '').startswith('inspect_') for w in self.pages()[page]))

    def test_all_skill_and_rune_slots_are_live_on_skills_page(self):
        widgets = self.pages()['combat']
        bindings = {w.get('bind_text') for w in widgets}
        self.assertTrue({f'skills.{i}.name' for i in range(6)} <= bindings)
        self.assertTrue({f'skills.{i}.rune' for i in range(6)} <= bindings)

    def test_class_image_covers_unknown_and_seven_classes_without_stale_error_state(self):
        widgets = self.pages()['character']
        classes = [w for w in widgets if w.get('bind') == 'details.class_index']
        self.assertEqual(len(classes), 1)
        title = classes[0]
        self.assertEqual(title['need_bind'], 'module_ready')
        self.assertEqual(title['hide_bind'], 'module_error')
        self.assertEqual(title['hide_eq'], 1)
        self.assertEqual(title['src_names'], [f'file:assets/class-{i}.png' for i in range(8)])
        for state in ('!module_ready', 'module_error'):
            generic = [w for w in widgets if w.get('need_bind') == state
                       and w.get('src') == 'file:assets/class-0.png' and 'bind' not in w]
            self.assertTrue(generic, f'Generic class heading missing for {state}')
        pins = json.loads((ROOT / 'ASSETS.json').read_text())
        for source in title['src_names']:
            path = ROOT / source[5:]
            self.assertIn(path.name, pins)
            pixels = path.read_bytes()
            self.assertEqual(pixels[:8], b'\x89PNG\r\n\x1a\n')
            width, height = struct.unpack_from('>II', pixels, 16)
            self.assertGreater(width, 0)
            self.assertGreater(height, 0)

    def test_map_image_and_local_controls_have_available_and_unavailable_states(self):
        widgets = self.pages()['map']
        terrain = [w for w in widgets if w.get('src_bind') == 'map.image']
        self.assertEqual(len(terrain), 1)
        self.assertEqual(terrain[0]['need_bind'], 'map.available')
        self.assertEqual(terrain[0]['hide_bind'], 'module_error')
        self.assertEqual(terrain[0]['hide_eq'], 1)
        self.assertTrue(any(w.get('need_bind') == '!map.available' and w.get('text') for w in widgets))
        controls = {w['on_tap'] for w in widgets if w.get('on_tap') in self.LOCAL_MAP_ACTIONS}
        self.assertEqual(controls, self.LOCAL_MAP_ACTIONS)
        self.assertTrue({'map.description', 'map.status'} <= {w.get('bind_text') for w in widgets})

    def test_all_manifest_artwork_and_fonts_are_pinned_native_assets(self):
        manifest = self.manifest()
        sources = {manifest['font'], manifest['font_atlas']}
        for page in manifest['pages']:
            for widget in page['widgets']:
                if widget['type'] == 'image':
                    sources.add(widget['src'])
                    sources.update(widget.get('src_names', []))
        pins = json.loads((ROOT / 'ASSETS.json').read_text())
        for source in sources:
            self.assertTrue(source.startswith('file:assets/'), source)
            path = Path(source[5:])
            self.assertEqual(path.parts, ('assets', path.name))
            self.assertIn(path.name, pins)
            self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), pins[path.name])

    def test_native_numeric_buffer_cannot_be_truncated_into_a_different_value(self):
        widgets = [w for w in self.pages()['character'] if w.get('bind_text', '').startswith('stats.')]
        self.assertEqual(len(widgets), 10)
        full_buffer = ('8' * 27, '-' + '8' * 22 + '.88%', '+' + '8' * 22 + '.88%')
        for widget in widgets:
            field = int(widget['bind_text'].split('.')[1])
            if field in (2, 4, 5, 6, 7):
                # Native nearest-even formatting deliberately rejects larger magnitudes.
                values = ('8388607', '-8388607', 'Unavailable')
            elif field == 8:
                # The native critical-chance formula clamps its result to 0..1.
                values = ('100.00%', '0.00%', 'Unavailable')
            else:
                values = full_buffer + ('Unavailable',)
            self.assert_fits(widget, values)


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

    def test_native_font_or_class_artwork_mutation_is_rejected(self):
        target = self.copy_source()
        for name in ('duo-sans.mfnt', 'duo-sans.png', 'class-0.png', 'class-7.png'):
            with self.subTest(asset=name):
                path = target / 'assets' / name
                original = path.read_bytes()
                path.write_bytes(original + b'altered asset')
                with self.assertRaisesRegex(ValueError, 'asset hash'):
                    self.builder.validate_sources(target)
                path.write_bytes(original)

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
