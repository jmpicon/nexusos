#include "checksum.hpp"
#include "logger.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <fmt/core.h>

namespace nexus {

namespace {

std::string hex_encode(const unsigned char* buf, unsigned int len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < len; ++i)
        oss << std::setw(2) << static_cast<int>(buf[i]);
    return oss.str();
}

Result<std::string, std::string> evp_sha256(
    const std::function<bool(EVP_MD_CTX*)>& feed)
{
    auto* ctx = EVP_MD_CTX_new();
    if (!ctx) return Result<std::string,std::string>::err("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return Result<std::string,std::string>::err("EVP_DigestInit failed");
    }

    if (!feed(ctx)) {
        EVP_MD_CTX_free(ctx);
        return Result<std::string,std::string>::err("Feed function failed");
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int  hash_len = 0;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return Result<std::string,std::string>::err("EVP_DigestFinal failed");
    }

    EVP_MD_CTX_free(ctx);
    return Result<std::string,std::string>::ok(hex_encode(hash, hash_len));
}

} // anonymous namespace

Result<std::string, std::string> Checksum::sha256_file(
    const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return Result<std::string,std::string>::err(
        fmt::format("Cannot open: {}", path.string()));

    return evp_sha256([&](EVP_MD_CTX* ctx) -> bool {
        std::array<char, 65536> buf;
        while (ifs.good()) {
            ifs.read(buf.data(), buf.size());
            auto n = ifs.gcount();
            if (n <= 0) break;
            if (EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(n)) != 1)
                return false;
        }
        return true;
    });
}

Result<std::string, std::string> Checksum::sha256_string(std::string_view data) {
    return evp_sha256([&](EVP_MD_CTX* ctx) -> bool {
        return EVP_DigestUpdate(ctx, data.data(), data.size()) == 1;
    });
}

VoidResult Checksum::write_sha256_file(const std::filesystem::path& target) {
    auto result = sha256_file(target);
    if (!result) return VoidResult::err(result.error());

    auto out_path = std::filesystem::path(target.string() + ".sha256");
    std::ofstream ofs(out_path);
    if (!ofs) return VoidResult::err(fmt::format("Cannot write: {}", out_path.string()));

    ofs << result.value() << "  " << target.filename().string() << "\n";
    log_ok(fmt::format("SHA-256: {} → {}", target.filename().string(), result.value().substr(0,16) + "..."));
    return VoidResult::ok();
}

Result<bool, std::string> Checksum::verify_sha256_file(
    const std::filesystem::path& target)
{
    auto sidecar = std::filesystem::path(target.string() + ".sha256");
    std::ifstream ifs(sidecar);
    if (!ifs) return Result<bool,std::string>::err(
        fmt::format("Checksum file not found: {}", sidecar.string()));

    std::string expected_hash, filename;
    ifs >> expected_hash >> filename;

    auto computed = sha256_file(target);
    if (!computed) return Result<bool,std::string>::err(computed.error());

    bool match = (computed.value() == expected_hash);
    return Result<bool,std::string>::ok(match);
}

VoidResult Checksum::generate_checksums_dir(
    const std::filesystem::path& dir,
    const std::filesystem::path& out_file)
{
    if (!std::filesystem::is_directory(dir))
        return VoidResult::err(fmt::format("Not a directory: {}", dir.string()));

    std::ofstream ofs(out_file);
    if (!ofs) return VoidResult::err(fmt::format("Cannot write: {}", out_file.string()));

    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        auto hash = sha256_file(entry.path());
        if (!hash) {
            log_warn(fmt::format("Skip {}: {}", entry.path().string(), hash.error()));
            continue;
        }
        ofs << hash.value() << "  " << entry.path().filename().string() << "\n";
    }

    log_ok(fmt::format("Checksums written to {}", out_file.string()));
    return VoidResult::ok();
}

} // namespace nexus
