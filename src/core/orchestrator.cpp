#include "orchestrator.hpp"
#include "config/config_parser.hpp"
#include "profiles/profile_manager.hpp"
#include "builder/rootfs_builder.hpp"
#include "builder/iso_builder.hpp"
#include "security/hardening_engine.hpp"
#include "packaging/package_manager.hpp"
#include "branding/branding_manager.hpp"
#include "testing/smoke_tester.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"
#include "utils/checksum.hpp"
#include "utils/manifest.hpp"
#include "nexus/version_gen.hpp"

#include <fmt/core.h>
#include <chrono>
#include <array>
#include <unistd.h>

namespace nexus {

// ── Required tools for building ───────────────────────────────────────────────
static constexpr std::array REQUIRED_TOOLS = {
    "debootstrap", "mksquashfs", "xorriso", "grub-mkimage",
    "grub-mkrescue", "chroot", "mount", "umount", "rsync",
    "dpkg", "apt-get"
};

static constexpr std::array OPTIONAL_TOOLS = {
    "qemu-system-x86_64", "syft", "cosign"
};

Orchestrator::Orchestrator(const BuildOptions& opts)
    : opts_(opts)
{
    // Load configuration
    auto cfg_result = ConfigParser::load();
    if (!cfg_result) {
        log_warn(fmt::format("Config load warning: {}", cfg_result.error()));
        cfg_ = std::make_shared<NexusConfig>();
    } else {
        cfg_ = std::make_shared<NexusConfig>(std::move(cfg_result.value()));
    }

    // Apply BuildOptions overrides
    if (!opts_.profile.empty()) {}  // profile is in opts
    if (!opts_.workspace.empty())   cfg_->workspace_dir = opts_.workspace;
    if (!opts_.output_dir.empty())  cfg_->output_dir    = opts_.output_dir;
    if (!opts_.mirror.empty())      cfg_->debian_mirror = opts_.mirror;
    if (!opts_.suite.empty())       cfg_->debian_suite  = opts_.suite;
    if (opts_.jobs > 0)             cfg_->jobs          = opts_.jobs;

    init_components();
}

void Orchestrator::init_components() {
    profile_mgr_    = std::make_shared<ProfileManager>(cfg_->profiles_dir);
    pkg_mgr_        = std::make_shared<PackageManager>(*cfg_);
    hardening_      = std::make_shared<HardeningEngine>(*cfg_);
    rootfs_builder_ = std::make_shared<RootfsBuilder>(*cfg_, *pkg_mgr_);
    iso_builder_    = std::make_shared<IsoBuilder>(*cfg_);
    branding_       = std::make_shared<BrandingManager>(*cfg_);
    smoker_         = std::make_shared<SmokeTester>(*cfg_);
}

void Orchestrator::print_banner() {
    fmt::print(R"(
  ███╗   ██╗███████╗██╗  ██╗██╗   ██╗███████╗
  ████╗  ██║██╔════╝╚██╗██╔╝██║   ██║██╔════╝
  ██╔██╗ ██║█████╗   ╚███╔╝ ██║   ██║███████╗
  ██║╚██╗██║██╔══╝   ██╔██╗ ██║   ██║╚════██║
  ██║ ╚████║███████╗██╔╝ ██╗╚██████╔╝███████║
  ╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝
  NexusOS {} — {} — Professional Cybersecurity Linux
  Profile: {} | Mode: {}
)",
    VERSION_STRING, DISTRO_CODENAME,
    opts_.profile, to_string(opts_.mode));
}

std::filesystem::path Orchestrator::rootfs_path() const {
    return cfg_->workspace_dir / "rootfs";
}

std::filesystem::path Orchestrator::iso_path() const {
    return opts_.output_dir /
        fmt::format("NexusOS-{}-{}-{}.iso",
            opts_.profile, VERSION_STRING, cfg_->arch);
}

std::string Orchestrator::artifact_name() const {
    return fmt::format("NexusOS-{}-{}-{}",
        opts_.profile, VERSION_STRING, cfg_->arch);
}

PipelineResult Orchestrator::run_pipeline(
    const std::string& name,
    std::vector<PipelineStep> steps)
{
    PipelineResult result;
    auto t0 = std::chrono::steady_clock::now();

    fmt::print("\n┌─ Pipeline: {} ────────────────────────\n", name);

    for (auto& step : steps) {
        log_step(step.name, step.description);

        if (opts_.dry_run && step.skip_on_dry_run) {
            log_warn(fmt::format("  [DRY-RUN] Skipping: {}", step.name));
            result.skipped_steps.push_back(step.name);
            continue;
        }

        auto r = step.action();
        if (!r.is_ok()) {
            log_fail(fmt::format("{}: {}", step.name, r.error()));
            result.failed_steps.push_back(step.name);
            result.success = false;

            if (!step.optional) {
                fmt::print("└─ Pipeline: {} FAILED at step '{}'\n\n", name, step.name);
                break;
            } else {
                log_warn(fmt::format("Optional step failed, continuing: {}", step.name));
            }
        } else {
            log_ok(step.name);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::seconds>(t1 - t0);

    if (result.success)
        fmt::print("└─ Pipeline: {} OK ({} s)\n\n", name, result.elapsed.count());

    return result;
}

VoidResult Orchestrator::check_required_tools() {
    std::vector<std::string> missing;
    for (auto& tool : REQUIRED_TOOLS) {
        if (!Process::which(tool))
            missing.push_back(tool);
    }
    if (!missing.empty()) {
        std::string msg = "Missing required tools:";
        for (auto& t : missing) msg += " " + t;
        return VoidResult::err(msg);
    }
    for (auto& tool : OPTIONAL_TOOLS) {
        if (!Process::which(tool))
            log_warn(fmt::format("Optional tool not found: {}", tool));
    }
    return VoidResult::ok();
}

VoidResult Orchestrator::prepare_workspace() {
    std::error_code ec;
    for (auto& d : {cfg_->workspace_dir, cfg_->output_dir, cfg_->cache_dir}) {
        std::filesystem::create_directories(d, ec);
        if (ec) return VoidResult::err(
            fmt::format("Cannot create directory {}: {}", d.string(), ec.message()));
    }
    return VoidResult::ok();
}

// ── doctor ────────────────────────────────────────────────────────────────────
VoidResult Orchestrator::doctor() {
    print_banner();
    fmt::print("Checking environment...\n\n");

    bool all_ok = true;

    // Check root
    if (geteuid() != 0) {
        log_warn("Not running as root — build steps will require sudo");
    } else {
        log_ok("Running as root");
    }

    // Check required tools
    for (auto& tool : REQUIRED_TOOLS) {
        auto path = Process::find_binary(tool);
        if (path) log_ok(fmt::format("{}: {}", tool, path->string()));
        else { log_fail(tool); all_ok = false; }
    }

    for (auto& tool : OPTIONAL_TOOLS) {
        auto path = Process::find_binary(tool);
        if (path) log_ok(fmt::format("{} (optional): {}", tool, path->string()));
        else log_warn(fmt::format("{} (optional): not found", tool));
    }

    // Check kernel version
    auto kver = Process::shell("uname -r");
    if (kver.success())
        log_ok(fmt::format("Kernel: {}", kver.stdout_out.substr(0, kver.stdout_out.find('\n'))));

    // Check disk space on workspace
    auto df = Process::shell(fmt::format("df -BG {} 2>/dev/null | awk 'NR==2{{print $4}}'",
        cfg_->workspace_dir.string()));
    if (df.success())
        log_ok(fmt::format("Disk space (workspace): {}", df.stdout_out.substr(0, df.stdout_out.find('\n'))));

    // Check profiles
    auto profiles = profile_mgr_->list();
    log_ok(fmt::format("Profiles found: {}", profiles.size()));
    for (auto& p : profiles)
        fmt::print("    - {}\n", p);

    // Config summary
    fmt::print("\nConfiguration:\n");
    fmt::print("  Workspace : {}\n", cfg_->workspace_dir.string());
    fmt::print("  Output    : {}\n", cfg_->output_dir.string());
    fmt::print("  Cache     : {}\n", cfg_->cache_dir.string());
    fmt::print("  Mirror    : {}\n", cfg_->debian_mirror);
    fmt::print("  Suite     : {}\n", cfg_->debian_suite);
    fmt::print("  Arch      : {}\n", cfg_->arch);
    fmt::print("  Jobs      : {}\n", cfg_->jobs);

    if (!all_ok) {
        fmt::print("\n  Install missing tools:\n");
        fmt::print("    bash scripts/bootstrap/install-deps.sh\n\n");
        return VoidResult::err("Environment check failed — install missing dependencies");
    }

    fmt::print("\nEnvironment: OK\n\n");
    return VoidResult::ok();
}

// ── init ──────────────────────────────────────────────────────────────────────
VoidResult Orchestrator::init() {
    log_step("init", "Initialising workspace");
    auto r = prepare_workspace();
    if (!r) return r;

    auto cfg_r = ConfigParser::write_default("nexus.yaml");
    if (!cfg_r && cfg_r.error().find("already exists") == std::string::npos)
        return cfg_r;

    log_ok("Workspace initialised at " + cfg_->workspace_dir.string());
    return VoidResult::ok();
}

// ── build_rootfs ──────────────────────────────────────────────────────────────
VoidResult Orchestrator::build_rootfs() {
    print_banner();

    auto pkgs_r = profile_mgr_->get_packages(opts_.profile, opts_.mode);
    if (!pkgs_r) return VoidResult::err(pkgs_r.error());

    auto overlays_r = profile_mgr_->get_overlays(opts_.profile, opts_.mode);
    if (!overlays_r) return VoidResult::err(overlays_r.error());

    auto profile_r = profile_mgr_->resolve(opts_.profile, opts_.mode);
    if (!profile_r) return VoidResult::err(profile_r.error());

    const auto& profile  = profile_r.value();
    const auto& packages = pkgs_r.value();
    const auto& overlays = overlays_r.value();
    const auto rootfs    = rootfs_path();

    auto result = run_pipeline("build-rootfs", {
        {"prepare-workspace",  "Creating workspace directories",
            [&]{ return prepare_workspace(); }, true},

        {"check-tools",        "Verifying required build tools",
            [&]{ return check_required_tools(); }},

        {"debootstrap",        fmt::format("Bootstrapping Debian {} ({})",
                cfg_->debian_suite, cfg_->arch),
            [&]{ return rootfs_builder_->debootstrap(rootfs,
                cfg_->debian_mirror, cfg_->debian_suite, cfg_->arch); },
            false},

        {"mount-vfs",          "Mounting /proc /sys /dev inside chroot",
            [&]{ return Process::mount_chroot_vfs(rootfs); }},

        {"configure-base",     "Configuring apt, locale, timezone",
            [&]{ return rootfs_builder_->configure_base(rootfs,
                cfg_->debian_mirror, cfg_->debian_suite); }},

        {"install-packages",   fmt::format("Installing {} packages (profile: {})",
                packages.size(), opts_.profile),
            [&]{ return pkg_mgr_->install(rootfs, packages); }},

        {"apply-overlays",     "Applying filesystem overlays",
            [&]{ return rootfs_builder_->apply_overlays(rootfs, overlays,
                cfg_->overlays_dir); }},

        {"apply-branding",     "Applying NexusOS branding",
            [&]{ return branding_->apply(rootfs, opts_.profile); }},

        {"setup-user",         "Creating default nexus user",
            [&]{ return rootfs_builder_->setup_user(rootfs); }},

        {"pre-harden-hook",    "Running pre-harden profile hook",
            [&]() -> VoidResult {
                if (profile.hook_post_packages.empty()) return VoidResult::ok();
                return rootfs_builder_->run_hook(rootfs,
                    cfg_->scripts_dir / "chroot" / profile.hook_post_packages);
            }, false, true},

        {"generate-initramfs", "Generating initramfs",
            [&]{ return rootfs_builder_->generate_initramfs(rootfs); }},

        {"umount-vfs",         "Unmounting chroot virtual filesystems",
            [&]{ return Process::umount_chroot_vfs(rootfs); }},
    });

    if (!result.success)
        return VoidResult::err("build-rootfs pipeline failed");
    return VoidResult::ok();
}

// ── harden ────────────────────────────────────────────────────────────────────
VoidResult Orchestrator::harden() {
    const auto rootfs = rootfs_path();
    if (!std::filesystem::exists(rootfs))
        return VoidResult::err("Rootfs not found — run build-rootfs first");

    auto result = run_pipeline("harden", {
        {"mount-vfs",       "Mounting chroot VFS",
            [&]{ return Process::mount_chroot_vfs(rootfs); }},

        {"sysctl",          "Applying sysctl hardening",
            [&]{ return hardening_->apply_sysctl(rootfs, opts_.mode); }},

        {"apparmor",        "Configuring AppArmor",
            [&]{ return hardening_->configure_apparmor(rootfs); }},

        {"auditd",          "Configuring auditd",
            [&]{ return hardening_->configure_auditd(rootfs, opts_.mode); }},

        {"nftables",        "Installing nftables rules",
            [&]{ return hardening_->configure_nftables(rootfs, opts_.mode); }},

        {"systemd-harden",  "Hardening systemd units",
            [&]{ return hardening_->harden_systemd(rootfs, opts_.mode); }},

        {"umount-vfs",      "Unmounting chroot VFS",
            [&]{ return Process::umount_chroot_vfs(rootfs); }},
    });

    if (!result.success) return VoidResult::err("harden pipeline failed");
    return VoidResult::ok();
}

// ── build_iso ─────────────────────────────────────────────────────────────────
VoidResult Orchestrator::build_iso() {
    const auto rootfs   = rootfs_path();
    const auto iso_out  = iso_path();

    if (!std::filesystem::exists(rootfs))
        return VoidResult::err("Rootfs not found — run build-rootfs first");

    std::filesystem::create_directories(opts_.output_dir);

    auto result = run_pipeline("build-iso", {
        {"squashfs",      "Compressing filesystem with SquashFS",
            [&]{ return iso_builder_->build_squashfs(rootfs,
                cfg_->workspace_dir / "squashfs"); }, false},

        {"prepare-iso-tree", "Preparing ISO directory tree",
            [&]{ return iso_builder_->prepare_iso_tree(
                cfg_->workspace_dir / "squashfs",
                cfg_->workspace_dir / "iso",
                cfg_->assets_dir); }},

        {"grub-bios",     "Installing GRUB BIOS support",
            [&]{ return iso_builder_->install_grub_bios(
                cfg_->workspace_dir / "iso",
                rootfs); }},

        {"grub-uefi",     "Installing GRUB UEFI support",
            [&]{ return iso_builder_->install_grub_uefi(
                cfg_->workspace_dir / "iso",
                rootfs); }},

        {"xorriso",       fmt::format("Generating ISO: {}", iso_out.filename().string()),
            [&]{ return iso_builder_->generate_iso(
                cfg_->workspace_dir / "iso",
                iso_out,
                artifact_name()); }, false},

        {"checksum",      "Computing SHA-256 checksum",
            [&]{ return Checksum::write_sha256_file(iso_out); }},
    });

    if (!result.success) return VoidResult::err("build-iso pipeline failed");
    return VoidResult::ok();
}

// ── verify ────────────────────────────────────────────────────────────────────
VoidResult Orchestrator::verify(const std::filesystem::path& iso) {
    log_step("verify", iso.filename().string());
    auto r = Checksum::verify_sha256_file(iso);
    if (!r) return VoidResult::err(r.error());
    if (!r.value()) return VoidResult::err("SHA-256 mismatch — ISO is corrupted or tampered");
    log_ok(fmt::format("{}: checksum verified", iso.filename().string()));
    return VoidResult::ok();
}

// ── generate_manifest ─────────────────────────────────────────────────────────
VoidResult Orchestrator::generate_manifest() {
    log_step("manifest", "Generating build manifest");
    auto m_r = Manifest::build(opts_.output_dir, opts_.profile, to_string(opts_.mode));
    if (!m_r) return VoidResult::err(m_r.error());

    auto& m = m_r.value();

    // Try to collect installed packages
    auto rootfs = rootfs_path();
    if (std::filesystem::exists(rootfs)) {
        auto pkgs = Manifest::packages_from_rootfs(rootfs);
        if (pkgs) m.packages = pkgs.value();
    }

    auto out = opts_.output_dir / fmt::format("{}.manifest.json", artifact_name());
    auto w = Manifest::write_json(m, out);
    if (!w) return w;

    Manifest::print(m);
    return VoidResult::ok();
}

// ── smoke_test ────────────────────────────────────────────────────────────────
VoidResult Orchestrator::smoke_test(const std::filesystem::path& iso) {
    if (!std::filesystem::exists(iso))
        return VoidResult::err(fmt::format("ISO not found: {}", iso.string()));
    return smoker_->run(iso, opts_.mode);
}

// ── clean ─────────────────────────────────────────────────────────────────────
VoidResult Orchestrator::clean() {
    log_step("clean", "Cleaning workspace");

    // Ensure VFS is unmounted
    auto rootfs = rootfs_path();
    Process::umount_chroot_vfs(rootfs);

    std::error_code ec;
    std::filesystem::remove_all(cfg_->workspace_dir, ec);
    if (ec) log_warn(fmt::format("clean: {}", ec.message()));
    else    log_ok("Workspace cleaned");
    return VoidResult::ok();
}

// ── release ───────────────────────────────────────────────────────────────────
VoidResult Orchestrator::release() {
    print_banner();

    auto steps = std::vector<PipelineStep>{
        {"build-rootfs",  "Full rootfs build",
            [&]{ return build_rootfs(); }, false},

        {"harden",        "Apply hardening profile",
            [&]{ return harden(); }, false},

        {"build-iso",     "Generate bootable ISO",
            [&]{ return build_iso(); }, false},

        {"manifest",      "Generate build manifest",
            [&]{ return generate_manifest(); }, false},
    };

    if (!opts_.skip_smoke) {
        steps.push_back({"smoke-test", "QEMU smoke test",
            [&]{ return smoke_test(iso_path()); }, false, true});
    }

    auto result = run_pipeline("release", std::move(steps));
    if (!result.success) return VoidResult::err("Release pipeline failed");

    fmt::print("Release artifacts in: {}\n", opts_.output_dir.string());
    return VoidResult::ok();
}

} // namespace nexus
