# One repository, two operating systems

This repository is meant to boot both iPadOS and macOS on piano. The
hardware half is shared; each OS adds only what is specific to it.

| Path | Shared | What |
| --- | --- | --- |
| `dts/piano.dts` | yes | Hardware description for the ADT |
| `board.toml` `[firmware]`, `[bootimg]`, `[userdata]` | yes | Our Mu-Silicium UEFI, RAM boot image, UFS partitions |
| `modules/`, `package/` | yes | Drivers (UFS, SMMU, power, display, input) |
| `patches/local/` | yes | Fixes to fetched upstream trees, re-applied by `scripts/apply-local-fixes.sh` |
| `configs/piano_defconfig` | iPadOS 17.7.11 | Kernel config of the current profile |
| `board.toml` `[package.defconfig]` | iPadOS | One line per iPadOS profile (17.7.11 now; 26.5.2 and 27.0.1 exist in UniXNU) |
| `macos/` | macOS | Kernel collection, image and boot argument tooling (UniXNU has no macOS profile) |

## iPadOS profiles

UniXNU builds one `os_profile` per board. A new profile needs its own
defconfig, kernelcache hash, AMFI `early_security_init` address, trust cache
and root volume UUID, all taken from that IPSW. Add them only once the IPSW
is fetched and the values are measured; nothing is guessed here.

## macOS

Written here from scratch. Other piano ports are used as references for what
the hardware needs, but no code is copied from repositories without a
license. Order: kernel collection and ADT, RAM boot to launchd, then the GPU
(Adreno 830), USB console and power.
