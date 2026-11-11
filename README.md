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
the iPadOS root from the `unixnu` UFS partition and launchd runs; there is
no picture after the boot log yet (no GPU driver).

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
