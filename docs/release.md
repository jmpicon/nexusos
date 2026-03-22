# NexusOS — Release Process

## Release Artifacts

A complete release consists of:

```
release/
├── NexusOS-<profile>-<version>-<arch>.iso         # Bootable ISO
├── NexusOS-<profile>-<version>-<arch>.iso.sha256  # SHA-256 sidecar
├── NexusOS-<profile>-<version>-<arch>.manifest.json  # Build manifest
└── CHECKSUMS.sha256                               # All checksums
```

## Full Release Command

```bash
# Single command, requires root
sudo nexus release \
    --profile full \
    --mode hardened \
    --output ./release \
    --jobs 4

# Skip QEMU smoke test (e.g. in CI without KVM)
sudo nexus release --profile analyst --mode hardened --skip-smoke
```

## Manual Release Steps

```bash
# 1. Build
sudo nexus build-rootfs --profile analyst --mode hardened
sudo nexus harden --mode hardened --profile analyst
sudo nexus build-iso --profile analyst --output ./release

# 2. Verify
nexus verify ./release/NexusOS-analyst-0.1.0-amd64.iso

# 3. Generate manifest
nexus manifest --profile analyst --mode hardened --output ./release

# 4. All checksums
bash scripts/release/generate-checksums.sh ./release

# 5. Smoke test
nexus smoke-test ./release/NexusOS-analyst-0.1.0-amd64.iso
```

## Verifying a Release

```bash
# Verify ISO integrity
nexus verify ./release/NexusOS-analyst-0.1.0-amd64.iso

# Manual SHA-256 check
sha256sum -c NexusOS-analyst-0.1.0-amd64.iso.sha256

# Check all checksums
sha256sum -c CHECKSUMS.sha256
```

## Release Naming Convention

```
NexusOS-{profile}-{version}-{arch}.iso
NexusOS-analyst-0.1.0-amd64.iso
NexusOS-full-0.1.0-amd64.iso
NexusOS-forensic-0.1.0-hardened-amd64.iso
```

## Tagging a Release

```bash
git tag -a v0.1.0 -m "NexusOS v0.1.0 Phantom"
git push origin v0.1.0
```

The CI will build all profiles and attach artifacts to the GitHub release.
