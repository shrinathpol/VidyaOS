#!/bin/bash
set -e

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo)."
  exit 1
fi

TARGET_DEV=$1
if [ -z "$TARGET_DEV" ]; then
  echo "Usage: $0 <target_device> (e.g., /dev/sdb)"
  exit 1
fi

if [ ! -b "$TARGET_DEV" ]; then
  echo "Error: $TARGET_DEV is not a valid block device."
  exit 1
fi

# Safety check
echo "WARNING: All data on $TARGET_DEV will be DESTROYED!"
echo "Target device info:"
lsblk "$TARGET_DEV"
echo ""
read -p "Are you sure you want to proceed? (type 'yes' to confirm): " CONFIRM
if [ "$CONFIRM" != "yes" ]; then
  echo "Installation cancelled."
  exit 1
fi

echo "Unmounting existing partitions on $TARGET_DEV..."
for part in ${TARGET_DEV}*; do
  if [ -b "$part" ]; then
    umount "$part" 2>/dev/null || true
  fi
done

echo "Creating partitions on $TARGET_DEV..."
# MBR Partition table:
# Partition 1: boot (50M, bootable)
# Partition 2: rootfs A (2G)
# Partition 3: rootfs B (2G)
# Partition 4: data (remaining)
sfdisk "$TARGET_DEV" <<EOF
label: dos
device: $TARGET_DEV
unit: sectors

1 : start=        2048, size=      102400, type=83, bootable
2 : start=      104448, size=     4194304, type=83
3 : start=     4298752, size=     4194304, type=83
4 : start=     8493056, size=            , type=83
EOF

# Define partition paths (handling /dev/nvme0n1p1 vs /dev/sdb1)
if [[ "$TARGET_DEV" =~ "nvme" || "$TARGET_DEV" =~ "mmcblk" ]]; then
  PART1="${TARGET_DEV}p1"
  PART2="${TARGET_DEV}p2"
  PART3="${TARGET_DEV}p3"
  PART4="${TARGET_DEV}p4"
else
  PART1="${TARGET_DEV}1"
  PART2="${TARGET_DEV}2"
  PART3="${TARGET_DEV}3"
  PART4="${TARGET_DEV}4"
fi

echo "Formatting partitions..."
mkfs.ext4 -F -L boot "$PART1"
mkfs.ext4 -F -L rootfs_a "$PART2"
mkfs.ext4 -F -L rootfs_b "$PART3"
mkfs.ext4 -F -L data "$PART4"

echo "Mounting boot partition..."
MNT_DIR=$(mktemp -d)
mount "$PART1" "$MNT_DIR"

echo "Installing GRUB2 to $TARGET_DEV..."
grub-install --target=i386-pc --boot-directory="$MNT_DIR" "$TARGET_DEV"

echo "Writing grub.cfg..."
mkdir -p "$MNT_DIR/grub"
cat <<'EOF' > "$MNT_DIR/grub/grub.cfg"
set default="0"
set timeout=5

menuentry "VidyaOS (Slot A - Primary)" {
    search --no-floppy --set=root --label rootfs_a
    linux /boot/bzImage root=/dev/sda2 rw console=ttyS0 console=tty0 quiet
}

menuentry "VidyaOS (Slot B - Backup)" {
    search --no-floppy --set=root --label rootfs_b
    linux /boot/bzImage root=/dev/sda3 rw console=ttyS0 console=tty0 quiet
}
EOF

umount "$MNT_DIR"
rm -rf "$MNT_DIR"

# Copy rootfs images if they exist in buildroot output
IMAGES_DIR="buildroot/output/images"
if [ -d "$IMAGES_DIR" ]; then
  echo "Writing default rootfs images from buildroot output..."
  if [ -f "$IMAGES_DIR/rootfs.ext4" ]; then
    echo "Writing rootfs.ext4 to $PART2 (Slot A)..."
    dd if="$IMAGES_DIR/rootfs.ext4" of="$PART2" bs=4M status=progress conv=fsync
    echo "Writing rootfs.ext4 to $PART3 (Slot B)..."
    dd if="$IMAGES_DIR/rootfs.ext4" of="$PART3" bs=4M status=progress conv=fsync
    
    # Mount boot from rootfs and copy bzImage if needed
    MNT_BOOT=$(mktemp -d)
    MNT_ROOT=$(mktemp -d)
    mount "$PART1" "$MNT_BOOT"
    mount "$PART2" "$MNT_ROOT"
    
    echo "Copying bzImage to boot partition..."
    mkdir -p "$MNT_BOOT/boot"
    if [ -f "$MNT_ROOT/boot/bzImage" ]; then
      cp "$MNT_ROOT/boot/bzImage" "$MNT_BOOT/boot/bzImage"
    elif [ -f "$IMAGES_DIR/bzImage" ]; then
      cp "$IMAGES_DIR/bzImage" "$MNT_BOOT/boot/bzImage"
    fi
    
    umount "$MNT_ROOT"
    umount "$MNT_BOOT"
    rm -rf "$MNT_ROOT" "$MNT_BOOT"
  fi
else
  echo "Notice: Buildroot images not found. Partitions are formatted and GRUB is installed."
  echo "Please flash slot A/B manually using dd once rootfs.ext4 is compiled."
fi

echo "Installation complete!"
