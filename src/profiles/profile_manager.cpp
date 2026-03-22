#include "profile_manager.hpp"
#include "utils/logger.hpp"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <stdexcept>
#include <fmt/core.h>

namespace nexus {

namespace {

template<typename T>
T yget(const YAML::Node& n, const std::string& k, const T& def) {
    if (!n[k]) return def;
    try { return n[k].as<T>(); }
    catch (...) { return def; }
}

std::vector<std::string> yget_list(const YAML::Node& n, const std::string& k) {
    std::vector<std::string> out;
    if (!n[k] || !n[k].IsSequence()) return out;
    for (auto& item : n[k]) out.push_back(item.as<std::string>());
    return out;
}

void dedup(std::vector<std::string>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

} // anonymous namespace

ProfileManager::ProfileManager(const std::filesystem::path& profiles_dir)
    : profiles_dir_(profiles_dir)
{}

std::vector<std::string> ProfileManager::list() const {
    std::vector<std::string> names;
    if (!std::filesystem::is_directory(profiles_dir_)) return names;
    for (auto& entry : std::filesystem::directory_iterator(profiles_dir_)) {
        if (entry.path().extension() == ".yaml")
            names.push_back(entry.path().stem().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

Result<Profile, std::string> ProfileManager::load_yaml(
    const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
        return Result<Profile,std::string>::err(
            fmt::format("Profile file not found: {}", path.string()));

    YAML::Node doc;
    try { doc = YAML::LoadFile(path.string()); }
    catch (const YAML::Exception& e) {
        return Result<Profile,std::string>::err(
            fmt::format("YAML error in {}: {}", path.string(), e.what()));
    }

    Profile p;
    p.name              = yget<std::string>(doc, "name",        path.stem().string());
    p.description       = yget<std::string>(doc, "description", "");
    p.version           = yget<std::string>(doc, "version",     "0.1.0");
    p.desktop           = yget<std::string>(doc, "desktop",     "xfce4");
    p.support_hardened  = yget<bool>(doc, "support_hardened",  true);
    p.support_lab       = yget<bool>(doc, "support_lab",       true);

    p.extends           = yget_list(doc, "extends");
    p.packages          = yget_list(doc, "packages");
    p.packages_hardened = yget_list(doc, "packages_hardened");
    p.packages_lab      = yget_list(doc, "packages_lab");
    p.packages_exclude  = yget_list(doc, "packages_exclude");
    p.overlays          = yget_list(doc, "overlays");

    p.hook_pre_packages  = yget<std::string>(doc, "hook_pre_packages",  "");
    p.hook_post_packages = yget<std::string>(doc, "hook_post_packages", "");
    p.hook_post_harden   = yget<std::string>(doc, "hook_post_harden",   "");

    if (doc["config"] && doc["config"].IsMap()) {
        for (const auto& kv : doc["config"])
            p.config[kv.first.as<std::string>()] = kv.second.as<std::string>();
    }

    return Result<Profile,std::string>::ok(std::move(p));
}

void ProfileManager::merge_into(Profile& base, const Profile& ext) {
    // ext's packages are prepended (base overrides)
    base.packages.insert(base.packages.begin(),
        ext.packages.begin(), ext.packages.end());
    base.packages_hardened.insert(base.packages_hardened.begin(),
        ext.packages_hardened.begin(), ext.packages_hardened.end());
    base.packages_lab.insert(base.packages_lab.begin(),
        ext.packages_lab.begin(), ext.packages_lab.end());
    // overlays: ext goes first
    base.overlays.insert(base.overlays.begin(),
        ext.overlays.begin(), ext.overlays.end());
    // config: ext doesn't override base keys
    for (auto& [k, v] : ext.config)
        if (!base.config.count(k)) base.config[k] = v;
    // hooks: only set if base is empty
    if (base.hook_pre_packages.empty())  base.hook_pre_packages  = ext.hook_pre_packages;
    if (base.hook_post_packages.empty()) base.hook_post_packages = ext.hook_post_packages;
    if (base.hook_post_harden.empty())   base.hook_post_harden   = ext.hook_post_harden;
}

Result<Profile, std::string> ProfileManager::merge_inheritance(
    const Profile& p,
    std::vector<std::string>& visited) const
{
    Profile merged = p;

    for (auto& parent_name : p.extends) {
        if (std::find(visited.begin(), visited.end(), parent_name) != visited.end())
            return Result<Profile,std::string>::err(
                fmt::format("Circular profile dependency: {}", parent_name));

        visited.push_back(parent_name);
        auto parent_path = profiles_dir_ / (parent_name + ".yaml");
        auto parent = load_yaml(parent_path);
        if (!parent) return Result<Profile,std::string>::err(parent.error());

        auto resolved_parent = merge_inheritance(parent.value(), visited);
        if (!resolved_parent) return Result<Profile,std::string>::err(resolved_parent.error());

        merge_into(merged, resolved_parent.value());
    }
    return Result<Profile,std::string>::ok(std::move(merged));
}

Result<Profile, std::string> ProfileManager::load(const std::string& name) const {
    auto path = profiles_dir_ / (name + ".yaml");
    auto p = load_yaml(path);
    if (!p) return p;
    std::vector<std::string> visited = {name};
    return merge_inheritance(p.value(), visited);
}

Result<Profile, std::string> ProfileManager::resolve(
    const std::string& name,
    BuildMode mode) const
{
    auto p = load(name);
    if (!p) return p;

    auto& prof = p.value();

    if (mode == BuildMode::Hardened && !prof.support_hardened)
        return Result<Profile,std::string>::err(
            fmt::format("Profile '{}' does not support hardened mode", name));
    if (mode == BuildMode::Lab && !prof.support_lab)
        return Result<Profile,std::string>::err(
            fmt::format("Profile '{}' does not support lab mode", name));

    // Add mode-specific overlays
    std::string mode_overlay = (mode == BuildMode::Hardened) ? "hardened" : "lab";
    if (std::find(prof.overlays.begin(), prof.overlays.end(), mode_overlay)
        == prof.overlays.end())
        prof.overlays.push_back(mode_overlay);

    return p;
}

Result<std::vector<std::string>, std::string> ProfileManager::get_packages(
    const std::string& name,
    BuildMode mode) const
{
    auto p = resolve(name, mode);
    if (!p) return Result<std::vector<std::string>,std::string>::err(p.error());

    auto& prof = p.value();
    std::vector<std::string> pkgs = prof.packages;

    if (mode == BuildMode::Hardened)
        pkgs.insert(pkgs.end(), prof.packages_hardened.begin(), prof.packages_hardened.end());
    else if (mode == BuildMode::Lab)
        pkgs.insert(pkgs.end(), prof.packages_lab.begin(), prof.packages_lab.end());

    // Remove excluded packages
    for (auto& excl : prof.packages_exclude) {
        pkgs.erase(std::remove(pkgs.begin(), pkgs.end(), excl), pkgs.end());
    }

    dedup(pkgs);
    return Result<std::vector<std::string>,std::string>::ok(std::move(pkgs));
}

Result<std::vector<std::string>, std::string> ProfileManager::get_overlays(
    const std::string& name,
    BuildMode mode) const
{
    auto p = resolve(name, mode);
    if (!p) return Result<std::vector<std::string>,std::string>::err(p.error());

    std::vector<std::string> overlays = {"common"};  // always first
    for (auto& o : p.value().overlays)
        if (o != "common") overlays.push_back(o);

    // Ensure no duplicates
    std::vector<std::string> seen;
    std::vector<std::string> deduped;
    for (auto& o : overlays) {
        if (std::find(seen.begin(), seen.end(), o) == seen.end()) {
            deduped.push_back(o);
            seen.push_back(o);
        }
    }
    return Result<std::vector<std::string>,std::string>::ok(std::move(deduped));
}

std::vector<std::string> ProfileManager::validate_all() const {
    std::vector<std::string> issues;
    for (auto& name : list()) {
        auto p = load(name);
        if (!p) {
            issues.push_back(fmt::format("[{}] {}", name, p.error()));
            continue;
        }
        if (p.value().packages.empty())
            issues.push_back(fmt::format("[{}] has empty package list", name));
    }
    return issues;
}

void ProfileManager::print(const Profile& p) {
    fmt::print("\n┌─ Profile: {} ─────────────────────────\n", p.name);
    fmt::print("│  Description : {}\n", p.description);
    fmt::print("│  Desktop     : {}\n", p.desktop);
    fmt::print("│  Extends     : {}\n",
        p.extends.empty() ? "(none)" :
        [&]{ std::string s; for (auto& e : p.extends) s += e + " "; return s; }());
    fmt::print("│  Packages    : {}\n", p.packages.size());
    fmt::print("│  Overlays    : ");
    for (auto& o : p.overlays) fmt::print("{} ", o);
    fmt::print("\n│  Hardened    : {}\n", p.support_hardened ? "yes" : "no");
    fmt::print("│  Lab         : {}\n", p.support_lab      ? "yes" : "no");
    fmt::print("└────────────────────────────────────────\n\n");
}

} // namespace nexus
