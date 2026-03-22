# NexusOS — Roadmap

## v0.1.0 — Phantom (current)

- [x] C++20 CLI orchestrator (`nexus`)
- [x] CMake build system with presets
- [x] Profiles: core, analyst, forensic, blueteam, reverse, cloud, lab, full
- [x] Profile inheritance and YAML parsing
- [x] Hardened / lab mode switching
- [x] RootfsBuilder: debootstrap + packages + overlays
- [x] IsoBuilder: SquashFS + GRUB BIOS + GRUB UEFI + xorriso
- [x] HardeningEngine: sysctl + nftables + AppArmor + auditd + SSH
- [x] SHA-256 checksums and JSON manifests
- [x] QEMU smoke test
- [x] Catch2 unit + integration tests
- [x] CI via GitHub Actions
- [x] Documentation

## v0.2.0

- [ ] Signed packages: GPG key management for custom repos
- [ ] Incremental builds: skip debootstrap if rootfs hash unchanged
- [ ] Package version pinning for reproducible builds
- [ ] `nexus update` command: upgrade packages in existing rootfs
- [ ] Secure Boot: MOK enrolment, kernel + GRUB signing workflow
- [ ] SBOM generation via `syft` integration
- [ ] Persistence mode: write changes back to USB/disk

## v0.3.0

- [ ] `nexus profile show <name>` — detailed profile info with tool list
- [ ] `nexus doctor --fix` — auto-install missing dependencies
- [ ] ARM64 support (aarch64 ISO)
- [ ] Raspberry Pi 4/5 image variant
- [ ] Container image output (OCI/Docker) alongside ISO
- [ ] Live USB installer (calamares integration)

## v0.4.0

- [ ] Custom package mirror: built-in apt-cacher-ng for offline builds
- [ ] Forensic write-blocker integration (USB udev rules)
- [ ] Automated YARA rule updates at boot
- [ ] `nexus baseline` — system integrity baseline capture and diff
- [ ] XFCE4 theme and icon set (NexusOS branding)
- [ ] Greeter customisation (lightdm nexus theme)

## Long-term

- [ ] GUI frontend for nexus CLI (Electron or Qt)
- [ ] Cloud image variants (AWS AMI, Azure image, GCP image)
- [ ] NexusOS Update infrastructure (signed deltas)
- [ ] Community profile repository
- [ ] Kubernetes operator for lab deployment
