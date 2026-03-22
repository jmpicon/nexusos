#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include "nexus/common.hpp"

namespace nexus {

// ── Checksum — SHA-256 via OpenSSL EVP ───────────────────────────────────────
class Checksum {
public:
    Checksum() = delete;

    // Compute SHA-256 hex digest of a file
    [[nodiscard]] static Result<std::string, std::string>
        sha256_file(const std::filesystem::path& path);

    // Compute SHA-256 hex digest of a string
    [[nodiscard]] static Result<std::string, std::string>
        sha256_string(std::string_view data);

    // Write a .sha256 sidecar next to a file (file.iso → file.iso.sha256)
    [[nodiscard]] static VoidResult write_sha256_file(
        const std::filesystem::path& target_file);

    // Verify that a .sha256 sidecar matches the target file
    [[nodiscard]] static Result<bool, std::string> verify_sha256_file(
        const std::filesystem::path& target_file);

    // Generate checksums for all files in a directory, write CHECKSUMS.sha256
    [[nodiscard]] static VoidResult generate_checksums_dir(
        const std::filesystem::path& dir,
        const std::filesystem::path& out_file);
};

} // namespace nexus
