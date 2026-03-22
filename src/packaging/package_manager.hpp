#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include "nexus/common.hpp"
#include "config/config_parser.hpp"

namespace nexus {

// ── PackageList — resolved, de-duplicated list ────────────────────────────────
struct PackageList {
    std::vector<std::string> packages;
    std::vector<std::string> excluded;
    std::size_t estimated_mb {0};
};

class PackageManager {
public:
    explicit PackageManager(const NexusConfig& cfg);

    // Load a .list file from packages/lists/
    [[nodiscard]] Result<PackageList, std::string> load_list(
        const std::string& name) const;

    // Merge multiple package lists (de-duplicate, apply exclusions)
    [[nodiscard]] static PackageList merge(
        const std::vector<PackageList>& lists);

    // Install packages inside a chroot
    [[nodiscard]] VoidResult install(
        const std::filesystem::path&    rootfs,
        const std::vector<std::string>& packages);

    // Install packages from a specific .list file inside a chroot
    [[nodiscard]] VoidResult install_list(
        const std::filesystem::path& rootfs,
        const std::string& list_name);

    // Query installed packages in a rootfs
    [[nodiscard]] Result<std::vector<std::string>, std::string> query_installed(
        const std::filesystem::path& rootfs) const;

    // Verify that a set of packages is installed
    [[nodiscard]] std::vector<std::string> check_installed(
        const std::filesystem::path&    rootfs,
        const std::vector<std::string>& expected) const;

    // Clean apt cache inside chroot
    [[nodiscard]] VoidResult clean_cache(const std::filesystem::path& rootfs);

private:
    const NexusConfig& cfg_;

    [[nodiscard]] VoidResult apt_install(
        const std::filesystem::path&    rootfs,
        const std::vector<std::string>& packages);

    std::vector<std::string> list_files_;  // cache
};

} // namespace nexus
