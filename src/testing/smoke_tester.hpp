#pragma once
#include <filesystem>
#include "nexus/common.hpp"
#include "config/config_parser.hpp"

namespace nexus {

class SmokeTester {
public:
    explicit SmokeTester(const NexusConfig& cfg);

    // Boot the ISO in QEMU, verify it reaches a usable state
    [[nodiscard]] VoidResult run(
        const std::filesystem::path& iso_path,
        BuildMode mode);

    // Validate rootfs structure without booting
    [[nodiscard]] VoidResult validate_rootfs(
        const std::filesystem::path& rootfs);

private:
    const NexusConfig& cfg_;

    [[nodiscard]] VoidResult check_iso_integrity(
        const std::filesystem::path& iso_path) const;

    [[nodiscard]] VoidResult boot_qemu(
        const std::filesystem::path& iso_path,
        int timeout_secs) const;
};

} // namespace nexus
