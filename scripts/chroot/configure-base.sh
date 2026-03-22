#!/usr/bin/env bash
# NexusOS — Base chroot configuration
# Executed inside chroot after debootstrap.
# Arguments: none (environment is set by caller)
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
export DEBCONF_NONINTERACTIVE_SEEN=true
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

echo "[nexus] Configuring base system..."

# ── Locale ────────────────────────────────────────────────────────────────────
echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen
echo "es_ES.UTF-8 UTF-8" >> /etc/locale.gen
locale-gen
update-locale LANG=en_US.UTF-8

# ── Timezone ──────────────────────────────────────────────────────────────────
ln -sf /usr/share/zoneinfo/UTC /etc/localtime
echo "UTC" > /etc/timezone
dpkg-reconfigure -f noninteractive tzdata

# ── Hostname ──────────────────────────────────────────────────────────────────
echo "nexus" > /etc/hostname
cat > /etc/hosts << 'EOF'
127.0.0.1  localhost
127.0.1.1  nexus
::1        localhost ip6-localhost ip6-loopback
ff02::1    ip6-allnodes
ff02::2    ip6-allrouters
EOF

# ── Disable service start during build ───────────────────────────────────────
cat > /usr/sbin/policy-rc.d << 'EOF'
#!/bin/sh
exit 101
EOF
chmod +x /usr/sbin/policy-rc.d

# ── Update initramfs modules ──────────────────────────────────────────────────
# Live boot modules
cat > /etc/initramfs-tools/modules << 'EOF'
# NexusOS live boot modules
squashfs
overlay
loop
sr_mod
virtio
virtio_pci
virtio_blk
virtio_net
EOF

# Live initramfs configuration
cat > /etc/initramfs-tools/conf.d/nexus.conf << 'EOF'
MODULES=most
BUSYBOX=auto
COMPRESS=xz
BOOT=live
EOF

# ── Systemd configuration ─────────────────────────────────────────────────────
# Limit journal size
mkdir -p /etc/systemd/journald.conf.d
cat > /etc/systemd/journald.conf.d/nexus.conf << 'EOF'
[Journal]
SystemMaxUse=256M
RuntimeMaxUse=64M
Compress=yes
EOF

# Disable services that shouldn't run in live/lab
for svc in apt-daily.timer apt-daily-upgrade.timer \
            motd-news.service motd-news.timer; do
    systemctl disable "$svc" 2>/dev/null || true
done

echo "[nexus] Base configuration complete."
