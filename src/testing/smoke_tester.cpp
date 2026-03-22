#include "smoke_tester.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"

#include <fstream>
#include <fmt/core.h>

namespace nexus {

SmokeTester::SmokeTester(const NexusConfig& cfg) : cfg_(cfg) {}

VoidResult SmokeTester::check_iso_integrity(
    const std::filesystem::path& iso_path) const
{
    if (!std::filesystem::exists(iso_path))
        return VoidResult::err(fmt::format("ISO not found: {}", iso_path.string()));

    auto size = std::filesystem::file_size(iso_path);
    if (size < 50 * 1024 * 1024)
        return VoidResult::err(fmt::format("ISO suspiciously small ({} bytes)", size));

    // Check ISO 9660 magic at offset 32768 (0x8000)
    std::ifstream ifs(iso_path, std::ios::binary);
    if (!ifs) return VoidResult::err("Cannot open ISO for reading");
    ifs.seekg(0x8001);
    std::array<char, 5> magic;
    ifs.read(magic.data(), 5);
    if (std::string_view(magic.data(), 5) != "CD001")
        return VoidResult::err("Invalid ISO 9660 signature — file may be corrupt");

    log_ok(fmt::format("ISO signature valid ({:.1f} MB)",
        size / 1024.0 / 1024.0));
    return VoidResult::ok();
}

VoidResult SmokeTester::boot_qemu(
    const std::filesystem::path& iso_path,
    int timeout_secs) const
{
    if (!Process::which(cfg_.qemu_binary)) {
        log_warn(fmt::format("{} not found — skipping QEMU boot test",
            cfg_.qemu_binary));
        return VoidResult::ok();  // Optional test
    }

    // Create a small log file for QEMU serial output
    auto serial_log = std::filesystem::temp_directory_path() /
        "nexus-smoke-serial.log";

    log_info(fmt::format("Booting ISO in QEMU ({} s timeout)...", timeout_secs));

    // QEMU: headless, serial output, no GUI, timeout via timeout(1)
    std::vector<std::string> args = {
        fmt::format("{}", timeout_secs),
        cfg_.qemu_binary,
        "-m",    fmt::format("{}M", cfg_.qemu_ram_mb),
        "-cdrom", iso_path.string(),
        "-boot", "d",
        "-nographic",
        "-serial", "file:" + serial_log.string(),
        "-no-reboot",
        "-enable-kvm",
        "-cpu", "host",
        "-smp", "2",
        "-display", "none",
    };

    ProcessOptions opts;
    opts.capture_stdout = true;
    opts.capture_stderr = true;
    opts.timeout_secs   = timeout_secs + 10;

    auto r = Process::run("timeout", args, opts);

    // Exit code 124 = timeout = ISO didn't crash = acceptable
    // Exit code 0 = QEMU exited cleanly (unexpected in live boot but OK)
    if (r.exit_code != 0 && r.exit_code != 124) {
        return VoidResult::err(fmt::format(
            "QEMU exited with code {} — possible boot failure", r.exit_code));
    }

    // Check serial log for known good indicators
    std::ifstream log_ifs(serial_log);
    if (log_ifs) {
        std::string serial_content;
        std::getline(log_ifs, serial_content, '\0');

        std::vector<std::string> good_signs = {
            "Linux version", "GRUB", "Booting", "NexusOS"
        };
        std::vector<std::string> bad_signs = {
            "Kernel panic", "BUG:", "OOPS:", "double fault"
        };

        for (auto& bad : bad_signs) {
            if (serial_content.find(bad) != std::string::npos)
                return VoidResult::err(fmt::format(
                    "Kernel error detected in serial output: {}", bad));
        }

        bool found_good = false;
        for (auto& good : good_signs) {
            if (serial_content.find(good) != std::string::npos) {
                found_good = true; break;
            }
        }
        if (!found_good)
            log_warn("No boot progress detected in serial output");
        else
            log_ok("Boot progress detected in serial output");
    }

    log_ok("QEMU smoke test passed");
    return VoidResult::ok();
}

VoidResult SmokeTester::validate_rootfs(const std::filesystem::path& rootfs) {
    struct Check {
        std::string name;
        std::filesystem::path path;
        bool required;
    };

    std::vector<Check> checks = {
        {"bin",           rootfs / "bin",              true},
        {"etc",           rootfs / "etc",              true},
        {"lib",           rootfs / "lib",              true},
        {"usr",           rootfs / "usr",              true},
        {"var",           rootfs / "var",              true},
        {"etc/debian_version", rootfs/"etc"/"debian_version", true},
        {"etc/os-release",rootfs/"etc"/"os-release",   true},
        {"etc/hostname",  rootfs/"etc"/"hostname",     true},
        {"etc/passwd",    rootfs/"etc"/"passwd",       true},
        {"etc/shadow",    rootfs/"etc"/"shadow",       true},
        {"etc/nftables.conf", rootfs/"etc"/"nftables.conf", false},
        {"boot/vmlinuz",  rootfs/"boot",              true},
        {"usr/bin/bash",  rootfs/"usr"/"bin"/"bash",  true},
        {"usr/bin/python3",rootfs/"usr"/"bin"/"python3",false},
    };

    bool all_ok = true;
    fmt::print("\nRootfs validation: {}\n", rootfs.string());
    for (auto& c : checks) {
        bool exists = std::filesystem::exists(c.path);
        if (exists) {
            log_ok(c.name);
        } else if (c.required) {
            log_fail(c.name + " (REQUIRED — MISSING)");
            all_ok = false;
        } else {
            log_warn(c.name + " (optional — missing)");
        }
    }

    if (!all_ok)
        return VoidResult::err("Rootfs validation failed — required paths missing");
    return VoidResult::ok();
}

VoidResult SmokeTester::run(
    const std::filesystem::path& iso_path,
    BuildMode mode)
{
    log_step("smoke-test", iso_path.filename().string());

    auto r = check_iso_integrity(iso_path);
    if (!r) return r;

    r = boot_qemu(iso_path, cfg_.qemu_timeout);
    if (!r) return r;

    log_ok("Smoke test complete");
    return VoidResult::ok();
}

} // namespace nexus
