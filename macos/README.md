# macOS on piano

Not booting yet: the kernel sources and components compile, the kernel does
not link (see "What is missing").

macOS 27.3 (26C5035r) is built by the UniXNU core
([UniXNU/UniXNU](https://github.com/UniXNU/UniXNU),
`profiles/os/macos-27.3-26C5035r`), not by the SM8750 fork the iPadOS line
uses. The board values are the `[os_overrides."macos-27.3-26C5035r"]` tables
of `board.toml`, modelled on the Xiaomi Pad 6S Pro board
([UniXNU/UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng),
BSD-2-Clause-Patent).

| Path | What |
| --- | --- |
| `board.toml` `[os_overrides."macos-27.3-26C5035r"]` | Components, kernel archives, boot arguments, ADT identity (Mac17,11) |
| `configs/piano_macos_defconfig` | Kernel configuration |
| `patches/xnu-12377/` | SM8750 patches for the macOS kernel (xnu-12377) |
| `macos/unixnu.lock` | Pins of the macOS build |
| `scripts/prepare-macos-profile.py` | Writes `.cache/macos-device`, this repository with `macos/unixnu.lock` as its lock |

The two systems are built by different UniXNU checkouts and every fetch
rewrites `unixnu.lock`, so macOS is built from `.cache/macos-device` and
keeps its own lock.

## Build

```sh
scripts/prepare-macos-profile.py
M=$PWD/.cache/macos-device
cd <UniXNU core checkout>
O="-D $M --out out/piano-macos --os macos-27.3-26C5035r"
./unixnu defconfig piano_macos_defconfig $O
./unixnu fetch $O
./unixnu build kernel $O
```

## What is missing

The macOS kernel of the UniXNU core takes its storage, display, key event
and AES services from one board bundle. On the Pad 6S Pro that is
`build-sheng-source-storage.py` (the UFS runtime the root, SystemOS and Data
mounts are verified through) and `build-gop-legacy-display-27.py`. Piano has
no such bundle yet, so the link ends with undefined symbols:

- `native_ufs_store_*`, `native_owner_root_*`: the source storage archive
  with a UFS driver for SM8750.
- `IOBlockStorageDevice`, `IOAESAccelerator` classes: the bindings to the
  26C5035r kernel collection, made by the board script.
- `source_corecrypto14_*`, `native_exit_trace`: defaults the same scripts
  provide.

Order of work: the storage bundle (UFS on SM8750, then
`CONFIG_XNU_SOURCE_STORAGE` and the `CONFIG_XNU_OWNER_*` mounts), the display
script, the SMP patch for xnu-12377, then the GPU, input and USB.
