<div align="center">

```
  ███╗   ██╗███████╗██╗  ██╗██╗   ██╗███████╗
  ████╗  ██║██╔════╝╚██╗██╔╝██║   ██║██╔════╝
  ██╔██╗ ██║█████╗   ╚███╔╝ ██║   ██║███████╗
  ██║╚██╗██║██╔══╝   ██╔██╗ ██║   ██║╚════██║
  ██║ ╚████║███████╗██╔╝ ██╗╚██████╔╝███████║
  ╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝
```

# NexusOS

**Professional Cybersecurity Linux Distribution**

*Codename Phantom · Debian 12 Bookworm · amd64*

[![Build](https://github.com/jmpicon/NexusOS/actions/workflows/build.yml/badge.svg)](https://github.com/jmpicon/NexusOS/actions)
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Debian](https://img.shields.io/badge/Base-Debian%2012%20Bookworm-red.svg)](https://www.debian.org/)

</div>

---

NexusOS is a **Debian 12 Bookworm-based Linux distribution** for professional cybersecurity
work. Unlike other security distributions, NexusOS is built around a **C++20 CLI orchestrator**
(`nexus`) that manages the full lifecycle: from base system bootstrap to bootable ISO generation,
hardening, live USB persistence, and OTA updates.

Designed for:
- Digital forensics and incident response (DFIR)
- Defensive security and threat hunting
- Reverse engineering and binary analysis
- Cloud and container security (K8s, OCI)
- Authorised security auditing and penetration testing
- Security education and laboratory environments

---

## Key Features

| Feature | Implementation |
|---------|---------------|
| **C++20 build orchestrator** | `nexus` CLI — profiles, pipeline, ISO, hardening |
| **Modular profile system** | YAML profiles with inheritance (`core` → `analyst` → `forensic` …) |
| **Dual security modes** | `hardened` (deny-all, strict) / `lab` (flexible, virtualisation-ready) |
| **Hybrid ISO** | BIOS + UEFI, El Torito, hybrid MBR — boots from CD, USB, or VM |
| **Live persistence** | USB with persistence partition — `apt upgrade` survives reboot |
| **OTA updates** | `nexus-update` tool inside live system — patch or replace squashfs |
| **AppArmor + nftables** | Mandatory access control and stateful firewall by default |
| **auditd** | Comprehensive syscall and file-access auditing |
| **Reproducible builds** | Deterministic within a Debian snapshot; SBOM-ready |
| **Catch2 test suite** | Unit + integration tests for all pipeline components |

---

## Profiles

| Profile | Description | Size (est.) | GUI |
|---------|-------------|-------------|-----|
| `core` | Minimal base — kernel, init, networking | ~800 MB | No |
| `analyst` | Network analysis: nmap, wireshark, tshark, zeek, suricata | ~2.5 GB | XFCE |
| `forensic` | DFIR: sleuthkit, volatility3, yara, binwalk, foremost | ~3.5 GB | XFCE |
| `blueteam` | Defensive: lynis, osquery, openscap, trivy, prometheus | ~4 GB | XFCE |
| `reverse` | RE/binary: ghidra, radare2, gdb, strace, valgrind | ~3 GB | XFCE |
| `cloud` | Cloud/K8s: kubectl, helm, trivy, grype, terraform, podman | ~3.5 GB | XFCE |
| `lab` | Malware lab: qemu-kvm, virt-manager, firejail, bubblewrap | ~5 GB | XFCE |
| `full` | All profiles combined | ~8 GB | XFCE |

---

## Quick Start

### 1 — Build dependencies (Debian/Ubuntu host)

```bash
sudo bash scripts/bootstrap/install-deps.sh
```

### 2 — Build the nexus CLI

```bash
cmake --preset debug
cmake --build --preset debug
```

### 3 — Validate environment

```bash
sudo ./build/debug/src/cli/nexus doctor
```

### 4 — Build an ISO

```bash
# Full release pipeline
sudo ./build/debug/src/cli/nexus release \
    --profile analyst \
    --mode lab \
    --output ./release

# Or step by step:
sudo nexus build-rootfs --profile analyst --mode lab
sudo nexus harden       --mode lab --profile analyst
sudo nexus build-iso    --profile analyst --output ./release
nexus verify            ./release/NexusOS-analyst-0.1.0-amd64.iso
nexus manifest          --profile analyst --mode lab
```

### 5 — Write to USB with persistence

```bash
sudo bash scripts/iso/write-usb.sh \
    ./release/NexusOS-analyst-0.1.0-amd64.iso \
    /dev/sdX \
    2048   # persistence partition size in MB
```

Boot the USB. To keep changes across reboots, select **"Live + Persistence"** from the GRUB menu.

### 6 — Update the live system

```bash
# Inside the running NexusOS live system:
nexus-update check          # check for updates
sudo nexus-update apply     # apply apt upgrades (requires persistence)
nexus-update status         # show version and persistence info

# Full squashfs replacement from update server:
sudo nexus-update squashfs /dev/sdX
```

---

## Architecture

```
nexus CLI (C++20)
│
├── Orchestrator ──────────── coordinates pipeline stages
│   ├── ConfigParser          nexus.yaml (yaml-cpp)
│   ├── ProfileManager        profiles/*.yaml with inheritance
│   ├── RootfsBuilder         debootstrap → apt → overlays → branding → initramfs
│   ├── IsoBuilder            mksquashfs → grub-mkstandalone → xorriso
│   ├── HardeningEngine       sysctl / nftables / AppArmor / auditd / SSH
│   ├── PackageManager        apt-get inside chroot (batched, noninteractive)
│   ├── BrandingManager       motd / os-release / aliases / wallpaper
│   └── SmokeTester           QEMU headless boot test
│
├── Utils
│   ├── Logger               thread-safe, coloured, multi-sink
│   ├── Process              fork/exec, chroot_run, bind_mount
│   ├── Checksum             SHA-256 via OpenSSL EVP
│   └── Manifest             JSON build manifest (nlohmann/json)
│
└── Live update system
    ├── nexus-update          in-system update tool (persistence + squashfs OTA)
    ├── write-usb.sh          ISO + persistence partition writer
    └── update manifest       JSON format for server-side update distribution
```

### ISO Build Pipeline

```
debootstrap (bookworm, minbase)
  → apt-get install (profile packages, batched, noninteractive)
  → rsync overlays/ into rootfs
  → branding (motd, os-release, aliases)
  → useradd nexus / sudoers
  → update-initramfs -c -k <kver>
  → mksquashfs -comp xz -b 1048576
  → grub-mkstandalone --format=i386-pc  (BIOS El Torito)
  → grub-mkstandalone --format=x86_64-efi + mkdosfs (UEFI ESP)
  → xorriso -as mkisofs (hybrid ISO 9660 + GPT + MBR)
  → sha256sum sidecar + JSON manifest
```

---

## Security Architecture

### Hardened mode
- **nftables**: deny all inbound + forward, allow established
- **sysctl**: `kptr_restrict=2`, `ptrace_scope=2`, BPF disabled, ASLR=2, core disabled
- **AppArmor**: enforcing, service enabled at boot (`apparmor=1 security=apparmor`)
- **auditd**: immutable rules (`-e 2`), authentication, privilege escalation, mounts
- **Root**: locked (`passwd --lock`), empty `/etc/securetty`
- **sudo**: requires password (nexus-hardened overlay)
- **SSH**: keys only, no root, strict ciphers (chacha20-poly1305, aes256-gcm)

### Lab mode
- nftables stateful but permissive (forwarding enabled for VMs)
- ptrace_scope=1, no BPF restriction
- Passwordless sudo (convenience for lab VMs)
- Password SSH (for lab-to-lab access)
- auditd active but not immutable

---

## Repository Structure

```
nexus/
├── CMakeLists.txt          Root build (CMake 3.25+, C++20, FetchContent)
├── CMakePresets.json       Presets: debug, release, ci, asan
├── Makefile                Convenience wrapper
├── nexus.yaml              Build configuration
├── src/
│   ├── cli/                main.cpp + all subcommands (CLI11)
│   ├── core/               Orchestrator — pipeline coordination
│   ├── builder/            RootfsBuilder + IsoBuilder
│   ├── config/             ConfigParser (yaml-cpp)
│   ├── profiles/           ProfileManager (YAML inheritance)
│   ├── security/           HardeningEngine
│   ├── packaging/          PackageManager (apt in chroot)
│   ├── branding/           BrandingManager
│   ├── testing/            SmokeTester (QEMU)
│   └── utils/              Logger, Process, Checksum, Manifest
├── include/nexus/          common.hpp (Result<T>, BuildOptions)
├── profiles/               core, analyst, forensic, blueteam, reverse, cloud, lab, full
├── packages/lists/         Per-profile package lists
├── overlays/               common, hardened, lab, forensic, analyst…
├── scripts/
│   ├── bootstrap/          install-deps.sh
│   ├── build/              check-env.sh
│   ├── chroot/             configure-base.sh, setup-user.sh
│   ├── iso/                build-squashfs.sh, generate-iso.sh, write-usb.sh
│   ├── test/               smoke-test-qemu.sh, validate-rootfs.sh
│   └── release/            generate-checksums.sh
├── assets/boot/grub/       GRUB template (BIOS + UEFI + persistence entries)
├── tests/unit/             Catch2 unit tests
├── tests/integration/      Integration tests
├── ci/                     Dockerfile.build + GitHub Actions
└── docs/                   architecture, build, profiles, hardening, iso-pipeline,
                            cli, testing, release, troubleshooting, legal, roadmap
```

---

## Build Requirements

| Requirement | Package | Notes |
|-------------|---------|-------|
| CMake ≥ 3.25 | `cmake` | |
| Ninja | `ninja-build` | |
| debootstrap | `debootstrap` | Debian only |
| mksquashfs | `squashfs-tools` | |
| xorriso | `xorriso` | |
| grub-mkstandalone | `grub-common` | |
| GRUB BIOS | `grub-pc-bin` | |
| GRUB UEFI | `grub-efi-amd64-bin` | |
| mkdosfs | `dosfstools` | |
| OpenSSL | `libssl-dev` | SHA-256 |
| yaml-cpp | `libyaml-cpp-dev` | |
| QEMU (optional) | `qemu-system-x86` | smoke tests |

**Host OS**: Debian 12 Bookworm or Ubuntu 22.04+, amd64.
**Build requires root** for debootstrap and chroot operations.

---

## CLI Reference

```
nexus doctor                         Validate build environment
nexus init    -p <profile>           Initialise workspace
nexus build-rootfs -p <p> -m <mode> Build root filesystem
nexus harden  -m <mode>              Apply hardening
nexus build-iso -p <p> -o <dir>     Generate bootable ISO
nexus verify  <iso>                  Verify SHA-256 checksum
nexus manifest -p <p>               Generate JSON manifest
nexus smoke-test <iso>              QEMU boot test
nexus clean                         Clean workspace
nexus release -p <p> -m <mode>      Full pipeline

Modes: hardened | lab
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [docs/architecture.md](docs/architecture.md) | System architecture and C++ design |
| [docs/build.md](docs/build.md) | Build guide and configuration |
| [docs/profiles.md](docs/profiles.md) | Profile system and customisation |
| [docs/hardening.md](docs/hardening.md) | Security hardening reference |
| [docs/iso-pipeline.md](docs/iso-pipeline.md) | ISO build pipeline details |
| [docs/cli.md](docs/cli.md) | CLI command reference |
| [docs/testing.md](docs/testing.md) | Testing guide |
| [docs/release.md](docs/release.md) | Release process |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Common issues |
| [docs/legal-and-ethical-use.md](docs/legal-and-ethical-use.md) | Legal policy |
| [docs/roadmap.md](docs/roadmap.md) | Roadmap |

---

## Legal Notice

NexusOS is designed exclusively for **authorised security research, DFIR, defensive
security operations, and education**. All included tools are for defensive, forensic,
and authorised analysis purposes only.

See [docs/legal-and-ethical-use.md](docs/legal-and-ethical-use.md) for the full policy.

---

## License

NexusOS build tooling and overlays are released under [GPL-2.0](LICENSE).
Included third-party tools retain their respective licences.

---

<div align="center">
Built with precision. Designed for defenders.
</div>
