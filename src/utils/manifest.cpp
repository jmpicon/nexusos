#include "manifest.hpp"
#include "checksum.hpp"
#include "logger.hpp"
#include "process.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <fmt/core.h>

using json = nlohmann::json;

namespace nexus {

namespace {

std::string iso8601_now() {
    auto now  = std::chrono::system_clock::now();
    auto t    = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string get_hostname() {
    char buf[256] = {};
    gethostname(buf, sizeof(buf));
    return buf;
}

std::string get_git_commit() {
    auto r = Process::shell("git rev-parse --short HEAD 2>/dev/null || echo unknown");
    auto s = r.stdout_out;
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

} // anonymous namespace

Result<BuildManifest, std::string> Manifest::build(
    const std::filesystem::path& release_dir,
    const std::string& profile,
    const std::string& mode,
    const std::string& version)
{
    BuildManifest m;
    m.profile         = profile;
    m.mode            = mode;
    m.version         = version;
    m.build_date      = iso8601_now();
    m.builder_version = version; // same as project version
    m.git_commit      = get_git_commit();
    m.hostname        = get_hostname();

    if (!std::filesystem::is_directory(release_dir))
        return Result<BuildManifest,std::string>::err(
            fmt::format("release_dir not found: {}", release_dir.string()));

    for (auto& entry : std::filesystem::directory_iterator(release_dir)) {
        if (!entry.is_regular_file()) continue;
        auto& p = entry.path();
        auto  ext = p.extension().string();
        if (ext == ".sha256" || ext == ".manifest") continue;

        ManifestEntry me;
        me.filename   = p.filename().string();
        me.size_bytes = entry.file_size();

        auto hash = Checksum::sha256_file(p);
        if (!hash) me.sha256 = "unknown";
        else        me.sha256 = hash.value();

        if (ext == ".iso")    me.type = "iso";
        else if (ext == ".squashfs") me.type = "squashfs";
        else                  me.type = "artifact";

        m.artifacts.push_back(std::move(me));
    }

    return Result<BuildManifest,std::string>::ok(std::move(m));
}

VoidResult Manifest::write_json(const BuildManifest& m,
                                const std::filesystem::path& out_path)
{
    json j;
    j["distro_name"]     = m.distro_name;
    j["distro_codename"] = m.distro_codename;
    j["version"]         = m.version;
    j["profile"]         = m.profile;
    j["mode"]            = m.mode;
    j["arch"]            = m.arch;
    j["base_distro"]     = m.base_distro;
    j["build_date"]      = m.build_date;
    j["builder_version"] = m.builder_version;
    j["git_commit"]      = m.git_commit;
    j["hostname"]        = m.hostname;

    json artifacts = json::array();
    for (auto& a : m.artifacts) {
        artifacts.push_back({
            {"filename",   a.filename},
            {"sha256",     a.sha256},
            {"size_bytes", a.size_bytes},
            {"type",       a.type}
        });
    }
    j["artifacts"] = artifacts;
    j["packages"]  = m.packages;
    j["build_env"] = m.build_env;

    std::ofstream ofs(out_path);
    if (!ofs) return VoidResult::err(fmt::format("Cannot write: {}", out_path.string()));
    ofs << j.dump(2) << "\n";
    log_ok(fmt::format("Manifest written: {}", out_path.string()));
    return VoidResult::ok();
}

Result<BuildManifest, std::string> Manifest::read_json(
    const std::filesystem::path& path)
{
    std::ifstream ifs(path);
    if (!ifs) return Result<BuildManifest,std::string>::err(
        fmt::format("Cannot open: {}", path.string()));

    json j;
    try { j = json::parse(ifs); }
    catch (const json::exception& e) {
        return Result<BuildManifest,std::string>::err(
            fmt::format("JSON parse error: {}", e.what()));
    }

    BuildManifest m;
    m.distro_name     = j.value("distro_name", "NexusOS");
    m.distro_codename = j.value("distro_codename", "");
    m.version         = j.value("version", "");
    m.profile         = j.value("profile", "");
    m.mode            = j.value("mode", "");
    m.arch            = j.value("arch", "amd64");
    m.build_date      = j.value("build_date", "");
    m.git_commit      = j.value("git_commit", "");

    for (auto& a : j.value("artifacts", json::array())) {
        ManifestEntry e;
        e.filename   = a.value("filename", "");
        e.sha256     = a.value("sha256", "");
        e.size_bytes = a.value("size_bytes", 0ULL);
        e.type       = a.value("type", "");
        m.artifacts.push_back(std::move(e));
    }
    for (auto& p : j.value("packages", json::array()))
        m.packages.push_back(p.get<std::string>());

    return Result<BuildManifest,std::string>::ok(std::move(m));
}

Result<std::vector<std::string>, std::string> Manifest::packages_from_rootfs(
    const std::filesystem::path& rootfs)
{
    auto r = Process::chroot_run(rootfs, "dpkg-query",
        {"-W", "--showformat=${Package}=${Version}\n"});
    if (!r.success())
        return Result<std::vector<std::string>,std::string>::err(
            fmt::format("dpkg-query failed: {}", r.stderr_out));

    std::vector<std::string> pkgs;
    std::istringstream iss(r.stdout_out);
    std::string line;
    while (std::getline(iss, line))
        if (!line.empty()) pkgs.push_back(line);

    return Result<std::vector<std::string>,std::string>::ok(std::move(pkgs));
}

void Manifest::print(const BuildManifest& m) {
    fmt::print("\n┌─ NexusOS Build Manifest ────────────────────────\n");
    fmt::print("│  Distro   : {} {}\n", m.distro_name, m.version);
    fmt::print("│  Codename : {}\n", m.distro_codename);
    fmt::print("│  Profile  : {} ({})\n", m.profile, m.mode);
    fmt::print("│  Arch     : {}\n", m.arch);
    fmt::print("│  Base     : {}\n", m.base_distro);
    fmt::print("│  Built    : {}\n", m.build_date);
    fmt::print("│  Commit   : {}\n", m.git_commit);
    fmt::print("│  Packages : {}\n", m.packages.size());
    fmt::print("├─ Artifacts ──────────────────────────────────────\n");
    for (auto& a : m.artifacts)
        fmt::print("│  [{:<10}] {:50} {:>10} B\n",
            a.type, a.filename, a.size_bytes);
    fmt::print("└──────────────────────────────────────────────────\n\n");
}

} // namespace nexus
