# NexusOS — Architecture

## Overview

NexusOS is a Debian 12 Bookworm-based Linux distribution for professional
cybersecurity work. The build system is a C++20 CLI tool (`nexus`) that
orchestrates the full pipeline from base system bootstrap to bootable ISO.

## Component Diagram

```
nexus (CLI)
│
├── Orchestrator          ← coordinates pipeline stages
│   ├── ConfigParser      ← loads nexus.yaml
│   ├── ProfileManager    ← loads profiles/*.yaml (with inheritance)
│   ├── PackageManager    ← apt-get inside chroot
│   ├── RootfsBuilder     ← debootstrap + configure + overlays
│   ├── HardeningEngine   ← sysctl / nftables / AppArmor / auditd
│   ├── BrandingManager   ← motd / os-release / aliases / wallpaper
│   ├── IsoBuilder        ← squashfs + GRUB + xorriso
│   └── SmokeTester       ← QEMU boot + rootfs validation
│
├── Utils
│   ├── Logger            ← thread-safe, coloured, multi-sink
│   ├── Process           ← fork/exec + chroot_run + mount helpers
│   ├── Checksum          ← SHA-256 via OpenSSL EVP
│   └── Manifest          ← JSON build manifest (nlohmann/json)
│
└── External deps (FetchContent)
    ├── yaml-cpp           ← YAML profile parsing
    ├── nlohmann/json      ← JSON manifests
    ├── CLI11              ← argument parsing
    ├── fmt                ← text formatting
    └── Catch2             ← unit + integration tests
```

## Build Pipeline

```
doctor          → validate host tools and environment
│
init            → create workspace dirs, write nexus.yaml
│
build-rootfs
  ├── debootstrap          debootstrap(1) → minimal Debian in /tmp/nexus-workspace/rootfs
  ├── mount-vfs            mount /proc /sys /dev inside chroot
  ├── configure-base       apt sources, locale, timezone, hostname, fstab
  ├── install-packages     apt-get install (batched, noninteractive)
  ├── apply-overlays       rsync overlays/{common,profile,mode}/ → rootfs/
  ├── apply-branding       motd, os-release, lsb-release, aliases
  ├── setup-user           useradd nexus + sudoers
  ├── generate-initramfs   update-initramfs -c -k <kver>
  └── umount-vfs
│
harden
  ├── apply-sysctl         write /etc/sysctl.d/99-nexus-*.conf
  ├── configure-apparmor   enable apparmor.service
  ├── configure-auditd     write /etc/audit/rules.d/nexus.rules
  ├── configure-nftables   write /etc/nftables.conf
  ├── harden-systemd       systemd defaults (hardened mode)
  └── configure-sshd       /etc/ssh/sshd_config.d/nexus.conf
│
build-iso
  ├── squashfs             mksquashfs rootfs → filesystem.squashfs (XZ, 1M blocks)
  ├── prepare-iso-tree     layout: iso/{boot/grub/, live/, EFI/BOOT/}
  ├── grub-bios            grub-mkstandalone --format=i386-pc → eltorito.img
  ├── grub-uefi            grub-mkstandalone --format=x86_64-efi → BOOTX64.EFI
  │                        mkdosfs → efi.img (FAT32 ESP)
  └── xorriso              xorriso -as mkisofs (hybrid ISO 9660 + GPT)
│
manifest        → SHA-256 + JSON manifest in release/
smoke-test      → QEMU -nographic -serial file: boot verification
```

## Profile System

Profiles are YAML files in `profiles/`. They support inheritance via `extends`.
The `ProfileManager` resolves inheritance at load time, merging package lists
and overlay lists (parent first, child overrides).

```yaml
# profiles/forensic.yaml
name: forensic
extends:
  - analyst     # inherits analyst which extends core

packages:
  - sleuthkit
  - volatility3

packages_hardened:
  - aide

overlays:
  - forensic
```

Resolution order: `core` → `analyst` → `forensic` (packages merged, no dups)

## Overlay System

Overlays are directories mirroring the rootfs structure. `rsync -a` copies them
into the rootfs in order:

```
overlays/common/    → always applied first
overlays/<profile>/ → profile-specific config
overlays/hardened/  → only in hardened mode
overlays/lab/       → only in lab mode
```

## Hardening Design

Two modes:

**hardened** — production-like:
- nftables: deny all inbound, deny forward, allow established
- sysctl: kptr_restrict=2, ptrace_scope=2, BPF disabled, core disabled
- AppArmor: enforcing, service enabled
- auditd: immutable rules (`-e 2`), comprehensive syscall coverage
- Root login disabled (passwd --lock root, empty /etc/securetty)
- Sudo: requires password (nexus-hardened overlay)
- SSH: keys only, no root, strict ciphers

**lab** — isolated lab use:
- nftables: stateful but permissive, forwarding enabled
- sysctl: moderate (ptrace_scope=1, no BPF restriction)
- AppArmor: enabled but more permissive profiles
- auditd: active but not immutable
- Sudo: passwordless (convenience)
- SSH: password auth permitted (for lab VMs)

## ISO Boot

The ISO supports both BIOS and UEFI:

```
BIOS:  El Torito boot record → grub/i386-pc/eltorito.img
       Hybrid MBR via boot_hybrid.img
UEFI:  EFI System Partition (FAT32 image) → EFI/BOOT/BOOTX64.EFI
       GRUB x86_64-efi standalone image
```

GRUB boots via `boot=live` parameter (live-boot + live-config). The squashfs
is at `/live/filesystem.squashfs` on the ISO. `live-boot` mounts it read-only
with an overlay tmpfs, providing a fully writable environment from RAM.

## C++ Design Principles

- **RAII throughout**: resources (file handles, mounts) released in destructors
- **Result<T,E>**: explicit error propagation without exceptions in I/O paths
- **No global mutable state**: Logger is the only singleton, mutex-protected
- **Process isolation**: all rootfs mutations happen via fork/exec (never eval)
- **Dry-run safe**: `--dry-run` skips filesystem-mutating steps
- **Testable**: all components have injectable dependencies (config, paths)
