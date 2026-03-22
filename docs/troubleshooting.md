# NexusOS — Troubleshooting

## Build Failures

### `debootstrap: command not found`
```bash
sudo apt-get install debootstrap
# or run:
sudo bash scripts/bootstrap/install-deps.sh
```

### `debootstrap` fails with GPG error
The mirror's Release file signature may be stale or the keyring is missing:
```bash
sudo apt-get install --reinstall debian-archive-keyring
# Or use a different mirror:
sudo nexus build-rootfs --profile core --mirror http://ftp.debian.org/debian
```

### `mksquashfs: command not found`
```bash
sudo apt-get install squashfs-tools
```

### `xorriso: command not found`
```bash
sudo apt-get install xorriso
```

### `grub-mkstandalone: command not found`
```bash
sudo apt-get install grub-common grub-pc-bin grub-efi-amd64-bin
```

### `Permission denied` running build steps
Build steps that modify the chroot require root:
```bash
sudo nexus build-rootfs --profile analyst --mode lab
```

### `mount: permission denied` for /proc in chroot
This usually means the build is not running as root, or a previous build
left mounted filesystems. Unmount manually:
```bash
sudo umount -l /tmp/nexus-workspace/rootfs/dev/pts 2>/dev/null
sudo umount -l /tmp/nexus-workspace/rootfs/dev 2>/dev/null
sudo umount -l /tmp/nexus-workspace/rootfs/sys 2>/dev/null
sudo umount -l /tmp/nexus-workspace/rootfs/proc 2>/dev/null
```

### `apt-get install` fails inside chroot
Common causes:
1. `/proc` not mounted — re-run `build-rootfs` from scratch
2. DNS not working — check host DNS and `/etc/resolv.conf`
3. Mirror unreachable — test with `curl -I http://deb.debian.org/debian`
4. Disk full — need at least 10 GB free for a full profile

Check available disk space:
```bash
df -h /tmp
df -h /var/cache/nexus
```

### `update-initramfs: command not found` inside chroot
The kernel package was not installed. Make sure `linux-image-amd64` is in
the profile's package list. Check `packages/lists/base.list`.

---

## ISO Boot Issues

### ISO doesn't boot on BIOS systems
Verify the hybrid MBR was embedded:
```bash
file NexusOS-*.iso
# Should say: "x86 boot sector" or "ISO 9660"
```
If the GRUB BIOS image is missing, check that `grub-pc-bin` is installed
and that `/usr/lib/grub/i386-pc/boot_hybrid.img` exists.

### ISO doesn't boot on UEFI systems
Check that `BOOTX64.EFI` is in the ISO:
```bash
isoinfo -l -i NexusOS-*.iso | grep -i efi
```
Verify `grub-efi-amd64-bin` is installed and `x86_64-efi` modules are available:
```bash
ls /usr/lib/grub/x86_64-efi/
```

### GRUB boots but says "error: file not found"
The squashfs path or kernel/initrd paths don't match `grub.cfg`.
Check that `iso/live/filesystem.squashfs` and `iso/boot/vmlinuz` exist.

### Kernel panic: "VFS: Unable to mount root fs"
The initramfs doesn't include `live-boot` modules or the `squashfs` module.
Verify that `live-boot` and `live-boot-initramfs-tools` were installed in
the rootfs before running `update-initramfs`.

Check the initramfs modules:
```bash
lsinitramfs /tmp/nexus-workspace/rootfs/boot/initrd.img-* | grep squash
```

---

## QEMU Smoke Test

### `kvm: permission denied`
```bash
sudo usermod -aG kvm $USER
# log out and back in
# Or run the smoke test as root
```

### QEMU exits immediately (exit code != 0)
Could be: missing KVM, insufficient RAM, or a bad ISO.
Try with more memory:
```bash
NEXUS_QEMU_RAM=4096 nexus smoke-test NexusOS-*.iso
```
Or verify the ISO manually:
```bash
xorriso -indev NexusOS-*.iso -report_system_area plain
```

### Smoke test times out without boot indicator
Increase the timeout in `nexus.yaml`:
```yaml
qemu:
  timeout: 240
```

---

## CMake Build Issues

### `Could not find OpenSSL`
```bash
sudo apt-get install libssl-dev
```

### `Could not find yaml-cpp`
```bash
sudo apt-get install libyaml-cpp-dev
```

### FetchContent fails (no internet in build environment)
Pre-download the dependencies and point CMake to local tarballs:
```bash
cmake --preset debug \
    -DFETCHCONTENT_SOURCE_DIR_CLI11=/path/to/cli11 \
    -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=/path/to/json
```

### CMake version too old
```bash
cmake --version
# Need >= 3.25
# Install via snap:
sudo snap install cmake --classic
# Or download from cmake.org
```

---

## Common Errors

### `nexus: command not found`
The binary must be run from the build directory or installed:
```bash
# Development:
./build/debug/src/cli/nexus doctor
# Or install:
sudo cmake --install build/release
nexus doctor
```

### Profile not found
Ensure the profile YAML file exists:
```bash
ls profiles/
nexus profiles
```

### Workspace directory not writable
```bash
sudo mkdir -p /tmp/nexus-workspace
sudo chown $(whoami) /tmp/nexus-workspace
```
Or specify a different workspace:
```bash
sudo nexus build-rootfs --profile core --workspace /opt/nexus-ws
```
