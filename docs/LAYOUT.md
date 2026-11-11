# One repository, two operating systems

This repository is meant to boot both iPadOS and macOS on piano. The
hardware half is shared; each OS adds only what is specific to it.

| Path | Shared | What |
| --- | --- | --- |
| `dts/piano.dts` | yes | Hardware description for the ADT |
| `board.toml` up to `[os_overrides]` | yes | Our Mu-Silicium UEFI, RAM boot image, UFS partition |
| `board.toml` `[os_overrides."<profile>"]` | one OS | Components, kernel archives, boot arguments and identity of that system |
| `modules/`, `package/` | yes | Drivers (UFS, SMMU, power, display, input) |
| `patches/local/` | yes | Fixes to fetched upstream trees, re-applied by `scripts/apply-local-fixes.sh` |
| `configs/piano_defconfig`, `patches/xnu/`, `unixnu.lock` | iPadOS 17.7.11 | Kernel config, patches (xnu-10063) and pins |
| `configs/piano_macos_defconfig`, `patches/xnu-12377/`, `macos/` | macOS 27.3 | Kernel config, patches (xnu-12377), pins and notes |

`unixnu --os <profile>` selects a system; iPadOS 17.7.11 is the default.
iPadOS is built by the SM8750 fork of UniXNU with the OnePlus 13T drivers,
macOS by the UniXNU core, which has the macOS profile.

## iPadOS profiles

UniXNU builds one `os_profile` per board. A new profile needs its own
defconfig, kernelcache hash, AMFI `early_security_init` address, trust cache
and root volume UUID, all taken from that IPSW. Add them only once the IPSW
is fetched and the values are measured; nothing is guessed here.

## macOS

State, build line and what is missing: [../macos/README.md](../macos/README.md).
