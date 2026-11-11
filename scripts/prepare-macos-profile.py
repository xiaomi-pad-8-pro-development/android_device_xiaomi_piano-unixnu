#!/usr/bin/env python3
"""Write the macOS device folder into .cache/macos-device.

iPadOS and macOS are built by different UniXNU checkouts (the SM8750 fork and
the UniXNU core) and each fetch rewrites unixnu.lock. The macOS folder is this
repository through links, with macos/unixnu.lock as its lock, so the pins of
the two systems never overwrite each other.

    scripts/prepare-macos-profile.py
    ./unixnu fetch -D <repo>/.cache/macos-device --os macos-27.3-26C5035r
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LINKS = ('board.toml', 'feeds.toml', 'porting.toml', 'configs', 'dts', 'modules', 'package', 'patches', 'scripts')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / '.cache/macos-device')
    out = parser.parse_args().out
    out.mkdir(parents=True, exist_ok=True)
    links = {name: ROOT / name for name in LINKS}
    links['unixnu.lock'] = ROOT / 'macos/unixnu.lock'
    for name, source in links.items():
        target = out / name
        if target.is_symlink():
            target.unlink()
        elif target.exists():
            raise SystemExit(f'prepare-macos-profile: {target} exists and is not a link')
        if source.exists() or name == 'unixnu.lock':
            target.symlink_to(source, target_is_directory=source.is_dir())
    print(f'wrote {out}')


if __name__ == '__main__':
    main()
