#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host sanitizer and live UI/package tests. Optional explicit NDK builds twice."""
from pathlib import Path
import argparse
import subprocess
import sys
import build


def main():
    root = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, help='Also verify two reproducible Android builds using this NDK')
    parser.add_argument('--output', type=Path, default=root.parent/'dist/live',
                        help='Keep test/build output in this directory under repository dist/')
    args = parser.parse_args()
    build.validate_sources()
    directory = args.output.absolute()
    if not directory.is_relative_to(root.parent/'dist') or '..' in directory.parts:
        raise ValueError('Test output must remain inside repository dist/')
    output = directory/'host-tests'
    suites = {
        'module': list(build.SOURCES),
        'names': ['names_probe.c'],
        'cdr_default': ['player_probe.c', 'stats_probe.c'],
        'skills': ['player_probe.c', 'skills_probe.c'],
        'font_decoder': ['duo_font.c'],
        'player_probe': ['player_probe.c'],
        'stats_probe': ['player_probe.c', 'stats_probe.c'],
        'equipment': ['player_probe.c', 'equipment_probe.c'],
        'equipment_inspect': ['equipment_inspect.c','names_probe.c'],
        'map': ['player_probe.c', 'map_probe.c'],
        'map_view': ['map_probe.c', 'map_view.c', 'nav_probe.c'],
        'nav': ['player_probe.c','nav_probe.c'],
    }
    build.validate_tree(output, {'test_' + n for n in suites})
    output.mkdir(parents=True, exist_ok=True)
    base = ['clang', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
            '-I', str(root / 'vendor'), '-I', str(root / 'src')]
    for name, sources in suites.items():
        binary = output / ('test_' + name)
        command = base + [str(root / 'src' / s) for s in sources]
        command += [str(root / 'tests' / ('test_' + name + '.c')), '-lm', '-o', str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True)
    subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(root.parent / 'tests'),
                    '-p', 'test_native_package.py', '-v'], check=True)
    if args.ndk:
        archive, receipt = build.build(args.ndk, directory)
        first = archive.read_bytes()
        second, _ = build.build(args.ndk, directory/'reproduction')
        if second.read_bytes() != first:
            raise ValueError('Separate Android build/archive outputs differ')
        print('PASS two native builds and ZIPs match: ' + receipt['archive_sha256'])


if __name__ == '__main__':
    main()
