#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Host sanitizer and live UI/package tests. Optional explicit NDK builds twice."""
from pathlib import Path
import argparse
import subprocess
import sys
import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ndk', type=Path, help='Also verify two reproducible Android builds using this NDK')
    args = parser.parse_args()
    build.validate_sources()
    root = Path(__file__).resolve().parent
    output = root.parent / 'dist/live/host-tests'
    suites = {
        'module': ['player_probe.c', 'skills_probe.c', 'stats_probe.c', 'details_probe.c', 'names_probe.c', 'duo_font.c', 'module.c'],
        'names': ['names_probe.c'],
        'cdr_default': ['player_probe.c', 'stats_probe.c'],
        'skills': ['player_probe.c', 'skills_probe.c'],
        'font_decoder': ['duo_font.c'],
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
        archive, receipt = build.build(args.ndk, root.parent / 'dist/live')
        first = archive.read_bytes()
        second, _ = build.build(args.ndk, root.parent / 'dist/live/reproduction')
        if second.read_bytes() != first:
            raise ValueError('Separate Android build/archive outputs differ')
        print('PASS two native builds and ZIPs match: ' + receipt['archive_sha256'])


if __name__ == '__main__':
    main()
