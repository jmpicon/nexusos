#!/bin/sh
# NexusOS forensic live-config hook
# Activated by kernel parameter: nexus_mode=forensic
# This hook is run by live-config during boot.

NEXUS_MODE=$(grep -o 'nexus_mode=[a-z]*' /proc/cmdline | cut -d= -f2)

if [ "$NEXUS_MODE" != "forensic" ]; then
    exit 0
fi

# ── Forensic mode: disable automounting ───────────────────────────────────────
# Disable udisks2 automount
if command -v dbus-launch >/dev/null 2>&1; then
    dbus-launch gsettings set org.gnome.desktop.media-handling automount false \
        2>/dev/null || true
fi

# Disable automount via udev rules
cat > /etc/udev/rules.d/85-nexus-forensic-nomount.rules << 'EOF'
# NexusOS forensic mode — prevent automounting of evidence drives
ACTION=="add", SUBSYSTEM=="block", ENV{UDISKS_AUTOMOUNT_HINT}="never"
EOF

# ── Disable swap ──────────────────────────────────────────────────────────────
swapoff -a 2>/dev/null || true
# Prevent swap from being activated
systemctl mask swap.target 2>/dev/null || true

# ── Write-protect loop device ─────────────────────────────────────────────────
# The squashfs is already read-only; this reminds the analyst
echo "[NexusOS] Forensic mode active: automounting disabled, swap disabled"
echo "[NexusOS] Attached evidence drives will NOT be automatically mounted"
echo "[NexusOS] Mount evidence READ-ONLY: mount -o ro,noatime /dev/sdX /mnt/evidence"

# Write to MOTD
cat >> /etc/motd << 'MOTD'

  ┌──────────────────────────────────────────────────────┐
  │  FORENSIC MODE ACTIVE                                │
  │  - Automounting: DISABLED                            │
  │  - Swap: DISABLED                                    │
  │  - Evidence drives will NOT auto-mount               │
  │  Mount evidence: mount -o ro,noatime /dev/sdX /mnt  │
  └──────────────────────────────────────────────────────┘

MOTD
