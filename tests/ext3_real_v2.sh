#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Disposable real EXT3 image with source-owned journal metadata.
set -euo pipefail
test_exe=${1:?compiled Common V2 EXT3 acceptance executable required}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/root/boot"
printf '%s' 'EXT3 Common V2 real native namespace' > "$work/root/boot/kernel.bin"
truncate -s 128M "$work/ext3.img"
mkfs.ext3 -q -F -b 1024 -O none,has_journal,filetype,sparse_super,large_file \
  -d "$work/root" "$work/ext3.img"
before=$(sha256sum "$work/ext3.img")
"$test_exe" "$work/ext3.img"
test "$(sha256sum "$work/ext3.img")" = "$before"
echo EXT3-REAL-MEDIA-NONMUTATION-PASS
