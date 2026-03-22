#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <optional>
#include "nexus/common.hpp"

namespace nexus {

// ── Profile — loaded from profiles/<name>.yaml ───────────────────────────────
struct Profile {
    std::string              name        {};
    std::string              description {};
    std::string              version     {"0.1.0"};
    std::vector<std::string> extends     {};  // inherits from other profiles

    // Package groups
    std::vector<std::string> packages    {};  // core package list
    std::vector<std::string> packages_hardened {};  // extras for hardened mode
    std::vector<std::string> packages_lab       {};  // extras for lab mode
    std::vector<std::string> packages_exclude   {};  // explicitly removed

    // Overlays to apply (ordered)
    std::vector<std::string> overlays    {};  // names of overlays/ subdirectories

    // Supported modes for this profile
    bool support_hardened {true};
    bool support_lab      {true};

    // Profile-specific config
    std::map<std::string, std::string> config {};

    // Desktop environment (can be empty = no DE)
    std::string desktop {"xfce4"};

    // Hooks (optional shell scripts in scripts/chroot/)
    std::string hook_pre_packages  {};
    std::string hook_post_packages {};
    std::string hook_post_harden   {};

    [[nodiscard]] bool valid() const noexcept { return !name.empty(); }
};

// ── ProfileManager ────────────────────────────────────────────────────────────
class ProfileManager {
public:
    explicit ProfileManager(const std::filesystem::path& profiles_dir);

    // List available profiles
    [[nodiscard]] std::vector<std::string> list() const;

    // Load a named profile (resolves inheritance)
    [[nodiscard]] Result<Profile, std::string> load(const std::string& name) const;

    // Load and fully resolve a profile with mode-specific packages merged in
    [[nodiscard]] Result<Profile, std::string> resolve(
        const std::string& name,
        BuildMode mode) const;

    // Get merged package list for a profile + mode (de-duplicated)
    [[nodiscard]] Result<std::vector<std::string>, std::string> get_packages(
        const std::string& name,
        BuildMode mode) const;

    // Get overlay list for a profile
    [[nodiscard]] Result<std::vector<std::string>, std::string> get_overlays(
        const std::string& name,
        BuildMode mode) const;

    // Validate all profiles in the directory
    [[nodiscard]] std::vector<std::string> validate_all() const;

    // Print profile info
    static void print(const Profile& p);

private:
    std::filesystem::path profiles_dir_;

    [[nodiscard]] Result<Profile, std::string> load_yaml(
        const std::filesystem::path& path) const;

    // Recursively resolve inheritance, returns merged profile
    [[nodiscard]] Result<Profile, std::string> merge_inheritance(
        const Profile& p,
        std::vector<std::string>& visited) const;

    static void merge_into(Profile& base, const Profile& extension);
};

} // namespace nexus
