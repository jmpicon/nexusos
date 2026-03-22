# NexusOS — CLI Reference

## Synopsis

```
nexus [global-options] <command> [command-options]
```

## Global Options

| Option          | Description                              |
|-----------------|------------------------------------------|
| `-v, --verbose` | Enable DEBUG-level log output            |
| `-n, --dry-run` | Show pipeline without executing steps    |
| `--no-color`    | Disable ANSI colour output               |
| `-q, --quiet`   | Suppress most output                     |
| `-V, --version` | Print version and exit                   |

---

## Commands

### `nexus doctor`

Validate the build environment. Checks:
- Required tools (debootstrap, mksquashfs, xorriso, grub-mkstandalone, …)
- Optional tools (qemu-system-x86_64, syft, cosign)
- Available profiles
- Configuration values
- Disk space

```bash
nexus doctor
```

Exit code 0 = all required tools present.
Exit code 1 = one or more required tools missing.

---

### `nexus init`

Initialise workspace directories and write a default `nexus.yaml`:

```bash
nexus init [--profile core]
```

Options:
- `-p, --profile <name>` — Target profile (informational only)

Creates: workspace dir, output dir, cache dir, `nexus.yaml` (if absent).

---

### `nexus build-rootfs`

Bootstrap and configure the root filesystem.

```bash
sudo nexus build-rootfs --profile analyst --mode lab
sudo nexus build-rootfs --profile forensic --mode hardened --mirror http://ftp.es.debian.org/debian
```

Options:
- `-p, --profile <name>` — Profile name (**required**)
- `-m, --mode <mode>` — `hardened` or `lab` (default: `lab`)
- `--mirror <url>` — Debian mirror URL
- `--workspace <dir>` — Override workspace directory
- `-j, --jobs <n>` — Parallel apt jobs (default: 4)

This command requires root. Runtime: 10-40 minutes depending on profile.

Pipeline stages:
1. Prepare workspace directories
2. Check required build tools
3. Run `debootstrap --variant=minbase`
4. Mount `/proc`, `/sys`, `/dev` inside chroot
5. Configure `apt`, locale, timezone, hostname, fstab
6. Install packages (batched, `DEBIAN_FRONTEND=noninteractive`)
7. Apply overlays (`common`, profile, mode)
8. Apply branding (motd, os-release, aliases)
9. Create `nexus` user
10. Run post-packages hook (if defined in profile)
11. Run `update-initramfs -c -k <kver>`
12. Unmount chroot VFS

---

### `nexus harden`

Apply hardening to an existing rootfs.

```bash
sudo nexus harden --mode hardened --profile analyst
```

Options:
- `-m, --mode <mode>` — **required**: `hardened` or `lab`
- `-p, --profile <name>` — Profile name
- `--workspace <dir>` — Override workspace

Stages: sysctl → AppArmor → auditd → nftables → systemd → PAM → sshd

---

### `nexus build-iso`

Build a bootable ISO from the rootfs.

```bash
sudo nexus build-iso --profile analyst --output ./release
```

Options:
- `-p, --profile <name>` — Profile name (**required**)
- `-m, --mode <mode>` — `hardened` or `lab` (default: `lab`)
- `-o, --output <dir>` — Output directory (default: `./release`)
- `--workspace <dir>` — Override workspace

Output file: `<output>/NexusOS-<profile>-<version>-<arch>.iso`

Stages: squashfs → prepare-tree → GRUB BIOS → GRUB UEFI → xorriso → SHA-256

---

### `nexus verify <iso>`

Verify a release ISO against its SHA-256 sidecar file.

```bash
nexus verify ./release/NexusOS-analyst-0.1.0-amd64.iso
```

Expects `<iso>.sha256` to exist alongside the ISO.
Exit code 0 = checksum matches. Exit code 1 = mismatch or file missing.

---

### `nexus manifest`

Generate a JSON build manifest for the release directory.

```bash
nexus manifest --profile analyst --mode lab --output ./release
```

Options:
- `-p, --profile <name>` — Profile name
- `-m, --mode <mode>` — Mode
- `-o, --output <dir>` — Release directory to scan

Output: `<output>/NexusOS-<profile>-<version>-<arch>.manifest.json`

The manifest includes: artifact list, SHA-256 hashes, sizes, installed packages,
build date, git commit.

---

### `nexus smoke-test <iso>`

Boot the ISO headlessly in QEMU and verify boot progress.

```bash
nexus smoke-test ./release/NexusOS-analyst-0.1.0-amd64.iso
```

Options:
- `-m, --mode <mode>` — Mode (informational)

Requires `qemu-system-x86_64` and (strongly recommended) `kvm` support.
If QEMU is not available, the test is skipped with a warning.

The test:
1. Checks ISO 9660 signature
2. Boots in QEMU (headless, serial output, timeout)
3. Inspects serial log for boot indicators and kernel errors
4. Exit code 0 = passed (or gracefully skipped)

---

### `nexus clean`

Remove the workspace (rootfs + staging tree). Does not remove release artifacts.

```bash
sudo nexus clean
sudo nexus clean --workspace /custom/workspace
```

---

### `nexus release`

Full release pipeline in one command.

```bash
sudo nexus release --profile analyst --mode hardened --output ./release
sudo nexus release --profile full --mode hardened --skip-smoke
```

Options:
- `-p, --profile <name>` — Profile name (**required**)
- `-m, --mode <mode>` — `hardened` or `lab` (default: `hardened`)
- `-o, --output <dir>` — Output directory
- `-j, --jobs <n>` — Parallel apt jobs
- `--skip-smoke` — Skip QEMU smoke test

Stages: build-rootfs → harden → build-iso → manifest → smoke-test

---

### `nexus profiles`

List available profiles.

```bash
nexus profiles
```

---

## Exit Codes

| Code | Meaning                              |
|------|--------------------------------------|
| 0    | Success                              |
| 1    | Error (message printed to stderr)    |
| 124  | Timeout (QEMU smoke test — may pass) |
