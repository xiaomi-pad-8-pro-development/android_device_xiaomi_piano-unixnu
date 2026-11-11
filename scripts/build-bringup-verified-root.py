#!/usr/bin/env python3
"""Bring-up stand-in for the owner-root policy archive.

The real recipe (build-pagani-verified-root-probe.py --component os --include-systemos)
needs the untouched original OS and SystemOS images attached read-only with hdiutil, a
host build of the verified-root library and the owner signing keys, and verifies every
4 KiB block of both images. None of that is available while bringing the port up.

This builds the same archive path with the real verified-root sources and an empty
policy envelope: the symbols exist, and vr_open() refuses every image at runtime, so
storage stays unverified instead of being believed. Bring-up instrument only.
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/verified-root-os-xnu-bundle'
out.mkdir(parents=True, exist_ok=True)

assembly = out / 'bringup-inputs.S'
assembly.write_text('''// Bring-up stand-in: no policy, no trusted key, no samples.
.section __DATA_CONST,__const
.balign 8
.globl _verified_root_manifest, _verified_root_manifest_bytes
_verified_root_manifest:
.byte 0
_verified_root_manifest_bytes:
.quad 0
.globl _verified_root_key
_verified_root_key:
.space 395
.balign 8
.globl _verified_root_sample_indices
_verified_root_sample_indices:
.quad 0, 0, 0, 0
.globl _verified_root_sample_lengths
_verified_root_sample_lengths:
.quad 0, 0, 0, 0
.globl _verified_root_samples
_verified_root_samples:
.byte 0
.balign 8
.globl _verified_root_policies, _verified_root_policy_count
_verified_root_policies:
.quad 0, 0
_verified_root_policy_count:
.quad 0
.section __DATA,__data
.weak_definition _native_adreno_pool_stats
.globl _native_adreno_pool_stats
_native_adreno_pool_stats:
.space 32
// The GPU trace/pixel hooks belong to the Adreno component; the boot-progress and GOP-presenter
// paths reference them even when no GPU driver is linked. They stay weak so a linked Adreno driver
// (component:Adreno830, CONFIG_XNU_ADRENO830) provides the real ones instead of colliding here.
.section __TEXT,__text
.weak_definition _native_gpu_trace_enabled
.globl _native_gpu_trace_enabled
_native_gpu_trace_enabled:
mov w0, #0
ret
.weak_definition _native_gpu_trace_record
.globl _native_gpu_trace_record
_native_gpu_trace_record:
ret
.weak_definition _native_gpu_pixel_put
.globl _native_gpu_pixel_put
_native_gpu_pixel_put:
mov w0, #1
ret
.globl _pagani_haptics_ios_commit
_pagani_haptics_ios_commit:
mov w0, #0
ret
.globl _pagani_torch_ios_commit
_pagani_torch_ios_commit:
mov w0, #0
ret
''')

sources = [ROOT / 'kits/PaganiVerifiedRoot/src/verified_root.c',
           ROOT / 'kits/PaganiVerifiedRoot/adapters/xnu_crypto.c',
           ROOT / 'scripts/qemu-entry/verified-root-probe.c']
missing = [str(path) for path in sources if not path.is_file()]
if missing:
    raise SystemExit('verified-root sources missing: ' + ', '.join(missing))

recipe = ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/bsd/DEVELOPMENT/bsd_init.o.json'
record = json.loads(recipe.read_text().strip().rstrip(','))

objects = []
for source in sources:
    destination = out / (source.stem + '.o')
    compile_args, command, index = record['arguments'], [], 0
    while index < len(compile_args):
        value = compile_args[index]
        if value == record['file']:
            command.append(str(source))
        elif value == '-o':
            command += ['-o', str(destination)]
            index += 1
        elif value in ('-MF', '-MJ'):
            index += 1
        elif value not in ('-MD', '-MP'):
            command.append(value)
        index += 1
    command += ['-I' + str(ROOT / 'kits/PaganiVerifiedRoot/include'),
                # Bring-up instrument: newer clang turns XNU-header warnings into errors
                # (unsafe-buffer-usage, reserved-identifier, shadow-header) that Xcode 26
                # does not enable. Keep them as warnings here.
                '-Wno-error', '-Wno-error=missing-include-dirs']
    result = subprocess.run(command, cwd=record['directory'], capture_output=True, text=True)
    (out / (source.stem + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        sys.exit(result.stderr[-3500:])
    objects.append(destination)

sdk = subprocess.check_output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'], text=True).strip()
inputs = out / 'bringup-inputs.o'
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-isysroot', sdk, '-c',
                str(assembly), '-o', str(inputs)], check=True)
objects.append(inputs)

archive = out / 'libVerifiedRootProbe.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), *[str(obj) for obj in objects]], check=True)
print(f'bring-up verified-root archive: {archive} ({archive.stat().st_size} bytes, empty policy)')
