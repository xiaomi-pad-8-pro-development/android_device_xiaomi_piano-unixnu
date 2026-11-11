#!/usr/bin/env python3
"""Compile the device's macos/glue/PianoMacGlue.c into build/piano-macos-glue/libPianoMacGlue.a."""
import os
import subprocess
from pathlib import Path

TREE = Path(__file__).resolve().parents[1]   # the UniXNU build tree; scripts are copied into it


def main():
    source = Path(os.environ['UNIXNU_DEVICE']) / 'macos/glue/PianoMacGlue.c'
    out = TREE / 'build/piano-macos-glue'
    out.mkdir(parents=True, exist_ok=True)
    obj, lib = out / 'PianoMacGlue.o', out / 'libPianoMacGlue.a'
    subprocess.run(['xcrun', '--sdk', 'iphoneos', 'clang', '-target', 'arm64e-apple-ios27.0', '-mkernel', '-ffreestanding', '-fno-builtin',
                    '-fno-stack-protector', '-O2', '-Wall', '-Werror', '-c', source, '-o', obj], check=True)
    lib.unlink(missing_ok=True)
    subprocess.run(['xcrun', 'ar', 'rcs', lib, obj], check=True)
    print(lib)


if __name__ == '__main__':
    main()
