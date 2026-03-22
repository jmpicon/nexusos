#pragma once

#include <string>
#include <filesystem>
#include <map>
#include <optional>
#include "nexus/common.hpp"

namespace nexus {

// ── NexusConfig — global build configuration ─────────────────────────────────
// Loaded from nexus.yaml in the project root or CWD.
struct NexusConfig {
    // Identity
    std::string distro_name       {"NexusOS"};
    std::string distro_codename   {"Phantom"};
    std::string version           {"0.1.0"};

    // Build environment
    std::filesystem::path workspace_dir   {"/tmp/nexus-workspace"};
    std::filesystem::path output_dir      {"./release"};
    std::filesystem::path cache_dir       {"/var/cache/nexus"};
    std::filesystem::path profiles_dir    {"profiles"};
    std::filesystem::path packages_dir    {"packages"};
    std::filesystem::path overlays_dir    {"overlays"};
    std::filesystem::path scripts_dir     {"scripts"};
    std::filesystem::path assets_dir      {"assets"};

    // Debian base
    std::string debian_mirror   {"http://deb.debian.org/debian"};
    std::string debian_suite    {"bookworm"};
    std::string arch            {"amd64"};

    // Build settings
    int    jobs              {4};
    bool   use_cache         {true};
    bool   gpg_verify        {true};
    bool   include_firmware  {true};

    // QEMU smoke test
    std::string qemu_binary   {"qemu-system-x86_64"};
    int         qemu_ram_mb   {2048};
    int         qemu_timeout  {120};

    // Extra apt keys/repos
    std::vector<std::string> extra_repos;
    std::vector<std::string> extra_apt_keys;

    // Custom sysctl overrides (merged over overlay)
    std::map<std::string, std::string> sysctl_overrides;

    [[nodiscard]] bool valid() const noexcept {
        return !distro_name.empty() && !debian_suite.empty() && !arch.empty();
    }
};

// ── ConfigParser ─────────────────────────────────────────────────────────────
class ConfigParser {
public:
    ConfigParser() = delete;

    // Load from nexus.yaml (searches: provided path, CWD, project root)
    [[nodiscard]] static Result<NexusConfig, std::string> load(
        const std::filesystem::path& path = "nexus.yaml");

    // Write a default nexus.yaml to the given path
    [[nodiscard]] static VoidResult write_default(
        const std::filesystem::path& path = "nexus.yaml");

    // Validate a loaded config and return any issues
    [[nodiscard]] static std::vector<std::string> validate(
        const NexusConfig& cfg);

    // Merge environment variables into config (NEXUS_MIRROR, NEXUS_SUITE, …)
    static void apply_env_overrides(NexusConfig& cfg);
};

} // namespace nexus
