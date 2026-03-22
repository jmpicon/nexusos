#include "package_manager.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <fmt/core.h>

namespace nexus {

PackageManager::PackageManager(const NexusConfig& cfg) : cfg_(cfg) {}

Result<PackageList, std::string> PackageManager::load_list(
    const std::string& name) const
{
    auto path = cfg_.packages_dir / "lists" / (name + ".list");
    if (!std::filesystem::exists(path))
        return Result<PackageList,std::string>::err(
            fmt::format("Package list not found: {}", path.string()));

    PackageList pl;
    std::ifstream ifs(path);
    std::string line;
    while (std::getline(ifs, line)) {
        // Strip comments and whitespace
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
        while (!line.empty() && std::isspace(line.front())) line.erase(0, 1);
        while (!line.empty() && std::isspace(line.back()))  line.pop_back();
        if (line.empty()) continue;

        if (line.starts_with('-')) {
            // Exclusion: -package-name
            pl.excluded.push_back(line.substr(1));
        } else {
            pl.packages.push_back(line);
        }
    }

    log_debug(fmt::format("Loaded {} packages from {}", pl.packages.size(), name));
    return Result<PackageList,std::string>::ok(std::move(pl));
}

PackageList PackageManager::merge(const std::vector<PackageList>& lists) {
    PackageList merged;
    for (auto& l : lists) {
        merged.packages.insert(merged.packages.end(),
            l.packages.begin(), l.packages.end());
        merged.excluded.insert(merged.excluded.end(),
            l.excluded.begin(), l.excluded.end());
    }

    // Remove excluded packages
    for (auto& excl : merged.excluded) {
        merged.packages.erase(
            std::remove(merged.packages.begin(), merged.packages.end(), excl),
            merged.packages.end());
    }

    // De-duplicate while preserving order
    std::vector<std::string> seen;
    std::vector<std::string> deduped;
    for (auto& p : merged.packages) {
        if (std::find(seen.begin(), seen.end(), p) == seen.end()) {
            deduped.push_back(p);
            seen.push_back(p);
        }
    }
    merged.packages = std::move(deduped);
    return merged;
}

VoidResult PackageManager::apt_install(
    const std::filesystem::path& rootfs,
    const std::vector<std::string>& packages)
{
    if (packages.empty()) return VoidResult::ok();

    // Set DEBIAN_FRONTEND=noninteractive to suppress prompts
    std::vector<std::string> args = {
        "-o", "DPkg::Options::=--force-confnew",
        "-o", "DPkg::Options::=--force-overwrite",
        "-y", "--no-install-recommends",
        "install"
    };
    args.insert(args.end(), packages.begin(), packages.end());

    ProcessOptions opts;
    opts.env["DEBIAN_FRONTEND"] = "noninteractive";
    opts.env["DEBCONF_NONINTERACTIVE_SEEN"] = "true";
    opts.env["DEBCONF_FRONTEND"] = "noninteractive";
    opts.echo_cmd = cfg_.jobs > 0; // always show
    opts.stdout_callback = [](std::string_view line) {
        if (line.find("Unpacking") != std::string::npos ||
            line.find("Setting up") != std::string::npos)
            log_debug(line);
    };

    auto r = Process::chroot_run(rootfs, "apt-get", args, opts);
    if (!r.success())
        return VoidResult::err(fmt::format("apt-get install failed: {}", r.stderr_out));
    return VoidResult::ok();
}

VoidResult PackageManager::install(
    const std::filesystem::path& rootfs,
    const std::vector<std::string>& packages)
{
    if (packages.empty()) return VoidResult::ok();

    // Update first
    ProcessOptions opts;
    opts.env["DEBIAN_FRONTEND"] = "noninteractive";
    auto upd = Process::chroot_run(rootfs, "apt-get",
        {"update", "-qq"}, opts);
    if (!upd.success())
        log_warn("apt-get update warning: " + upd.stderr_out);

    // Install in batches of 50 to avoid extremely long command lines
    constexpr size_t BATCH = 50;
    size_t total = packages.size();
    size_t installed = 0;

    for (size_t i = 0; i < total; i += BATCH) {
        auto batch_end = std::min(i + BATCH, total);
        std::vector<std::string> batch(
            packages.begin() + i, packages.begin() + batch_end);

        log_info(fmt::format("Installing packages {}/{} ...",
            batch_end, total));

        auto r = apt_install(rootfs, batch);
        if (!r) return r;
        installed += batch.size();
    }

    // Clean up
    auto r = clean_cache(rootfs);
    if (!r) log_warn("Failed to clean apt cache: " + r.error());

    log_ok(fmt::format("{} packages installed", installed));
    return VoidResult::ok();
}

VoidResult PackageManager::install_list(
    const std::filesystem::path& rootfs,
    const std::string& list_name)
{
    auto pl = load_list(list_name);
    if (!pl) return VoidResult::err(pl.error());
    return install(rootfs, pl.value().packages);
}

Result<std::vector<std::string>, std::string> PackageManager::query_installed(
    const std::filesystem::path& rootfs) const
{
    auto r = Process::chroot_run(rootfs, "dpkg-query",
        {"-W", "--showformat=${Package}=${Version}\n"});
    if (!r.success())
        return Result<std::vector<std::string>,std::string>::err(
            "dpkg-query failed: " + r.stderr_out);

    std::vector<std::string> pkgs;
    std::istringstream iss(r.stdout_out);
    std::string line;
    while (std::getline(iss, line))
        if (!line.empty()) pkgs.push_back(line);

    return Result<std::vector<std::string>,std::string>::ok(std::move(pkgs));
}

std::vector<std::string> PackageManager::check_installed(
    const std::filesystem::path& rootfs,
    const std::vector<std::string>& expected) const
{
    std::vector<std::string> missing;
    for (auto& pkg : expected) {
        auto r = Process::chroot_run(rootfs, "dpkg-query",
            {"-W", "--showformat=${Status}", pkg});
        if (!r.success() ||
            r.stdout_out.find("install ok installed") == std::string::npos)
            missing.push_back(pkg);
    }
    return missing;
}

VoidResult PackageManager::clean_cache(const std::filesystem::path& rootfs) {
    Process::chroot_run(rootfs, "apt-get", {"clean"});
    Process::chroot_run(rootfs, "apt-get", {"autoclean"});

    // Remove apt lists to save space
    auto lists_dir = rootfs / "var" / "lib" / "apt" / "lists";
    for (auto& entry : std::filesystem::directory_iterator(lists_dir)) {
        if (entry.is_regular_file()) {
            std::error_code ec;
            std::filesystem::remove(entry.path(), ec);
        }
    }
    return VoidResult::ok();
}

} // namespace nexus
