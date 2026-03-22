#!/usr/bin/env bash
# NexusOS — Write bootable USB with persistence partition
#
# Layout:
#   /dev/sdX1  → ISO data (dd from .iso)  read-only partition
#   /dev/sdX2  → persistence (ext4, label "persistence")
#
# Usage: write-usb.sh <iso_path> <device> [persistence_mb]
# Example: sudo write-usb.sh NexusOS-analyst-0.1.0-amd64.iso /dev/sdb 2048
#
# WARNING: This DESTROYS all data on <device>. Confirm before running.
set -euo pipefail

RESET='\033[0m'; GREEN='\033[32m'; YELLOW='\033[33m'; RED='\033[31m'; BOLD='\033[1m'

ISO="${1:?Usage: $0 <iso> <device> [persistence_mb]}"
DEVICE="${2:?Usage: $0 <iso> <device> [persistence_mb]}"
PERSIST_MB="${3:-2048}"

# ── Safety checks ─────────────────────────────────────────────────────────────
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}Must run as root${RESET}"; exit 1
fi

if [[ ! -f "$ISO" ]]; then
    echo -e "${RED}ISO not found: ${ISO}${RESET}"; exit 1
fi

if [[ ! -b "$DEVICE" ]]; then
    echo -e "${RED}Not a block device: ${DEVICE}${RESET}"; exit 1
fi

# Refuse to write to mounted filesystem root devices
if mount | grep -q "^${DEVICE} on / "; then
    echo -e "${RED}${DEVICE} is the system root device — aborting${RESET}"; exit 1
fi

ISO_SIZE_MB=$(( $(stat -c%s "$ISO") / 1024 / 1024 ))
DEVICE_SIZE_MB=$(( $(blockdev --getsize64 "$DEVICE") / 1024 / 1024 ))
REQUIRED_MB=$(( ISO_SIZE_MB + PERSIST_MB + 64 ))  # 64 MB buffer

echo -e "${BOLD}NexusOS — USB Writer${RESET}"
echo ""
echo "  ISO         : ${ISO} (${ISO_SIZE_MB} MB)"
echo "  Device      : ${DEVICE} (${DEVICE_SIZE_MB} MB)"
echo "  Persistence : ${PERSIST_MB} MB"
echo "  Required    : ${REQUIRED_MB} MB"
echo ""

if [[ $DEVICE_SIZE_MB -lt $REQUIRED_MB ]]; then
    echo -e "${RED}Device too small: need ${REQUIRED_MB} MB, have ${DEVICE_SIZE_MB} MB${RESET}"
    exit 1
fi

# Final confirmation
echo -e "${RED}${BOLD}WARNING: ALL DATA ON ${DEVICE} WILL BE DESTROYED.${RESET}"
echo -n "Type YES to continue: "
read -r CONFIRM
if [[ "$CONFIRM" != "YES" ]]; then
    echo "Aborted."
    exit 0
fi

echo ""

# ── Step 1: Write ISO to device ───────────────────────────────────────────────
echo -e "${BOLD}▶ Writing ISO to ${DEVICE}...${RESET}"
dd if="$ISO" of="$DEVICE" bs=4M status=progress conv=fsync oflag=direct
sync
echo -e "${GREEN}  ✓ ISO written${RESET}"

# ── Step 2: Determine ISO end sector ─────────────────────────────────────────
echo -e "${BOLD}▶ Analysing partition table...${RESET}"

# The ISO ends at a known size; calculate the next free sector
ISO_SECTORS=$(( ISO_SIZE_MB * 1024 * 1024 / 512 ))
# Add 1 MB alignment padding
PERSIST_START=$(( (ISO_SECTORS / 2048 + 1) * 2048 ))

echo "  ISO ends at sector : ${ISO_SECTORS}"
echo "  Persistence starts : ${PERSIST_START}"

# ── Step 3: Create persistence partition (sgdisk GPT or fdisk MBR) ────────────
echo -e "${BOLD}▶ Creating persistence partition...${RESET}"

# We append a new partition to the existing GPT/hybrid layout
# Use sgdisk if available (better for hybrid ISO GPT), otherwise parted
if command -v sgdisk &>/dev/null; then
    sgdisk \
        --new=2:${PERSIST_START}:0 \
        --change-name=2:"nexus-persistence" \
        --typecode=2:8300 \
        "$DEVICE"
elif command -v parted &>/dev/null; then
    PERSIST_START_MB=$(( PERSIST_START * 512 / 1024 / 1024 ))
    parted --script "$DEVICE" \
        mkpart primary ext4 "${PERSIST_START_MB}MiB" 100%
else
    echo -e "${RED}Neither sgdisk nor parted found. Install gdisk or parted.${RESET}"
    exit 1
fi

# Re-read partition table
partprobe "$DEVICE" 2>/dev/null || true
sleep 2

echo -e "${GREEN}  ✓ Partition created${RESET}"

# ── Step 4: Format persistence partition ─────────────────────────────────────
echo -e "${BOLD}▶ Formatting persistence partition (ext4)...${RESET}"

# Determine partition name (sdX2 or sdXp2)
if [[ -b "${DEVICE}2" ]]; then
    PERSIST_PART="${DEVICE}2"
elif [[ -b "${DEVICE}p2" ]]; then
    PERSIST_PART="${DEVICE}p2"
else
    # Wait a moment and try again
    sleep 2
    PERSIST_PART=$(lsblk -lno NAME "$DEVICE" | grep -E 'p?2$' | head -1)
    PERSIST_PART="/dev/${PERSIST_PART}"
fi

if [[ ! -b "$PERSIST_PART" ]]; then
    echo -e "${RED}Cannot find persistence partition — check device manually${RESET}"
    exit 1
fi

mkfs.ext4 -L "persistence" -F "$PERSIST_PART"
echo -e "${GREEN}  ✓ Formatted: ${PERSIST_PART} (label: persistence)${RESET}"

# ── Step 5: Write persistence.conf ───────────────────────────────────────────
echo -e "${BOLD}▶ Writing persistence.conf...${RESET}"

MNT=$(mktemp -d)
mount "$PERSIST_PART" "$MNT"
cat > "${MNT}/persistence.conf" << 'EOF'
/ union
EOF
umount "$MNT"
rmdir "$MNT"

echo -e "${GREEN}  ✓ persistence.conf written (/ union)${RESET}"

# ── Done ─────────────────────────────────────────────────────────────────────
sync
echo ""
echo -e "${GREEN}${BOLD}USB ready: ${DEVICE}${RESET}"
echo ""
echo "Boot options:"
echo "  Normal    : boot from USB — changes are NOT saved"
echo "  Persistent: boot from USB with 'persistence' kernel parameter"
echo "              Changes (apt upgrades, configs) survive reboot"
echo ""
echo "To update NexusOS on this USB:"
echo "  1. Boot with persistence"
echo "  2. sudo nexus-update apply"
echo "  3. Or: sudo nexus-update squashfs /dev/sdb  (full squashfs replace)"
echo ""
