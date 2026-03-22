#!/usr/bin/env bash
# NexusOS — Install build dependencies on Debian/Ubuntu host
# Run as root or with sudo.
set -euo pipefail

RESET='\033[0m'; GREEN='\033[32m'; YELLOW='\033[33m'; RED='\033[31m'; BOLD='\033[1m'

log_ok()   { echo -e "${GREEN}  ✓ ${*}${RESET}"; }
log_warn() { echo -e "${YELLOW}  ! ${*}${RESET}"; }
log_info() { echo -e "${BOLD}▶ ${*}${RESET}"; }
log_fail() { echo -e "${RED}  ✗ ${*}${RESET}"; exit 1; }

# ── Check we're root ──────────────────────────────────────────────────────────
if [[ $EUID -ne 0 ]]; then
    log_fail "This script must be run as root (use sudo)"
fi

# ── Detect OS ─────────────────────────────────────────────────────────────────
if ! command -v apt-get &>/dev/null; then
    log_fail "This script requires a Debian/Ubuntu host with apt-get"
fi

log_info "Updating package lists..."
apt-get update -qq

# ── Build system dependencies ─────────────────────────────────────────────────
log_info "Installing build system dependencies..."
apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    curl \
    wget

log_ok "Build tools installed"

# ── C++ compiler and libs ─────────────────────────────────────────────────────
log_info "Installing C++ toolchain and libraries..."
apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    clang \
    libc++-dev \
    libssl-dev \
    libyaml-cpp-dev

log_ok "C++ toolchain installed"

# ── ISO build tools ───────────────────────────────────────────────────────────
log_info "Installing ISO build tools..."
apt-get install -y --no-install-recommends \
    debootstrap \
    squashfs-tools \
    xorriso \
    grub-pc-bin \
    grub-efi-amd64-bin \
    grub-efi-amd64-signed \
    grub-common \
    dosfstools \
    mtools \
    rsync \
    isolinux \
    syslinux-common \
    btrfs-progs \
    e2fsprogs \
    gdisk

log_ok "ISO build tools installed"

# ── Live system tools ─────────────────────────────────────────────────────────
log_info "Installing live system tools..."
apt-get install -y --no-install-recommends \
    live-build \
    initramfs-tools \
    linux-image-amd64 \
    systemd \
    dbus

log_ok "Live system tools installed"

# ── QEMU for smoke testing (optional) ────────────────────────────────────────
log_info "Installing QEMU (smoke testing)..."
if apt-get install -y --no-install-recommends qemu-system-x86 qemu-utils 2>/dev/null; then
    log_ok "QEMU installed"
else
    log_warn "QEMU installation failed — smoke tests will be skipped"
fi

# ── Utilities ─────────────────────────────────────────────────────────────────
apt-get install -y --no-install-recommends \
    jq \
    yq \
    sha256sum \
    openssl \
    gpg \
    apt-transport-https \
    ca-certificates \
    lsb-release \
    file

log_ok "Utilities installed"

# ── Python (needed by some tools) ─────────────────────────────────────────────
apt-get install -y --no-install-recommends python3 python3-pip 2>/dev/null || true

echo ""
log_info "All dependencies installed successfully."
echo -e "${BOLD}Next steps:${RESET}"
echo "  cmake --preset debug"
echo "  cmake --build --preset debug"
echo "  sudo ./build/debug/src/cli/nexus doctor"
echo ""
