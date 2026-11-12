# UniXNU for the Xiaomi Pad 8 Pro (piano)

| | |
| --- | --- |
| What | [UniXNU](https://github.com/UniXNU/UniXNU) device repository: runs iPadOS on the Pad 8 Pro (SM8750) |
| Goes | Next to a `UniXNU` checkout, as `-D ../android_device_xiaomi_piano-unixnu` |
| Comes from | The UniXNU device template, with the SM8750 support of the OnePlus 13T port ([UniXNU-sm8750-common](https://github.com/IssacL04/UniXNU-sm8750-common)) |
| Firmware | [android_device_xiaomi_piano-uefi](https://github.com/xiaomi-pad-8-pro-development/android_device_xiaomi_piano-uefi), branch `seventeen`: run its `tools/setup-deps.sh`, then build with `--uefi` pointing at its `deps/Mu-Silicium` |
| OS | iPadOS 17.7.11 (21H461) |

## State

Experimental. From a RAM boot (`fastboot boot`, nothing flashed) XNU mounts
the iPadOS root from the `unixnu` UFS partition and launchd runs to the home
screen. Touch (NT36532) works. The A830 GPU driver runs in the kernel, and
the Metal plugin draws app icons with the right colours; the home screen
still scrolls slowly (about 25 fps) and widgets stay white.

macOS 27.3 has its board tables and compiles, but its kernel does not link
yet ([macos/README.md](macos/README.md)).

## Layout

iPadOS and macOS share the hardware half of this repository; see
[docs/LAYOUT.md](docs/LAYOUT.md). After every `./unixnu fetch`, run
`scripts/apply-local-fixes.sh <UniXNU checkout>`.

## Build

Needs macOS with Xcode. From the UniXNU checkout:

```sh
./unixnu defconfig piano_defconfig -D ../android_device_xiaomi_piano-unixnu
./unixnu fetch
./unixnu build kernel
./unixnu build package
./unixnu build firmware --uefi ../android_device_xiaomi_piano-uefi/deps/Mu-Silicium
./unixnu build payload
./unixnu bootimg
fastboot boot <output>/boot/boot.img
```

See [Build and boot](https://unixnu.com/building).

### User data and GPU plugin

The user data volume is packed into the boot image as a RAM disk, so a
test boot writes nothing to the tablet. Point `data_ramdisk` in
`unixnu.local.toml` at the seed and `trust_cache` at the trust bundle that
lists the plugin. From the UniXNU checkout, with Rust 1.98.0 (target
`aarch64-apple-ios`), glslang, glslc and bison 3.8 on `PATH`:

```sh
O=out/piano/ipados-17.7.11-21H461/piano
sudo python3 ../android_device_xiaomi_piano-unixnu/scripts/prepare-ram-data.py   # writes the base seed
cd $O/tree
python3 scripts/build-turnip-darwin.py --target ios
python3 scripts/build-adreno-metal-plugin.py --chip a830 --target ios --no-replay
printf 'TU_DEBUG=sysmem,nocb\n' > ../gpu.env
sudo python3 scripts/add-gpu-plugin-data.py --publish \
    --clients 'iconservicesagent,WidgetRenderer*' \
    --enable-jobs com.apple.iconservices.iconservicesagent \
    --env ../gpu.env <base seed> ../ram-data-gpu.dbrdisk
python3 scripts/trust-append-module.py ../trust/original-trust.bin ../trust/gpu-trust.bin \
    $(python3 -c 'import json;j=json.load(open("build/adreno-metal-plugin/trust.json"));print(j["cdhash"],j["daemon_cdhash"],j["turnip_cdhash"])')
```

A plugin built elsewhere has other CDHashes, so its trust bundle has another
SHA-256: add it to `trust_cache_sha256` in `board.toml`. The `--clients`
list stays short: with `@apps,@extensions` boot is slower and nothing draws
better. The touch firmware headers under `PaganiInput/Nvt` come from the
vendor blobs (`xxd -i`) and are not in Git.
