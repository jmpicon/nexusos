#pragma once

#include <string>
#include <filesystem>
#include "nexus/common.hpp"
#include "config/config_parser.hpp"

namespace nexus {

class IsoBuilder {
public:
    explicit IsoBuilder(const NexusConfig& cfg);

    // Compress rootfs into squashfs
    [[nodiscard]] VoidResult build_squashfs(
        const std::filesystem::path& rootfs,
        const std::filesystem::path& squashfs_out);

    // Prepare the ISO staging tree (grub/, live/, boot/)
    [[nodiscard]] VoidResult prepare_iso_tree(
        const std::filesystem::path& squashfs_file,
        const std::filesystem::path& iso_tree,
        const std::filesystem::path& assets_dir);

    // Install GRUB BIOS boot support
    [[nodiscard]] VoidResult install_grub_bios(
        const std::filesystem::path& iso_tree,
        const std::filesystem::path& rootfs);

    // Install GRUB UEFI boot support (creates ESP image)
    [[nodiscard]] VoidResult install_grub_uefi(
        const std::filesystem::path& iso_tree,
        const std::filesystem::path& rootfs);

    // Generate final ISO using xorriso
    [[nodiscard]] VoidResult generate_iso(
        const std::filesystem::path& iso_tree,
        const std::filesystem::path& output_iso,
        const std::string&           volume_id);

private:
    const NexusConfig& cfg_;

    [[nodiscard]] VoidResult write_grub_cfg(
        const std::filesystem::path& grub_cfg_path,
        const std::string&           volume_id);

    [[nodiscard]] std::string find_kernel_version(
        const std::filesystem::path& rootfs) const;
};

} // namespace nexus
