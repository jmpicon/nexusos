#pragma once
#include <filesystem>
#include <string>
#include "nexus/common.hpp"
#include "config/config_parser.hpp"

namespace nexus {

class BrandingManager {
public:
    explicit BrandingManager(const NexusConfig& cfg);

    // Apply all branding to the rootfs
    [[nodiscard]] VoidResult apply(
        const std::filesystem::path& rootfs,
        const std::string& profile);

private:
    const NexusConfig& cfg_;

    [[nodiscard]] VoidResult write_motd(
        const std::filesystem::path& rootfs,
        const std::string& profile);

    [[nodiscard]] VoidResult write_issue(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult write_os_release(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult install_wallpaper(const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult write_bash_profile(const std::filesystem::path& rootfs);
};

} // namespace nexus
