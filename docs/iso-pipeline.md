# NexusOS — ISO Build Pipeline

## Overview

The ISO pipeline converts a chroot rootfs into a bootable hybrid ISO 9660 image
that boots on BIOS and UEFI systems, using only standard Debian/Linux tools:

```
rootfs/ → filesystem.squashfs → iso-tree/ → NexusOS-<profile>-<ver>-amd64.iso
```

No `live-build` orchestration is used at the outer level; `nexus` calls the tools
directly. `live-boot` and `live-config` are installed *inside* the rootfs and
handle mounting the squashfs at boot time.

---

## Stage 1: SquashFS Compression

**Tool:** `mksquashfs(1)` from `squashfs-tools`

```bash
mksquashfs <rootfs> <workspace>/squashfs/filesystem.squashfs \
    -comp xz -Xdict-size 1M -b 1048576 -noappend \
    -e proc -e sys -e dev -e run -e tmp \
    -e "var/cache/apt/archives/*.deb" \
    -e "var/lib/apt/lists/*"
```

- Compression: `xz` with 1 MB dictionary (good ratio, acceptable speed)
- Block size: 1 MB (balanced for random access performance)
- Excluded: virtual filesystems, apt cache, apt lists (saves 200-400 MB)

Typical output: 1-3 GB squashfs from 3-7 GB uncompressed rootfs.

---

## Stage 2: ISO Tree Preparation

The ISO staging tree layout:

```
<workspace>/iso/
├── boot/
│   ├── vmlinuz            → symlink to /boot/vmlinuz-<kver> from rootfs
│   ├── initrd.img         → symlink to /boot/initrd.img-<kver> from rootfs
│   └── grub/
│       ├── grub.cfg       → GRUB configuration
│       ├── font.pf2       → GRUB font (from /usr/share/grub/unicode.pf2)
│       ├── efi.img        → FAT32 ESP image (UEFI)
│       └── i386-pc/
│           └── eltorito.img → GRUB BIOS El Torito image
├── live/
│   └── filesystem.squashfs
└── EFI/
    └── BOOT/
        └── BOOTX64.EFI   → GRUB UEFI standalone binary
```

The kernel and initrd are copied from the rootfs `/boot/` directory.
`live-boot` will look for `/live/filesystem.squashfs` on the ISO.

---

## Stage 3: GRUB BIOS

**Tool:** `grub-mkstandalone(1)` from `grub-common`

```bash
grub-mkstandalone \
    --format=i386-pc \
    --output=iso/boot/grub/i386-pc/eltorito.img \
    --install-modules="linux normal iso9660 biosdisk memdisk search tar ls" \
    --modules="linux normal iso9660 biosdisk memdisk search configfile \
               part_gpt part_msdos fat ext2" \
    --locales="" --fonts="" \
    "boot/grub/grub.cfg=iso/boot/grub/grub.cfg"
```

This creates a self-contained GRUB image (`eltorito.img`) that embeds
the grub.cfg. The image is used as the El Torito boot catalog entry.

A hybrid MBR is also embedded via `--grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img`
so the ISO can also boot from USB drives (MBR mode).

---

## Stage 4: GRUB UEFI

**Tool:** `grub-mkstandalone(1)` + `mkdosfs(1)`

```bash
# Create GRUB UEFI binary
grub-mkstandalone \
    --format=x86_64-efi \
    --output=iso/EFI/BOOT/BOOTX64.EFI \
    --install-modules="linux normal iso9660 fat search efi_gop efi_uga" \
    ...

# Create FAT32 ESP image (2 MB)
mkdosfs -F 32 -n NEXUS_EFI -C iso/boot/grub/efi.img 4096

# Mount and copy BOOTX64.EFI into the ESP image
mount -o loop iso/boot/grub/efi.img /mnt/efi
cp iso/EFI/BOOT/BOOTX64.EFI /mnt/efi/EFI/BOOT/
umount /mnt/efi
```

The ESP image (`efi.img`) is referenced by xorriso as the UEFI El Torito
alternate boot record.

---

## Stage 5: ISO Generation

**Tool:** `xorriso(1)` invoked as `xorriso -as mkisofs`

```bash
xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -joliet -joliet-long -rational-rock \
    -volid "NexusOS" \
    -partition_offset 16 \
    # BIOS El Torito:
    -eltorito-boot boot/grub/i386-pc/eltorito.img \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --grub2-boot-info \
    --grub2-mbr /usr/lib/grub/i386-pc/boot_hybrid.img \
    # UEFI El Torito:
    -eltorito-alt-boot \
    -e boot/grub/efi.img \
    -no-emul-boot --efi-boot-part --efi-boot-image \
    -output NexusOS-analyst-0.1.0-amd64.iso \
    iso/
```

This produces a **hybrid ISO** that boots as:
- CD/DVD (ISO 9660 + El Torito BIOS)
- USB flash drive (MBR partition + BIOS or UEFI)
- UEFI optical / USB (El Torito UEFI + FAT32 ESP)

---

## Stage 6: Checksums and Manifest

```bash
# SHA-256 sidecar
sha256sum NexusOS-analyst-0.1.0-amd64.iso > NexusOS-analyst-0.1.0-amd64.iso.sha256

# JSON manifest
nexus manifest --profile analyst --mode lab --output ./release
```

The manifest includes:
- Distro name, version, codename, profile, mode
- Build date (ISO 8601 UTC)
- Git commit hash
- List of artifacts with SHA-256 and sizes
- Installed package list (from `dpkg-query`)

---

## Live Boot Mechanism

At boot, `live-boot` (installed inside the rootfs) does:

1. Locate the squashfs on the boot device (USB, CD, ISO)
2. Mount squashfs read-only
3. Create tmpfs overlay
4. `overlayfs` mount: `squashfs (lower)` + `tmpfs (upper)` → `/`
5. `live-config` runs, sets hostname, creates users, configures networking

The kernel command line controls the behaviour:
```
boot=live               # activate live-boot
rd.live.image           # locate the squashfs
quiet splash            # suppress boot messages
apparmor=1 security=apparmor  # enable AppArmor
nexus_mode=forensic     # custom NexusOS variable (parsed by live-config hook)
toram                   # copy squashfs to RAM (optional)
noswap                  # disable swap (forensic mode)
ro                      # read-only root overlay (forensic)
```

---

## Forensic Mode

When `nexus_mode=forensic` is passed on the kernel command line, a
`live-config` hook disables automounting of attached disks and sets the
overlay to read-only. This prevents accidental writes to evidence drives.

The hook is installed at `/etc/live/config.d/nexus-forensic.sh` by the
forensic overlay.
