#!/usr/bin/env bash
# NexusOS — Build SquashFS from rootfs
# Usage: build-squashfs.sh <rootfs_dir> <output_file>
set -euo pipefail

RESET='\033[0m'; GREEN='\033[32m'; BOLD='\033[1m'

ROOTFS="${1:?Usage: $0 <rootfs_dir> <output_file>}"
OUTPUT="${2:?Usage: $0 <rootfs_dir> <output_file>}"

OUTPUT_DIR="$(dirname "$OUTPUT")"
mkdir -p "$OUTPUT_DIR"

echo -e "${BOLD}▶ Building SquashFS${RESET}"
echo "  Source : ${ROOTFS}"
echo "  Output : ${OUTPUT}"

# Remove old squashfs if present
[[ -f "$OUTPUT" ]] && rm -f "$OUTPUT"

# Build squashfs with XZ compression
mksquashfs \
    "$ROOTFS" \
    "$OUTPUT" \
    -comp xz \
    -Xdict-size 1M \
    -b 1048576 \
    -noappend \
    -no-progress \
    -wildcards \
    -e proc \
    -e sys \
    -e dev \
    -e run \
    -e tmp \
    -e "var/cache/apt/archives/*.deb" \
    -e "var/lib/apt/lists/*"

# Report size
SIZE=$(du -sh "$OUTPUT" | cut -f1)
echo -e "${GREEN}  ✓ SquashFS built: ${OUTPUT} (${SIZE})${RESET}"
