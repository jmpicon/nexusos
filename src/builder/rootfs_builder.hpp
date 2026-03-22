#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "nexus/common.hpp"
#include "config/config_parser.hpp"
#include "packaging/package_manager.hpp"

namespace nexus {

class RootfsBuilder {
public:
    explicit RootfsBuilder(const NexusConfig& cfg, PackageManager& pkg_mgr);

    // Run debootstrap to create a minimal Debian rootfs
    [[nodiscard]] VoidResult debootstrap(
        const std::filesystem::path& rootfs,
        const std::string&           mirror,
        const std::string&           suite,
        const std::string&           arch);

    // Configure apt sources, locale, timezone inside chroot
    [[nodiscard]] VoidResult configure_base(
        const std::filesystem::path& rootfs,
        const std::string&           mirror,
        const std::string&           suite);

    // Apply overlays (rsync from overlays/<name>/ into rootfs)
    [[nodiscard]] VoidResult apply_overlays(
        const std::filesystem::path&      rootfs,
        const std::vector<std::string>&   overlays,
        const std::filesystem::path&      overlays_dir);

    // Create default non-root user 'nexus'
    [[nodiscard]] VoidResult setup_user(const std::filesystem::path& rootfs);

    // Generate initramfs inside chroot
    [[nodiscard]] VoidResult generate_initramfs(const std::filesystem::path& rootfs);

    // Disable unnecessary services inside chroot
    [[nodiscard]] VoidResult disable_services(const std::filesystem::path& rootfs);

    // Run an arbitrary hook script inside chroot
    [[nodiscard]] VoidResult run_hook(
        const std::filesystem::path& rootfs,
        const std::filesystem::path& script_path);

    // Copy kernel from rootfs to iso tree
    [[nodiscard]] VoidResult export_kernel(
        const std::filesystem::path& rootfs,
        const std::filesystem::path& dest_dir);

private:
    const NexusConfig& cfg_;
    PackageManager&    pkg_mgr_;

    [[nodiscard]] VoidResult write_apt_sources(
        const std::filesystem::path& rootfs,
        const std::string&           mirror,
        const std::string&           suite);

    [[nodiscard]] VoidResult configure_locale(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult configure_timezone(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult configure_hostname(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult configure_fstab(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult configure_network(const std::filesystem::path& rootfs);
};

} // namespace nexus
