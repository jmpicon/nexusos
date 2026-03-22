#!/usr/bin/env bash
# NexusOS — Generate bootable ISO from ISO tree
# Usage: generate-iso.sh <iso_tree> <output_iso> <volume_id>
set -euo pipefail

RESET='\033[0m'; GREEN='\033[32m'; RED='\033[31m'; BOLD='\033[1m'

ISO_TREE="${1:?Usage: $0 <iso_tree> <output_iso> <volume_id>}"
OUTPUT_ISO="${2:?Usage: $0 <iso_tree> <output_iso> <volume_id>}"
VOLUME_ID="${3:-NexusOS}"

OUTPUT_DIR="$(dirname "$OUTPUT_ISO")"
mkdir -p "$OUTPUT_DIR"

echo -e "${BOLD}▶ Generating ISO: ${OUTPUT_ISO}${RESET}"
echo "  Tree      : ${ISO_TREE}"
echo "  Volume ID : ${VOLUME_ID}"

# Determine if EFI image is available
EFI_ARGS=()
EFI_IMG="${ISO_TREE}/boot/grub/efi.img"
if [[ -f "$EFI_IMG" ]]; then
    EFI_ARGS=(
        "-eltorito-alt-boot"
        "-e" "boot/grub/efi.img"
        "-no-emul-boot"
        "--efi-boot-part"
        "--efi-boot-image"
    )
    echo "  EFI       : enabled"
fi

# Determine BIOS eltorito image
BIOS_ELTORITO="${ISO_TREE}/boot/grub/i386-pc/eltorito.img"
if [[ ! -f "$BIOS_ELTORITO" ]]; then
    # Try to create it
    if command -v grub-mkstandalone &>/dev/null; then
        mkdir -p "${ISO_TREE}/boot/grub/i386-pc"
        grub-mkstandalone \
            --format=i386-pc \
            --output="${BIOS_ELTORITO}" \
            --install-modules="linux normal iso9660 biosdisk memdisk search tar ls" \
            --modules="linux normal iso9660 biosdisk memdisk search configfile \
                       part_gpt part_msdos fat ext2" \
            --locales="" --fonts="" \
            "boot/grub/grub.cfg=${ISO_TREE}/boot/grub/grub.cfg" 2>/dev/null || true
    fi
fi

BIOS_ARGS=()
if [[ -f "$BIOS_ELTORITO" ]]; then
    BIOS_ARGS=(
        "-eltorito-boot" "boot/grub/i386-pc/eltorito.img"
        "-no-emul-boot"
        "-boot-load-size" "4"
        "-boot-info-table"
        "--grub2-boot-info"
    )
    # Add hybrid MBR if available
    HYBRID_MBR="/usr/lib/grub/i386-pc/boot_hybrid.img"
    [[ -f "$HYBRID_MBR" ]] && BIOS_ARGS+=("--grub2-mbr" "$HYBRID_MBR")
    echo "  BIOS      : enabled"
fi

# Run xorriso
xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -joliet \
    -joliet-long \
    -rational-rock \
    -volid "${VOLUME_ID:0:32}" \
    -appid "NexusOS" \
    -publisher "NexusOS Project" \
    -preparer "nexus-builder" \
    -partition_offset 16 \
    "${BIOS_ARGS[@]}" \
    "${EFI_ARGS[@]}" \
    -output "$OUTPUT_ISO" \
    "$ISO_TREE"

SIZE=$(du -sh "$OUTPUT_ISO" | cut -f1)
echo -e "${GREEN}  ✓ ISO generated: ${OUTPUT_ISO} (${SIZE})${RESET}"

# Generate SHA-256 checksum
sha256sum "$OUTPUT_ISO" > "${OUTPUT_ISO}.sha256"
echo -e "${GREEN}  ✓ Checksum: ${OUTPUT_ISO}.sha256${RESET}"
