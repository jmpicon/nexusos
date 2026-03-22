#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include "nexus/common.hpp"

namespace nexus {

// ── ManifestEntry — one artifact in the release ───────────────────────────────
struct ManifestEntry {
    std::string filename;
    std::string sha256;
    std::uintmax_t size_bytes {0};
    std::string type;  // "iso", "squashfs", "manifest", "checksum"
};

// ── BuildManifest — full build manifest ──────────────────────────────────────
struct BuildManifest {
    std::string     distro_name     {"NexusOS"};
    std::string     distro_codename {"Phantom"};
    std::string     version         {"0.1.0"};
    std::string     profile         {};
    std::string     mode            {};
    std::string     arch            {"amd64"};
    std::string     base_distro    {"Debian 12 Bookworm"};
    std::string     build_date      {};
    std::string     builder_version {};
    std::string     git_commit      {};
    std::string     hostname        {};

    std::vector<ManifestEntry>            artifacts   {};
    std::vector<std::string>              packages    {};  // installed packages
    std::map<std::string, std::string>    build_env   {};  // key env vars
};

// ── Manifest — serialize / deserialize JSON ────────────────────────────────────
class Manifest {
public:
    Manifest() = delete;

    // Build a manifest by scanning a release directory
    [[nodiscard]] static Result<BuildManifest, std::string> build(
        const std::filesystem::path& release_dir,
        const std::string&           profile,
        const std::string&           mode,
        const std::string&           version = "0.1.0");

    // Write manifest JSON to file
    [[nodiscard]] static VoidResult write_json(
        const BuildManifest&         manifest,
        const std::filesystem::path& out_path);

    // Load manifest JSON from file
    [[nodiscard]] static Result<BuildManifest, std::string> read_json(
        const std::filesystem::path& path);

    // Populate installed package list from a rootfs chroot
    [[nodiscard]] static Result<std::vector<std::string>, std::string>
        packages_from_rootfs(const std::filesystem::path& rootfs);

    // Pretty-print manifest to stdout
    static void print(const BuildManifest& m);
};

} // namespace nexus
