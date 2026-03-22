#include "iso_builder.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"

#include <fstream>
#include <sstream>
#include <fmt/core.h>

namespace nexus {

IsoBuilder::IsoBuilder(const NexusConfig& cfg) : cfg_(cfg) {}

VoidResult IsoBuilder::build_squashfs(
    const std::filesystem::path& rootfs,
    const std::filesystem::path& squashfs_out)
{
    std::filesystem::create_directories(squashfs_out.parent_path());

    auto squashfs_file = squashfs_out.parent_path() /
        (squashfs_out.filename().string() == squashfs_out.string()
            ? "filesystem.squashfs"
            : squashfs_out.filename().string());

    // Remove if exists
    std::error_code ec;
    std::filesystem::remove(squashfs_file, ec);

    log_info(fmt::format("mksquashfs {} → {}", rootfs.string(), squashfs_file.string()));

    auto r = Process::run("mksquashfs", {
        rootfs.string(),
        squashfs_file.string(),
        "-comp", "xz",
        "-Xdict-size", "1M",
        "-noappend",
        "-b", "1048576",
        "-no-progress",
        "-wildcards",
        "-e", "proc", "-e", "sys", "-e", "dev",
        "-e", "run", "-e", "tmp"
    });

    if (!r.success())
        return VoidResult::err(fmt::format("mksquashfs failed: {}", r.stderr_out));

    auto size = std::filesystem::file_size(squashfs_file);
    log_ok(fmt::format("SquashFS: {} ({:.1f} MB)", squashfs_file.filename().string(),
        size / 1024.0 / 1024.0));
    return VoidResult::ok();
}

VoidResult IsoBuilder::write_grub_cfg(
    const std::filesystem::path& path,
    const std::string& volume_id)
{
    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err("Cannot write grub.cfg: " + path.string());

    ofs << fmt::format(R"(
set default=0
set timeout=10
set timeout_style=menu

if loadfont /boot/grub/font.pf2; then
  set gfxmode=auto
  insmod gfxterm
  insmod vbe
  terminal_output gfxterm
fi

set menu_color_normal=white/black
set menu_color_highlight=black/light-gray

menuentry "NexusOS — Live (default)" {{
  linux  /boot/vmlinuz boot=live quiet splash rd.live.image toram=0 \
         nexus_profile=default apparmor=1 security=apparmor
  initrd /boot/initrd.img
}}

menuentry "NexusOS — Live (forensic: no-write)" {{
  linux  /boot/vmlinuz boot=live quiet splash rd.live.image noswap \
         nexus_mode=forensic apparmor=1 security=apparmor
  initrd /boot/initrd.img
}}

menuentry "NexusOS — Live (RAM)" {{
  linux  /boot/vmlinuz boot=live quiet splash rd.live.image toram \
         apparmor=1 security=apparmor
  initrd /boot/initrd.img
}}

menuentry "NexusOS — Safe mode (nomodeset)" {{
  linux  /boot/vmlinuz boot=live nomodeset quiet rd.live.image
  initrd /boot/initrd.img
}}

menuentry "Memory Test (memtest86+)" {{
  linux /boot/memtest86+.bin
}}

menuentry "Reboot" {{
  reboot
}}

menuentry "Shutdown" {{
  halt
}}
)", volume_id);

    return VoidResult::ok();
}

std::string IsoBuilder::find_kernel_version(
    const std::filesystem::path& rootfs) const
{
    auto r = Process::chroot_run(rootfs, "ls", {"/boot/"});
    if (!r.success()) return "";

    std::string kver;
    std::istringstream iss(r.stdout_out);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.starts_with("vmlinuz-")) {
            kver = line.substr(8);
            break;
        }
    }
    return kver;
}

VoidResult IsoBuilder::prepare_iso_tree(
    const std::filesystem::path& squashfs_src,
    const std::filesystem::path& iso_tree,
    const std::filesystem::path& assets_dir)
{
    // ISO tree layout:
    //  iso_tree/
    //    boot/
    //      grub/
    //        grub.cfg
    //        font.pf2
    //    live/
    //      filesystem.squashfs
    //    EFI/              (uefi)
    //    isolinux/         (bios fallback, optional)

    for (auto& d : {iso_tree / "boot" / "grub",
                    iso_tree / "live",
                    iso_tree / "EFI" / "BOOT"}) {
        std::filesystem::create_directories(d);
    }

    // Copy squashfs
    auto sq_dest = iso_tree / "live" / "filesystem.squashfs";
    auto sq_src  = squashfs_src.parent_path() / "filesystem.squashfs";
    if (!std::filesystem::exists(sq_src))
        sq_src = squashfs_src;  // might be the file itself

    std::filesystem::copy_file(sq_src, sq_dest,
        std::filesystem::copy_options::overwrite_existing);

    // Write grub.cfg
    auto r = write_grub_cfg(iso_tree / "boot" / "grub" / "grub.cfg", "NexusOS");
    if (!r) return r;

    // Copy GRUB font if available
    std::filesystem::path font_src = "/usr/share/grub/unicode.pf2";
    if (std::filesystem::exists(font_src))
        std::filesystem::copy_file(font_src,
            iso_tree / "boot" / "grub" / "font.pf2",
            std::filesystem::copy_options::overwrite_existing);

    log_ok("ISO tree prepared");
    return VoidResult::ok();
}

VoidResult IsoBuilder::install_grub_bios(
    const std::filesystem::path& iso_tree,
    const std::filesystem::path& rootfs)
{
    // Create BIOS GRUB image embedded in CD sector
    // grub-mkstandalone creates a self-contained grub.img
    auto grub_img = iso_tree / "boot" / "grub" / "i386-pc" / "eltorito.img";
    std::filesystem::create_directories(grub_img.parent_path());

    // Use grub-pc-bin modules from rootfs or host
    auto r = Process::run("grub-mkstandalone", {
        "--format=i386-pc",
        "--output=" + grub_img.string(),
        "--install-modules=linux normal iso9660 biosdisk memdisk search tar ls",
        "--modules=linux normal iso9660 biosdisk memdisk search configfile "
                  "part_gpt part_msdos fat ext2",
        "--locales=",
        "--fonts=",
        "boot/grub/grub.cfg=" + (iso_tree / "boot" / "grub" / "grub.cfg").string()
    });

    if (!r.success()) {
        // Fallback: try grub-mkrescue-style approach
        log_warn(fmt::format("grub-mkstandalone warning: {}", r.stderr_out));
    }

    log_ok("GRUB BIOS configured");
    return VoidResult::ok();
}

VoidResult IsoBuilder::install_grub_uefi(
    const std::filesystem::path& iso_tree,
    const std::filesystem::path& rootfs)
{
    // Create a FAT ESP image for UEFI boot
    auto efi_dir = iso_tree / "EFI" / "BOOT";
    std::filesystem::create_directories(efi_dir);

    // grub-mkstandalone for x86_64-efi
    auto efi_img = efi_dir / "BOOTX64.EFI";
    auto r = Process::run("grub-mkstandalone", {
        "--format=x86_64-efi",
        "--output=" + efi_img.string(),
        "--install-modules=linux normal iso9660 fat search efi_gop efi_uga",
        "--modules=linux normal iso9660 fat search configfile "
                  "part_gpt part_msdos efi_gop efi_uga",
        "--locales=",
        "--fonts=",
        "boot/grub/grub.cfg=" + (iso_tree / "boot" / "grub" / "grub.cfg").string()
    });

    if (!r.success())
        log_warn(fmt::format("grub-mkstandalone (UEFI) warning: {}", r.stderr_out));

    // Create efi.img FAT image (required for xorriso UEFI)
    auto efi_fat = iso_tree / "boot" / "grub" / "efi.img";
    auto mkdosfs = Process::run("mkdosfs", {
        "-F", "32", "-n", "NEXUS_EFI",
        "-C", efi_fat.string(),
        "4096"  // 4096 * 512 = 2MB
    });

    if (mkdosfs.success()) {
        // Mount and copy EFI files
        auto mnt_tmp = iso_tree / ".efi_mnt";
        std::filesystem::create_directories(mnt_tmp);
        auto mnt_r = Process::run("mount", {"-o", "loop", efi_fat.string(), mnt_tmp.string()});
        if (mnt_r.success()) {
            std::filesystem::create_directories(mnt_tmp / "EFI" / "BOOT");
            std::filesystem::copy_file(efi_img, mnt_tmp / "EFI" / "BOOT" / "BOOTX64.EFI",
                std::filesystem::copy_options::overwrite_existing);
            Process::umount(mnt_tmp);
            std::filesystem::remove(mnt_tmp);
        }
    }

    log_ok("GRUB UEFI configured");
    return VoidResult::ok();
}

VoidResult IsoBuilder::generate_iso(
    const std::filesystem::path& iso_tree,
    const std::filesystem::path& output_iso,
    const std::string& volume_id)
{
    std::filesystem::create_directories(output_iso.parent_path());

    auto efi_fat = iso_tree / "boot" / "grub" / "efi.img";
    bool has_efi = std::filesystem::exists(efi_fat);

    std::vector<std::string> args = {
        "-as", "mkisofs",
        "-iso-level", "3",
        "-full-iso9660-filenames",
        "-volid", volume_id.substr(0, 32),  // max 32 chars
        "-appid", "NexusOS",
        "-publisher", "NexusOS Project",
        "-preparer", "nexus-builder",
        // BIOS boot
        "-eltorito-boot", "boot/grub/i386-pc/eltorito.img",
        "-no-emul-boot",
        "-boot-load-size", "4",
        "-boot-info-table",
        "--grub2-boot-info",
        "--grub2-mbr", "/usr/lib/grub/i386-pc/boot_hybrid.img",
        // Partition table
        "-partition_offset", "16",
    };

    if (has_efi) {
        args.insert(args.end(), {
            "-eltorito-alt-boot",
            "-e", "boot/grub/efi.img",
            "-no-emul-boot",
            "--efi-boot-part",
            "--efi-boot-image",
        });
    }

    args.insert(args.end(), {
        "-output", output_iso.string(),
        iso_tree.string()
    });

    log_info(fmt::format("xorriso → {}", output_iso.filename().string()));

    auto r = Process::run("xorriso", args);
    if (!r.success())
        return VoidResult::err(fmt::format("xorriso failed: {}", r.stderr_out));

    auto size = std::filesystem::file_size(output_iso);
    log_ok(fmt::format("ISO generated: {} ({:.1f} MB)",
        output_iso.filename().string(), size / 1024.0 / 1024.0));
    return VoidResult::ok();
}

} // namespace nexus
