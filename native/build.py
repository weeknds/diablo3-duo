#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the exact Android live test candidate; no downloads or device access."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent
NDK_REVISION = '28.2.13676358'
SOURCES = ('module.c', 'player_probe.c', 'skills_probe.c', 'stats_probe.c',
           'details_probe.c', 'names_probe.c', 'duo_font.c')
HEADERS = ('details_probe.h', 'duo_font.h', 'health_probe.h', 'name_layout.h',
           'names_probe.h', 'player_probe.h', 'probe_internal.h', 'skills_probe.h', 'stats_probe.h')
TESTS = ('test_cdr_default.c', 'test_font_decoder.c', 'test_module.c', 'test_names.c',
         'test_player_probe.c', 'test_skills.c', 'test_stats_probe.c',
         'skills_frozen/test_player_probe.c', 'skills_frozen/test_skills_probe.c')
VENDOR = ('LICENSE.txt', 'UPSTREAM.json', 'dsmod_module_abi.h', 'dsmod_module_extensions.h')
PINNED_INPUTS = ({'src/' + p for p in SOURCES + HEADERS}
                 | {'tests/' + p for p in TESTS} | {'vendor/' + p for p in VENDOR})
SOURCE_FILES = PINNED_INPUTS | {'SOURCES.json', 'build.py', 'live_ui.py', 'test.py', 'README.md'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def no_symlink(path):
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError(f'Rejected symlink: {part}')


def validate_tree(root, allowed, *, exact=False, bytecode=False):
    """Reject unexpected files before writing; never remove unknown content."""
    no_symlink(root)
    for name in allowed:
        p = PurePosixPath(name)
        if p.is_absolute() or any(v in ('', '.', '..') for v in name.split('/')) or '\\' in name:
            raise ValueError('Invalid inventory path: ' + name)
    actual = set()
    for p in root.rglob('*') if root.exists() else []:
        if p.is_symlink():
            raise ValueError(f'Rejected symlink: {p}')
        relative = p.relative_to(root).as_posix()
        if bytecode and '__pycache__' in p.relative_to(root).parts and (p.is_dir() or p.suffix == '.pyc'):
            continue
        if p.is_dir():
            continue
        if not p.is_file() or relative not in allowed:
            raise ValueError('Unexpected file: ' + relative)
        actual.add(relative)
    if exact and actual != allowed:
        raise ValueError('Missing files: ' + ', '.join(sorted(allowed - actual)))
    return actual


def validate_sources(root=ROOT):
    validate_tree(root, SOURCE_FILES, exact=True, bytecode=True)
    pins = json.loads((root / 'SOURCES.json').read_text())
    if set(pins) != PINNED_INPUTS:
        raise ValueError('Native source inventory differs from the reviewed source set')
    for relative, expected in pins.items():
        if digest(root / relative) != expected:
            raise ValueError('Native source hash mismatch: ' + relative)


def load_module(name, path):
    no_symlink(path)
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def ui():
    return load_module('duo_live_ui', ROOT / 'live_ui.py')


def stage_package(module_path, stage):
    adapter = ui()
    no_symlink(module_path)
    if digest(module_path) != adapter.MODULE_HASH:
        raise ValueError('Native module differs from the reproduced reviewed build')
    static = load_module('duo_static_validation', REPO / 'tools/build.py')
    static.validate_development(REPO)
    manifest = adapter.make_manifest()
    assets = {manifest[k][5:] for k in ('font', 'font_atlas')}
    assets |= {w['src'][5:] for p in manifest['pages'] for w in p['widgets'] if w['type'] == 'image'}
    assets |= {'assets/OFL.txt', 'assets/NOTICE.txt'}
    allowed = {'dualscreen/' + p for p in assets | {adapter.MODULE, 'manifest.json', 'licenses/GPL-3.0.txt'}}
    allowed.add('package.json')
    validate_tree(stage, allowed)
    dual = stage / 'dualscreen'
    for relative in sorted(assets):
        target = dual / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(REPO / 'package/dualscreen' / relative, target)
    target = dual / adapter.MODULE
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(module_path, target)
    (dual / 'licenses').mkdir(exist_ok=True)
    shutil.copyfile(ROOT / 'vendor/LICENSE.txt', dual / 'licenses/GPL-3.0.txt')
    (dual / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    package = {k:manifest[k] for k in ('format', 'name', 'title_id', 'min_runtime', 'requires_module', 'module')}
    package.update(type='dual-screen-mod', version=adapter.VERSION)
    (stage / 'package.json').write_text(json.dumps(package, indent=2) + '\n')
    validate_tree(stage, allowed, exact=True)
    return manifest, allowed


def verify_archive(archive, stage, manifest, allowed):
    adapter = ui()
    with zipfile.ZipFile(archive) as z:
        if len(z.namelist()) != len(allowed) or set(z.namelist()) != allowed:
            raise ValueError('Archive file inventory mismatch')
        meta = json.loads(z.read('package.json'))
        expected = {'format':1, 'type':'dual-screen-mod', 'title_id':manifest['title_id'],
                    'name':manifest['name'], 'version':adapter.VERSION, 'min_runtime':18,
                    'requires_module':True, 'module':manifest['module']}
        if meta != expected or json.loads(z.read('dualscreen/manifest.json')) != manifest:
            raise ValueError('Archive manifest or module contract mismatch')
        for name in allowed - {'package.json', 'dualscreen/manifest.json'}:
            if z.read(name) != (stage / name).read_bytes():
                raise ValueError('Archive payload differs from stage: ' + name)
        if hashlib.sha256(z.read('dualscreen/' + adapter.MODULE)).hexdigest() != adapter.MODULE_HASH:
            raise ValueError('Archive module hash mismatch')


def build(ndk, output):
    validate_sources()
    no_symlink(output)
    output = output.absolute()
    if not output.is_relative_to(REPO / 'dist') or '..' in output.parts:
        raise ValueError('Native output must remain inside the ignored repository dist directory')
    ndk = ndk.expanduser().resolve(strict=True)
    properties = dict(line.split('=', 1) for line in (ndk / 'source.properties').read_text().splitlines() if '=' in line)
    properties = {k.strip():v.strip() for k,v in properties.items()}
    if properties.get('Pkg.Revision') != NDK_REVISION:
        raise ValueError('Expected Android NDK r28c (' + NDK_REVISION + ')')
    compiler = ndk / 'toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android24-clang'
    if not compiler.is_file():
        raise ValueError('Pinned macOS Android compiler missing: ' + str(compiler))
    adapter = ui()
    compiled = output / 'build'
    validate_tree(compiled, {'01001B300B9BE000.so'})
    compiled.mkdir(parents=True, exist_ok=True)
    module_path = compiled / '01001B300B9BE000.so'
    command = [str(compiler), '-std=c11', '-O2', '-fPIC', '-shared', '-fvisibility=hidden',
               '-Wall', '-Wextra', '-Werror', '-pedantic', '-Wl,--no-undefined',
               '-Wl,-z,relro,-z,now', '-Wl,-z,max-page-size=16384',
               '-I', str(ROOT / 'vendor'), '-I', str(ROOT / 'src')]
    command += [str(ROOT / 'src' / p) for p in SOURCES] + ['-lm', '-o', str(module_path)]
    subprocess.run(command, check=True)
    manifest, allowed = stage_package(module_path, output / 'stage')
    pin = json.loads((REPO / 'vendor/UPSTREAM.json').read_text())
    packager = REPO / 'vendor/build_dualscreen_package.py'
    if digest(packager) != pin['files']['vendor/build_dualscreen_package.py']['sha256']:
        raise ValueError('Pinned upstream packager hash mismatch')
    helper = load_module('duo_upstream_packager', packager)
    archive = output / '01001B300B9BE000.dsmod.zip'
    no_symlink(archive)
    archive = helper.build_package(package=output / 'stage', output=output, version=adapter.VERSION,
                    min_runtime=18, modules=['android-arm64-v8a=' + str(module_path)],
                    module_build_ids=[adapter.BUILD_ID])
    verify_archive(archive, output / 'stage', manifest, allowed)
    receipt = {'version':adapter.VERSION, 'stage':'live-test-candidate', 'ndk_revision':NDK_REVISION,
               'module_sha256':digest(module_path), 'archive_sha256':digest(archive),
               'archive_bytes':archive.stat().st_size, 'compiler_command':command,
               'staged_files':{p:digest(output / 'stage' / p) for p in sorted(allowed)},
               'hardware_support_claimed':False}
    for name, content in [('verification.json', json.dumps(receipt, indent=2) + '\n'),
                          ('SHA256SUMS', digest(archive) + '  ' + archive.name + '\n')]:
        no_symlink(output / name)
        (output / name).write_text(content)
    return archive, receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, required=True, help='Explicit local macOS Android NDK r28c directory')
    parser.add_argument('--output', type=Path, default=REPO / 'dist/live', help='Output directory under repository dist/')
    args = parser.parse_args()
    try:
        archive, receipt = build(args.ndk, args.output)
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        parser.exit(1, str(exc) + '\n')
    print(json.dumps({'archive':str(archive), 'sha256':receipt['archive_sha256'],
                      'module_sha256':receipt['module_sha256'], 'version':receipt['version']}, indent=2))


if __name__ == '__main__':
    main()
