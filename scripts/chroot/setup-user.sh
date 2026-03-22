#!/usr/bin/env bash
# NexusOS — Create default nexus user inside chroot
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

NEXUS_USER="nexus"
NEXUS_PASS="nexus"
NEXUS_GROUPS="sudo,audio,video,plugdev,netdev,cdrom,floppy,dialout,kvm"

echo "[nexus] Creating user: ${NEXUS_USER}"

# Create user
if ! id "${NEXUS_USER}" &>/dev/null; then
    useradd \
        --create-home \
        --shell /bin/bash \
        --comment "NexusOS Default User" \
        "${NEXUS_USER}"
fi

# Set password
echo "${NEXUS_USER}:${NEXUS_PASS}" | chpasswd

# Add to groups (create them if they don't exist)
for grp in $(echo "${NEXUS_GROUPS}" | tr ',' ' '); do
    groupadd --force "${grp}" 2>/dev/null || true
    usermod --append --groups "${grp}" "${NEXUS_USER}" 2>/dev/null || true
done

# Configure sudo: passwordless for lab convenience
install -d -m 750 /etc/sudoers.d
cat > /etc/sudoers.d/nexus-user << 'EOF'
# NexusOS default user — passwordless sudo
# This is intentional for lab use; remove for hardened deployments
nexus ALL=(ALL:ALL) NOPASSWD: ALL
EOF
chmod 440 /etc/sudoers.d/nexus-user

# Set up home directory structure
HOME_DIR="/home/${NEXUS_USER}"
mkdir -p "${HOME_DIR}"/{Desktop,Documents,Downloads,Tools,Cases,.config,.local/bin}
chown -R "${NEXUS_USER}:${NEXUS_USER}" "${HOME_DIR}"

# Create a useful .bashrc
cat > "${HOME_DIR}/.bashrc" << 'BASHRC'
# ~/.bashrc — NexusOS default shell profile
# Source global definitions
if [[ -f /etc/bashrc ]]; then . /etc/bashrc; fi
if [[ -d /etc/profile.d ]]; then
    for f in /etc/profile.d/*.sh; do . "$f"; done
fi

# Prompt: shows user@host:cwd with color
PS1='\[\033[01;36m\]\u\[\033[0m\]@\[\033[01;34m\]\h\[\033[0m\]:\[\033[01;33m\]\w\[\033[0m\]\$ '

# History
HISTCONTROL=ignoredups:erasedups
HISTSIZE=50000
HISTFILESIZE=100000
shopt -s histappend
PROMPT_COMMAND="history -a; $PROMPT_COMMAND"

# Useful defaults
export EDITOR=nano
export PAGER=less
export LESS='-R'

# Include .local/bin
export PATH="$HOME/.local/bin:$PATH"
BASHRC
chown "${NEXUS_USER}:${NEXUS_USER}" "${HOME_DIR}/.bashrc"

echo "[nexus] User '${NEXUS_USER}' configured."
