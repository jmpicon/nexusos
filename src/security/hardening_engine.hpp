#pragma once

#include <filesystem>
#include "nexus/common.hpp"
#include "config/config_parser.hpp"

namespace nexus {

class HardeningEngine {
public:
    explicit HardeningEngine(const NexusConfig& cfg);

    // Apply sysctl hardening parameters inside chroot
    [[nodiscard]] VoidResult apply_sysctl(
        const std::filesystem::path& rootfs,
        BuildMode mode);

    // Enable and configure AppArmor inside chroot
    [[nodiscard]] VoidResult configure_apparmor(
        const std::filesystem::path& rootfs);

    // Configure auditd rules inside chroot
    [[nodiscard]] VoidResult configure_auditd(
        const std::filesystem::path& rootfs,
        BuildMode mode);

    // Install and configure nftables firewall rules
    [[nodiscard]] VoidResult configure_nftables(
        const std::filesystem::path& rootfs,
        BuildMode mode);

    // Harden systemd unit defaults
    [[nodiscard]] VoidResult harden_systemd(
        const std::filesystem::path& rootfs,
        BuildMode mode);

    // Set restrictive umask, login.defs, pam config
    [[nodiscard]] VoidResult configure_pam_login(
        const std::filesystem::path& rootfs,
        BuildMode mode);

    // Disable root login (SSH, console, getty)
    [[nodiscard]] VoidResult disable_root_login(
        const std::filesystem::path& rootfs);

    // Configure SSH daemon (if installed)
    [[nodiscard]] VoidResult configure_sshd(
        const std::filesystem::path& rootfs,
        BuildMode mode);

    // Full hardening pipeline for a given mode
    [[nodiscard]] VoidResult apply_all(
        const std::filesystem::path& rootfs,
        BuildMode mode);

private:
    const NexusConfig& cfg_;

    [[nodiscard]] VoidResult write_sysctl_hardened(
        const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult write_sysctl_lab(
        const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult write_nftables_hardened(
        const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult write_nftables_lab(
        const std::filesystem::path& rootfs);
    [[nodiscard]] VoidResult write_audit_rules(
        const std::filesystem::path& rootfs,
        BuildMode mode);
    [[nodiscard]] VoidResult write_systemd_defaults(
        const std::filesystem::path& rootfs,
        BuildMode mode);
};

} // namespace nexus
