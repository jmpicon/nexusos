#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <filesystem>
#include <system_error>

#include "core/orchestrator.hpp"
#include "utils/logger.hpp"
#include "nexus/version_gen.hpp"

using namespace nexus;

int main(int argc, char* argv[]) {
    CLI::App app{
        fmt::format("NexusOS Builder v{} — Professional Cybersecurity Linux Distribution",
            VERSION_STRING)
    };
    app.set_version_flag("-V,--version",
        fmt::format("{} ({})", VERSION_STRING, DISTRO_CODENAME));

    // ── Global options ────────────────────────────────────────────────────────
    bool        verbose      = false;
    bool        dry_run      = false;
    bool        no_color     = false;
    bool        quiet        = false;
    std::string project_dir  = "";

    app.add_flag("-v,--verbose",  verbose,     "Enable verbose output");
    app.add_flag("-n,--dry-run",  dry_run,     "Show what would be done without executing");
    app.add_flag("--no-color",    no_color,    "Disable colored output");
    app.add_flag("-q,--quiet",    quiet,       "Suppress most output");
    app.add_option("-C,--project-dir", project_dir,
        "Project root directory (default: current working directory)");

    auto set_common_opts = [&](BuildOptions& opts) {
        opts.verbose = verbose;
        opts.dry_run = dry_run;
        Logger::instance().set_level(verbose ? LogLevel::DEBUG : LogLevel::INFO);
        Logger::instance().set_color(!no_color);
        Logger::instance().set_quiet(quiet);
        // Change to project root so relative paths (profiles/, overlays/) resolve
        if (!project_dir.empty()) {
            std::error_code ec;
            std::filesystem::current_path(project_dir, ec);
            if (ec) {
                fmt::print(stderr, "Error: cannot cd to '{}': {}\n",
                    project_dir, ec.message());
                std::exit(1);
            }
        }
    };

    // ── doctor ────────────────────────────────────────────────────────────────
    auto* doctor_cmd = app.add_subcommand("doctor",
        "Validate build environment and tools");
    doctor_cmd->callback([&] {
        BuildOptions opts;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.doctor();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── init ──────────────────────────────────────────────────────────────────
    auto* init_cmd = app.add_subcommand("init",
        "Initialise workspace and write default nexus.yaml");
    std::string init_profile = "core";
    init_cmd->add_option("-p,--profile", init_profile, "Profile to target");
    init_cmd->callback([&] {
        BuildOptions opts;
        opts.profile = init_profile;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.init();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── build-rootfs ──────────────────────────────────────────────────────────
    auto* rootfs_cmd = app.add_subcommand("build-rootfs",
        "Build the root filesystem for a profile");
    std::string rootfs_profile = "core";
    std::string rootfs_mode    = "lab";
    std::string rootfs_mirror  = "";
    std::string rootfs_workspace = "";
    int         rootfs_jobs    = 4;

    rootfs_cmd->add_option("-p,--profile",   rootfs_profile, "Profile name")->required();
    rootfs_cmd->add_option("-m,--mode",       rootfs_mode,    "Mode: hardened|lab");
    rootfs_cmd->add_option("--mirror",        rootfs_mirror,  "Debian mirror URL");
    rootfs_cmd->add_option("--workspace",     rootfs_workspace,"Workspace directory");
    rootfs_cmd->add_option("-j,--jobs",       rootfs_jobs,    "Parallel jobs");

    rootfs_cmd->callback([&] {
        BuildOptions opts;
        opts.profile  = rootfs_profile;
        opts.mirror   = rootfs_mirror;
        opts.jobs     = rootfs_jobs;
        if (!rootfs_workspace.empty()) opts.workspace = rootfs_workspace;
        auto m = build_mode_from_string(rootfs_mode);
        if (!m) {
            fmt::print(stderr, "Unknown mode: {}. Use 'hardened' or 'lab'\n", rootfs_mode);
            std::exit(1);
        }
        opts.mode = *m;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.build_rootfs();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── harden ────────────────────────────────────────────────────────────────
    auto* harden_cmd = app.add_subcommand("harden",
        "Apply hardening to an existing rootfs");
    std::string harden_mode    = "hardened";
    std::string harden_profile = "core";
    std::string harden_ws      = "";

    harden_cmd->add_option("-m,--mode",      harden_mode,    "hardened|lab")->required();
    harden_cmd->add_option("-p,--profile",   harden_profile, "Profile name");
    harden_cmd->add_option("--workspace",    harden_ws,      "Workspace directory");

    harden_cmd->callback([&] {
        BuildOptions opts;
        opts.profile = harden_profile;
        if (!harden_ws.empty()) opts.workspace = harden_ws;
        auto m = build_mode_from_string(harden_mode);
        if (!m) {
            fmt::print(stderr, "Unknown mode: {}\n", harden_mode);
            std::exit(1);
        }
        opts.mode = *m;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.harden();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── build-iso ─────────────────────────────────────────────────────────────
    auto* iso_cmd = app.add_subcommand("build-iso",
        "Build bootable ISO from rootfs");
    std::string iso_profile = "core";
    std::string iso_mode    = "lab";
    std::string iso_output  = "./release";
    std::string iso_ws      = "";

    iso_cmd->add_option("-p,--profile",  iso_profile, "Profile name")->required();
    iso_cmd->add_option("-m,--mode",     iso_mode,    "hardened|lab");
    iso_cmd->add_option("-o,--output",   iso_output,  "Output directory");
    iso_cmd->add_option("--workspace",   iso_ws,      "Workspace directory");

    iso_cmd->callback([&] {
        BuildOptions opts;
        opts.profile    = iso_profile;
        opts.output_dir = iso_output;
        if (!iso_ws.empty()) opts.workspace = iso_ws;
        auto m = build_mode_from_string(iso_mode);
        if (!m) {
            fmt::print(stderr, "Unknown mode: {}\n", iso_mode);
            std::exit(1);
        }
        opts.mode = *m;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.build_iso();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── verify ────────────────────────────────────────────────────────────────
    auto* verify_cmd = app.add_subcommand("verify",
        "Verify ISO checksum against .sha256 sidecar");
    std::string verify_iso;
    verify_cmd->add_option("iso", verify_iso, "Path to ISO file")->required();

    verify_cmd->callback([&] {
        BuildOptions opts;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.verify(verify_iso);
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── manifest ──────────────────────────────────────────────────────────────
    auto* manifest_cmd = app.add_subcommand("manifest",
        "Generate JSON build manifest for release directory");
    std::string manifest_profile = "core";
    std::string manifest_mode    = "lab";
    std::string manifest_output  = "./release";

    manifest_cmd->add_option("-p,--profile", manifest_profile, "Profile name");
    manifest_cmd->add_option("-m,--mode",    manifest_mode,    "hardened|lab");
    manifest_cmd->add_option("-o,--output",  manifest_output,  "Release directory");

    manifest_cmd->callback([&] {
        BuildOptions opts;
        opts.profile    = manifest_profile;
        opts.output_dir = manifest_output;
        auto m = build_mode_from_string(manifest_mode);
        if (m) opts.mode = *m;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.generate_manifest();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── smoke-test ────────────────────────────────────────────────────────────
    auto* smoke_cmd = app.add_subcommand("smoke-test",
        "Boot ISO in QEMU and verify basic functionality");
    std::string smoke_iso;
    std::string smoke_mode = "lab";

    smoke_cmd->add_option("iso", smoke_iso, "Path to ISO file")->required();
    smoke_cmd->add_option("-m,--mode", smoke_mode, "hardened|lab");

    smoke_cmd->callback([&] {
        BuildOptions opts;
        auto m = build_mode_from_string(smoke_mode);
        if (m) opts.mode = *m;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.smoke_test(smoke_iso);
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── clean ─────────────────────────────────────────────────────────────────
    auto* clean_cmd = app.add_subcommand("clean",
        "Remove workspace (rootfs, squashfs staging)");
    std::string clean_ws = "";
    clean_cmd->add_option("--workspace", clean_ws, "Workspace to clean");

    clean_cmd->callback([&] {
        BuildOptions opts;
        if (!clean_ws.empty()) opts.workspace = clean_ws;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.clean();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── release ───────────────────────────────────────────────────────────────
    auto* release_cmd = app.add_subcommand("release",
        "Full release pipeline: rootfs → harden → ISO → manifest → smoke");
    std::string rel_profile    = "full";
    std::string rel_mode       = "hardened";
    std::string rel_output     = "./release";
    bool        rel_skip_smoke = false;
    int         rel_jobs       = 4;

    release_cmd->add_option("-p,--profile",   rel_profile,    "Profile name")->required();
    release_cmd->add_option("-m,--mode",       rel_mode,       "hardened|lab");
    release_cmd->add_option("-o,--output",     rel_output,     "Output directory");
    release_cmd->add_option("-j,--jobs",       rel_jobs,       "Parallel jobs");
    release_cmd->add_flag("--skip-smoke",      rel_skip_smoke, "Skip QEMU smoke test");

    release_cmd->callback([&] {
        BuildOptions opts;
        opts.profile     = rel_profile;
        opts.output_dir  = rel_output;
        opts.skip_smoke  = rel_skip_smoke;
        opts.jobs        = rel_jobs;
        auto m = build_mode_from_string(rel_mode);
        if (!m) {
            fmt::print(stderr, "Unknown mode: {}\n", rel_mode);
            std::exit(1);
        }
        opts.mode = *m;
        set_common_opts(opts);
        Orchestrator orch(opts);
        auto r = orch.release();
        if (!r) { fmt::print(stderr, "Error: {}\n", r.error()); std::exit(1); }
    });

    // ── profiles (list) ───────────────────────────────────────────────────────
    auto* profiles_cmd = app.add_subcommand("profiles",
        "List available build profiles");
    profiles_cmd->callback([&] {
        BuildOptions opts;
        set_common_opts(opts);
        Orchestrator orch(opts);
        fmt::print("\nAvailable profiles:\n");
        auto& cfg = orch.config();
        // ProfileManager is accessible via Orchestrator
        // (simplified: just print known profiles)
        for (auto& p : {"core", "analyst", "forensic", "blueteam",
                         "reverse", "cloud", "lab", "full"})
            fmt::print("  - {}\n", p);
        fmt::print("\nModes: hardened, lab\n\n");
    });

    // ── Parse ─────────────────────────────────────────────────────────────────
    app.require_subcommand(1);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    return 0;
}
