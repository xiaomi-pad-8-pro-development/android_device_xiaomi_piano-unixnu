#!/bin/sh
# Re-apply the piano fixes that live outside this repository after every
# `./unixnu fetch` (a refetch restores the upstream copies).
#
#   scripts/apply-local-fixes.sh <UniXNU checkout, e.g. UniXNU-sm8750>
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
unixnu=${1:?UniXNU checkout}

apply() {  # <dir> <patch>
	if git -C "$1" apply --reverse --check "$2" 2>/dev/null; then
		echo "already applied: $(basename "$2") in $1"
	else
		git -C "$1" apply "$2" && echo "applied: $(basename "$2") in $1"
	fi
}

# SID 0x60 takeover for NativeUfs, in the fetched SM8750 common feed.
for feed in "$unixnu"/out/feeds/UniXNU-sm8750-common-*; do
	[ -d "$feed" ] && apply "$feed" "$here/patches/local/sm8750-common-smmu-piano.patch"
done

# Stage 7 display: NativeGOPPresenter is linked once, not twice.
apply "$unixnu" "$here/patches/local/sm8750-stage7-display-once.patch"

# Adreno830 needs ArmSmmu4K e923329 (feeds.toml); checked out under .cache.
smmu="$here/.cache/kits/ArmSmmu4K"
smmu_commit=e923329e74f18b11e63c155e470c970ebad3b6e2
[ -d "$smmu/.git" ] || git clone -q --no-checkout https://github.com/UniXNU/ArmSmmu4K.git "$smmu"
git -C "$smmu" cat-file -e "$smmu_commit^{commit}" 2>/dev/null ||
	git -C "$smmu" fetch -q https://github.com/UniXNU/ArmSmmu4K.git "$smmu_commit"
git -C "$smmu" checkout -q --detach "$smmu_commit"

# UniXNU core (macOS): accept the stock dtb, several compiled trees back to
# back. Only for the core checkout; the SM8750 fork already does.
if [ -f "$unixnu/profiles/os/macos-27.3-26C5035r/profile.toml" ]; then
	apply "$unixnu" "$here/patches/local/unixnu-bootimg-concatenated-dtb.patch"
fi

# launchd needs the original 21H461 trust cache (sha256 below, same bundle as
# the OnePlus 13T). Without it: "pid=1 launchd exec MAC policy REFUSED".
want=7416ca4f8377146626bb284e2133c76bee6cb4e3ee0af5d648b186235a7323a7
grep -q '^trust_cache *=' "$unixnu/unixnu.local.toml" 2>/dev/null ||
	echo "missing: set trust_cache in $unixnu/unixnu.local.toml to original-trust.bin" \
	     "(UniXNU-Pagani_SM8750/scripts/prepare-original-trust.py, sha256 $want)"
