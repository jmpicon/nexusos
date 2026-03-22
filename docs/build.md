# NexusOS — Build Guide

## Prerequisites

### Host OS

NexusOS must be built on **Debian 12 Bookworm** (or Ubuntu 22.04+), `amd64`.
Other distributions may work but are not tested.

### Root access

`build-rootfs` and `harden` require root (they run `debootstrap` and
`chroot`). `doctor`, `verify`, `manifest`, and `smoke-test` do not.

### Install dependencies

```bash
sudo bash scripts/bootstrap/install-deps.sh
```

This installs: `cmake`, `ninja-build`, `debootstrap`, `squashfs-tools`,
`xorriso`, `grub-pc-bin`, `grub-efi-amd64-bin`, `dosfstools`, `rsync`,
`libssl-dev`, `libyaml-cpp-dev`, `live-build`, and optional `qemu-system-x86`.

### Validate environment

```bash
bash scripts/build/check-env.sh
# or:
sudo ./build/debug/src/cli/nexus doctor
```

---

## Building the nexus CLI

```bash
# Debug build (with tests, ASan, UBSan)
cmake --preset debug
cmake --build --preset debug

# Release build
cmake --preset release
cmake --build --preset release

# Run tests
ctest --preset all
```

The binary is at `build/debug/src/cli/nexus` or `build/release/src/cli/nexus`.

### Make shortcuts

```bash
make deps     # install-deps.sh
make doctor   # check-env.sh
make debug    # cmake debug build
make release  # cmake release build
make test     # ctest all
make clean    # remove build/
```

---

## Building an ISO

### Full pipeline (recommended)

```bash
sudo ./build/release/src/cli/nexus release \
    --profile analyst \
    --mode lab \
    --output ./release \
    --jobs 4
```

### Step by step

```bash
# 1. Bootstrap base system (~10-20 min, downloads Debian)
sudo nexus build-rootfs --profile analyst --mode lab

# 2. Apply hardening (fast, ~1 min)
sudo nexus harden --mode lab --profile analyst

# 3. Build ISO (squashfs compression ~5-15 min)
sudo nexus build-iso --profile analyst --output ./release

# 4. Verify
nexus verify ./release/NexusOS-analyst-0.1.0-amd64.iso

# 5. Generate manifest
nexus manifest --profile analyst --mode lab

# 6. Smoke test (optional, requires qemu-system-x86_64)
nexus smoke-test ./release/NexusOS-analyst-0.1.0-amd64.iso
```

---

## Build inside a container

```bash
# Build the builder image
docker build -f ci/Dockerfile.build -t nexus-builder .

# Run the full release pipeline inside the container
docker run \
    --privileged \
    --rm \
    -v "$(pwd)":/workspace \
    nexus-builder \
    bash -c "cmake --preset release && cmake --build --preset release && \
             ./build/release/src/cli/nexus release --profile analyst --mode lab"
```

`--privileged` is required for `debootstrap`, `chroot`, and `mount`.

---

## Configuration

Edit `nexus.yaml` to customise:

```yaml
base:
  mirror: "http://ftp.es.debian.org/debian"  # faster local mirror
  suite:  "bookworm"
  arch:   "amd64"

build:
  jobs: 8      # parallel jobs for apt

qemu:
  ram_mb:  4096
  timeout: 180
```

Environment variables override config file:
```bash
export NEXUS_MIRROR="http://ftp.es.debian.org/debian"
export NEXUS_JOBS=8
```

---

## Workspace layout

After `build-rootfs`, the workspace (`/tmp/nexus-workspace` by default) contains:

```
/tmp/nexus-workspace/
├── rootfs/           ← chroot (debootstrap + packages + overlays)
├── squashfs/         ← filesystem.squashfs (built during build-iso)
└── iso/              ← ISO staging tree (boot/, live/, EFI/)
```

The final ISO goes to `./release/NexusOS-<profile>-<version>-amd64.iso`.

---

## Adding packages to a profile

1. Add package names to `profiles/<name>.yaml` under `packages:`
2. Or create/edit a `.list` file in `packages/lists/`
3. Rebuild: `sudo nexus build-rootfs --profile <name>`

For packages not in Debian main, add the repo to `nexus.yaml`:
```yaml
extra_repos:
  - "deb [signed-by=/etc/apt/keyrings/zeek.gpg] https://download.zeek.org/apt bookworm main"
```

---

## Reproducibility

The build is **deterministic within a Debian snapshot**. For fully reproducible
builds, pin the Debian snapshot:

```yaml
base:
  mirror: "http://snapshot.debian.org/archive/debian/20260101T000000Z/"
```

Note: snapshot mirrors are rate-limited; use a local caching proxy for speed.
