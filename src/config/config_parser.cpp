#include "config_parser.hpp"
#include "logger.hpp"

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <cstdlib>
#include <fmt/core.h>

namespace nexus {

namespace {

template<typename T>
T yaml_get(const YAML::Node& node, const std::string& key, const T& def) {
    if (!node[key]) return def;
    try { return node[key].as<T>(); }
    catch (...) { return def; }
}

} // anonymous namespace

Result<NexusConfig, std::string> ConfigParser::load(
    const std::filesystem::path& path)
{
    std::filesystem::path cfg_path = path;

    // Search for config file
    if (!std::filesystem::exists(cfg_path)) {
        std::vector<std::filesystem::path> candidates = {
            "nexus.yaml",
            std::filesystem::current_path() / "nexus.yaml",
            std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "")
                / ".config" / "nexus" / "nexus.yaml"
        };
        cfg_path = "";
        for (auto& c : candidates) {
            if (std::filesystem::exists(c)) { cfg_path = c; break; }
        }
    }

    NexusConfig cfg;

    if (cfg_path.empty() || !std::filesystem::exists(cfg_path)) {
        log_warn("nexus.yaml not found — using built-in defaults");
        apply_env_overrides(cfg);
        return Result<NexusConfig,std::string>::ok(std::move(cfg));
    }

    log_debug(fmt::format("Loading config from: {}", cfg_path.string()));

    YAML::Node doc;
    try {
        doc = YAML::LoadFile(cfg_path.string());
    } catch (const YAML::Exception& e) {
        return Result<NexusConfig,std::string>::err(
            fmt::format("YAML parse error in {}: {}", cfg_path.string(), e.what()));
    }

    // Identity
    cfg.distro_name     = yaml_get<std::string>(doc, "distro_name",     cfg.distro_name);
    cfg.distro_codename = yaml_get<std::string>(doc, "distro_codename", cfg.distro_codename);
    cfg.version         = yaml_get<std::string>(doc, "version",         cfg.version);

    // Paths
    auto paths = doc["paths"];
    if (paths) {
        cfg.workspace_dir = yaml_get<std::string>(paths, "workspace", cfg.workspace_dir.string());
        cfg.output_dir    = yaml_get<std::string>(paths, "output",    cfg.output_dir.string());
        cfg.cache_dir     = yaml_get<std::string>(paths, "cache",     cfg.cache_dir.string());
        cfg.profiles_dir  = yaml_get<std::string>(paths, "profiles",  cfg.profiles_dir.string());
        cfg.overlays_dir  = yaml_get<std::string>(paths, "overlays",  cfg.overlays_dir.string());
    }

    // Debian base
    auto base = doc["base"];
    if (base) {
        cfg.debian_mirror  = yaml_get<std::string>(base, "mirror",  cfg.debian_mirror);
        cfg.debian_suite   = yaml_get<std::string>(base, "suite",   cfg.debian_suite);
        cfg.arch           = yaml_get<std::string>(base, "arch",    cfg.arch);
        cfg.include_firmware = yaml_get<bool>(base, "include_firmware", cfg.include_firmware);
    }

    // Build settings
    auto build = doc["build"];
    if (build) {
        cfg.jobs      = yaml_get<int>(build,  "jobs",      cfg.jobs);
        cfg.use_cache = yaml_get<bool>(build, "use_cache", cfg.use_cache);
        cfg.gpg_verify = yaml_get<bool>(build,"gpg_verify",cfg.gpg_verify);
    }

    // QEMU
    auto qemu = doc["qemu"];
    if (qemu) {
        cfg.qemu_binary  = yaml_get<std::string>(qemu, "binary",    cfg.qemu_binary);
        cfg.qemu_ram_mb  = yaml_get<int>(qemu,         "ram_mb",    cfg.qemu_ram_mb);
        cfg.qemu_timeout = yaml_get<int>(qemu,         "timeout",   cfg.qemu_timeout);
    }

    // Extra repos
    if (doc["extra_repos"] && doc["extra_repos"].IsSequence()) {
        for (auto& r : doc["extra_repos"])
            cfg.extra_repos.push_back(r.as<std::string>());
    }

    // Sysctl overrides
    if (doc["sysctl_overrides"] && doc["sysctl_overrides"].IsMap()) {
        for (auto& kv : doc["sysctl_overrides"])
            cfg.sysctl_overrides[kv.first.as<std::string>()] = kv.second.as<std::string>();
    }

    apply_env_overrides(cfg);

    auto issues = validate(cfg);
    if (!issues.empty()) {
        for (auto& i : issues) log_warn(i);
    }

    return Result<NexusConfig,std::string>::ok(std::move(cfg));
}

VoidResult ConfigParser::write_default(const std::filesystem::path& path) {
    if (std::filesystem::exists(path))
        return VoidResult::err(fmt::format("{} already exists — not overwriting", path.string()));

    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err(fmt::format("Cannot write: {}", path.string()));

    ofs << R"(# NexusOS — build configuration
# Documentation: docs/build.md

distro_name:     "NexusOS"
distro_codename: "Phantom"
version:         "0.1.0"

paths:
  workspace: "/tmp/nexus-workspace"
  output:    "./release"
  cache:     "/var/cache/nexus"
  profiles:  "profiles"
  overlays:  "overlays"

base:
  mirror:           "http://deb.debian.org/debian"
  suite:            "bookworm"
  arch:             "amd64"
  include_firmware: true

build:
  jobs:       4
  use_cache:  true
  gpg_verify: true

qemu:
  binary:  "qemu-system-x86_64"
  ram_mb:  2048
  timeout: 120

# extra_repos:
#   - "deb http://example.com/repo stable main"

# sysctl_overrides:
#   net.ipv4.ip_forward: "0"
)";

    log_ok(fmt::format("Default config written: {}", path.string()));
    return VoidResult::ok();
}

std::vector<std::string> ConfigParser::validate(const NexusConfig& cfg) {
    std::vector<std::string> issues;
    if (cfg.distro_name.empty())
        issues.push_back("distro_name is empty");
    if (cfg.debian_suite.empty())
        issues.push_back("base.suite is empty");
    if (cfg.jobs < 1 || cfg.jobs > 64)
        issues.push_back(fmt::format("build.jobs={} is out of range [1,64]", cfg.jobs));
    if (cfg.qemu_ram_mb < 512)
        issues.push_back("qemu.ram_mb should be >= 512");
    return issues;
}

void ConfigParser::apply_env_overrides(NexusConfig& cfg) {
    auto getenv_str = [](const char* key) -> std::optional<std::string> {
        const char* v = std::getenv(key);
        if (v && *v) return std::string(v);
        return std::nullopt;
    };

    if (auto v = getenv_str("NEXUS_MIRROR"))    cfg.debian_mirror = *v;
    if (auto v = getenv_str("NEXUS_SUITE"))     cfg.debian_suite  = *v;
    if (auto v = getenv_str("NEXUS_WORKSPACE")) cfg.workspace_dir = *v;
    if (auto v = getenv_str("NEXUS_OUTPUT"))    cfg.output_dir    = *v;
    if (auto v = getenv_str("NEXUS_CACHE"))     cfg.cache_dir     = *v;
    if (auto v = getenv_str("NEXUS_JOBS")) {
        try { cfg.jobs = std::stoi(*v); } catch (...) {}
    }
}

} // namespace nexus
