#!/bin/bash
# NexusOS ISO Builder — run with: sudo bash scripts/build-iso-now.sh
# Builds a bootable NexusOS ISO ready for USB, VirtualBox, or bare-metal install.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
NEXUS_BIN="$PROJECT_DIR/build/release/src/cli/nexus"
OUTPUT_DIR="$PROJECT_DIR/release"
PROFILE="${1:-analyst}"
MODE="${2:-lab}"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
log()  { echo -e "${CYAN}[nexus-build]${NC} $*"; }
ok()   { echo -e "${GREEN}[  OK  ]${NC} $*"; }
warn() { echo -e "${YELLOW}[ WARN ]${NC} $*"; }
die()  { echo -e "${RED}[FAILED]${NC} $*"; exit 1; }

# ── Must run as root ──────────────────────────────────────────────────────────
[[ $EUID -eq 0 ]] || die "Run as root: sudo bash $0 [profile] [mode]"

log "NexusOS ISO Builder"
log "Profile : $PROFILE"
log "Mode    : $MODE"
log "Output  : $OUTPUT_DIR"
echo ""

# ── 1. Install build dependencies ────────────────────────────────────────────
log "Installing build dependencies..."
apt-get update -qq
apt-get install -y --no-install-recommends \
    debootstrap \
    xorriso \
    squashfs-tools \
    grub-pc-bin \
    grub-efi-amd64-bin \
    grub-common \
    dosfstools \
    rsync \
    live-boot \
    live-boot-initramfs-tools \
    live-config \
    live-config-systemd \
    initramfs-tools \
    mtools \
    isolinux \
    syslinux-common \
    2>/dev/null
ok "Dependencies installed"

# ── 2. Build nexus CLI if not already built ───────────────────────────────────
if [[ ! -x "$NEXUS_BIN" ]]; then
    log "Building nexus CLI..."
    cd "$PROJECT_DIR"
    # Build as the actual user, not root
    REAL_USER="${SUDO_USER:-$USER}"
    sudo -u "$REAL_USER" cmake --preset release -DNEXUS_ENABLE_TESTS=OFF \
        > /tmp/nexus-cmake.log 2>&1 || die "CMake configure failed — check /tmp/nexus-cmake.log"
    sudo -u "$REAL_USER" cmake --build --preset release -- -j"$(nproc)" \
        > /tmp/nexus-build.log 2>&1 || die "Build failed — check /tmp/nexus-build.log"
    ok "nexus CLI built: $NEXUS_BIN"
else
    ok "nexus CLI already built: $NEXUS_BIN"
fi

# ── 3. Run the full ISO pipeline ──────────────────────────────────────────────
log "Starting ISO build pipeline (this takes 20-60 min depending on internet speed)..."
log "Pipeline: debootstrap → apt packages → overlays → squashfs → grub → xorriso"
echo ""

mkdir -p "$OUTPUT_DIR"

"$NEXUS_BIN" -C "$PROJECT_DIR" release \
    --profile "$PROFILE" \
    --mode "$MODE" \
    --output "$OUTPUT_DIR" \
    --skip-smoke \
    --jobs "$(nproc)"

# ── 4. Show result ────────────────────────────────────────────────────────────
echo ""
ISO=$(ls "$OUTPUT_DIR"/NexusOS-*.iso 2>/dev/null | head -1)
if [[ -n "$ISO" ]]; then
    SIZE=$(du -sh "$ISO" | cut -f1)
    ok "ISO built successfully!"
    echo ""
    echo -e "  ${GREEN}File   :${NC} $ISO"
    echo -e "  ${GREEN}Size   :${NC} $SIZE"
    echo ""
    echo -e "  ${CYAN}── Write to USB ──────────────────────────────────────────────────────${NC}"
    echo -e "  sudo bash scripts/iso/write-usb.sh $ISO /dev/sdX 2048"
    echo ""
    echo -e "  ${CYAN}── VirtualBox / VMware ───────────────────────────────────────────────${NC}"
    echo -e "  New VM → Linux 64-bit → attach ISO as optical drive → boot"
    echo ""
    echo -e "  ${CYAN}── SHA-256 checksum ─────────────────────────────────────────────────${NC}"
    sha256sum "$ISO"
else
    die "ISO not found in $OUTPUT_DIR — check build output above"
fi
