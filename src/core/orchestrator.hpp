#pragma once

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <memory>
#include "nexus/common.hpp"

namespace nexus {

class ConfigParser;
struct NexusConfig;
class ProfileManager;
class RootfsBuilder;
class IsoBuilder;
class HardeningEngine;
class PackageManager;
class BrandingManager;
class SmokeTester;

// ── PipelineStep ─────────────────────────────────────────────────────────────
struct PipelineStep {
    std::string          name;
    std::string          description;
    std::function<VoidResult()> action;
    bool                 skip_on_dry_run {false};
    bool                 optional        {false};
};

// ── PipelineResult ────────────────────────────────────────────────────────────
struct PipelineResult {
    bool                          success    {true};
    std::vector<std::string>      failed_steps {};
    std::vector<std::string>      skipped_steps {};
    std::chrono::seconds          elapsed     {0};
};

// ── Orchestrator — coordinates the full build pipeline ────────────────────────
class Orchestrator {
public:
    explicit Orchestrator(const BuildOptions& opts);

    // ── High-level commands (called by CLI) ───────────────────────────────────

    // Run environment validation
    [[nodiscard]] VoidResult doctor();

    // Initialise workspace (directories, nexus.yaml)
    [[nodiscard]] VoidResult init();

    // Build root filesystem for a profile
    [[nodiscard]] VoidResult build_rootfs();

    // Apply hardening to an existing rootfs
    [[nodiscard]] VoidResult harden();

    // Build bootable ISO from rootfs
    [[nodiscard]] VoidResult build_iso();

    // Verify ISO checksum
    [[nodiscard]] VoidResult verify(const std::filesystem::path& iso_path);

    // Generate manifest for release directory
    [[nodiscard]] VoidResult generate_manifest();

    // Run smoke test in QEMU
    [[nodiscard]] VoidResult smoke_test(const std::filesystem::path& iso_path);

    // Clean workspace
    [[nodiscard]] VoidResult clean();

    // Full release pipeline: build_rootfs + harden + build_iso + checksums + manifest + smoke
    [[nodiscard]] VoidResult release();

    // ── Status ────────────────────────────────────────────────────────────────
    [[nodiscard]] const BuildOptions& options() const noexcept { return opts_; }
    [[nodiscard]] const NexusConfig&  config()  const noexcept { return *cfg_; }

private:
    BuildOptions                         opts_;
    std::shared_ptr<NexusConfig>         cfg_;
    std::shared_ptr<ProfileManager>      profile_mgr_;
    std::shared_ptr<RootfsBuilder>       rootfs_builder_;
    std::shared_ptr<IsoBuilder>          iso_builder_;
    std::shared_ptr<HardeningEngine>     hardening_;
    std::shared_ptr<PackageManager>      pkg_mgr_;
    std::shared_ptr<BrandingManager>     branding_;
    std::shared_ptr<SmokeTester>         smoker_;

    [[nodiscard]] PipelineResult run_pipeline(
        const std::string& name,
        std::vector<PipelineStep> steps);

    [[nodiscard]] VoidResult prepare_workspace();
    [[nodiscard]] VoidResult check_required_tools();
    [[nodiscard]] std::filesystem::path rootfs_path() const;
    [[nodiscard]] std::filesystem::path iso_path() const;
    [[nodiscard]] std::string artifact_name() const;

    void init_components();
    void print_banner();
};

} // namespace nexus
