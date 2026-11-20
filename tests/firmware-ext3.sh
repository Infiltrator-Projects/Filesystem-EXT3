#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Boot a real Linux EFI stub via the V2-only EXT3 reader in disposable OVMF.
set -euo pipefail
export LC_ALL=C
boot=$(realpath "${1:?BOOTX64.EFI required}")
module=$(realpath "${2:?firmware EXT3.so required}")
kernel=$(realpath "${3:?Linux EFI stub image required}")
work=$(realpath -m "${4:?output directory required}")
mode=${5:-clean}
case "$mode" in clean|bad-superblock|needs-recovery|no-journal) ;; *) exit 2 ;; esac
ovmf=${OVMF_DIR:-/usr/share/OVMF}
mkdir -p "$work"
esp="$work/esp.img"
media="$work/ext3.img"
disk="$work/gpt.img"
tree="$work/root"
ram="$work/ram"
mkdir -p "$tree/System/Kernel/Images" "$ram"/{bin,dev,proc}
images="$tree/System/Kernel/Images"
cp "$kernel" "$images/native.img"
cp "$(command -v busybox)" "$ram/bin/"
ln -sf busybox "$ram/bin/sh"
cat > "$ram/init" <<'GUEST'
#!/bin/sh
set -eu
b=/bin/busybox
$b mount -t devtmpfs devtmpfs /dev
exec </dev/ttyS0 >/dev/ttyS0 2>&1
trap '/bin/busybox poweroff -f' EXIT
$b mount -t proc proc /proc
$b grep -qw ext3_so_trial /proc/cmdline
echo EXT3_COMMON_V2_LINUX_HANDOFF_PASS
GUEST
chmod +x "$ram/init"
(cd "$ram" && find . -print0 | cpio --null -o -H newc --owner=0:0 --quiet | gzip -1) > "$images/native.initrd"
# Test real file contents, not a hardcoded firmware or native reader.
cp "$kernel" "$tree/vmlinuz"
cp "$images/native.initrd" "$images/current.initrd"
truncate -s 128M "$media"
# Independently create a real ext3 namespace. Use 1024-byte allocation
# blocks to exercise indirect mappings and sector-aligned V2 BlockIo.
mkfs.ext3 -q -F -b 1024 -O none,has_journal,filetype,sparse_super,large_file -L EXT3ROOT -d "$tree" "$media"
if [[ $mode == needs-recovery ]]; then
    # Require journal replay: incompat FILETYPE (2) + RECOVER (4).
    # Editing a disposable copy is the entire point of this negative test.
    printf '\006\000\000\000' | dd of="$media" bs=1 seek=1120 conv=notrunc status=none
elif [[ $mode == no-journal ]]; then
    # Replace compat flags at superblock + 0x5c, removing HAS_JOURNAL.
    printf '\000\000\000\000' | dd of="$media" bs=1 seek=1116 conv=notrunc status=none
fi
if [[ $mode == bad-superblock ]]; then
    # EXT3 superblock starts at byte 1024; magic is at +56.
    printf '\000\000' | dd of="$media" bs=1 seek=1080 conv=notrunc status=none
fi
truncate -s 64M "$esp"
mkfs.fat -F32 "$esp"
mmd -i "$esp" ::/EFI ::/EFI/BOOT ::/EFI/BOOT/filesystems_x64
mcopy -i "$esp" "$boot" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$esp" "$module" ::/EFI/BOOT/filesystems_x64/EXT3.so
truncate -s 200M "$disk"
sgdisk --clear --new=1:2048:+64M --typecode=1:ef00 \
    --new=2:133120:+128M --typecode=2:8300 "$disk"
partuuid=$(LC_ALL=C sgdisk --info=2 "$disk" | awk '/Partition unique GUID: / { print $4 }')
[[ $partuuid =~ ^[[:xdigit:]]{8}(-[[:xdigit:]]{4}){3}-[[:xdigit:]]{12}$ ]]
cat > "$work/boot.conf" <<BOOT
timeout 1
textonly true
use_nvram false
driver_diagnostics true
scanfor manual
menuentry "InfiltratorOS kernel from EXT3.so" {
    volume $partuuid
    filesystem_module ext3
    loader \vmlinuz
    initrd \System\Kernel\Images\current.initrd
    linux_load memory
    ostype Linux
    options "console=ttyS0 rdinit=/init panic=1 ext3_so_trial"
}
BOOT
mcopy -i "$esp" "$work/boot.conf" ::/EFI/BOOT/
dd if="$esp" of="$disk" bs=1M seek=1 conv=notrunc status=none
dd if="$media" of="$disk" bs=1M seek=65 conv=notrunc status=none
cp "$ovmf/OVMF_VARS_4M.fd" "$work/vars.fd"
qemu=(qemu-system-x86_64 -machine q35,accel=tcg -cpu max -m 768 -display none
      -monitor none -net none -no-reboot -serial "file:$work/serial.log"
      -drive "if=pflash,format=raw,readonly=on,file=$ovmf/OVMF_CODE_4M.fd"
      -drive "if=pflash,format=raw,file=$work/vars.fd"
      -drive "file=$disk,format=raw,if=none,id=bootdisk"
      -device nvme,drive=bootdisk,serial=ext3-qualification)
if [[ -n ${QEMU_DATA_DIR:-} ]]; then
    qemu+=(-L "$QEMU_DATA_DIR")
fi
if [[ $mode != clean ]]; then
    result=0
    timeout 30 "${qemu[@]}" || result=$?
    [[ $result == 0 || $result == 124 ]]
    grep -Fq 'validated EXT3.so' "$work/serial.log"
    if grep -Fq 'EXT3_COMMON_V2_LINUX_HANDOFF_PASS' "$work/serial.log"; then
        echo 'BOOT read a kernel from invalid EXT3 journal/superblock metadata' >&2
        exit 1
    fi
    dd if="$disk" bs=1M skip=65 count=128 status=none | cmp - "$media"
    echo EXT3_COMMON_V2_INVALID_METADATA_REFUSED
    exit 0
fi
if ! timeout 120 "${qemu[@]}" ||
   ! grep -Fq 'EXT3_COMMON_V2_LINUX_HANDOFF_PASS' "$work/serial.log"; then
    tail -n 70 "$work/serial.log" >&2
    exit 1
fi
grep -Fq 'validated EXT3.so' "$work/serial.log"
grep -Fq "close /vmlinuz: $(stat -c %s "$kernel") bytes read through module" "$work/serial.log"
grep -Fq "close /System/Kernel/Images/current.initrd: $(stat -c %s "$images/native.initrd") bytes read through module" "$work/serial.log"
dd if="$disk" bs=1M skip=65 count=128 status=none | cmp - "$media"
echo EXT3_COMMON_V2_LINUX_HANDOFF_PASS
