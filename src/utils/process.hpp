#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <optional>
#include <functional>
#include <map>
#include "nexus/common.hpp"

namespace nexus {

// ── ProcessResult ─────────────────────────────────────────────────────────────
struct ProcessResult {
    int         exit_code   {-1};
    std::string stdout_out  {};
    std::string stderr_out  {};

    [[nodiscard]] bool success() const noexcept { return exit_code == 0; }
    [[nodiscard]] std::string combined() const { return stdout_out + stderr_out; }
};

// ── ProcessOptions ─────────────────────────────────────────────────────────────
struct ProcessOptions {
    std::filesystem::path                   working_dir     {};
    std::map<std::string, std::string>      env             {};
    bool                                    capture_stdout  {true};
    bool                                    capture_stderr  {true};
    bool                                    inherit_env     {true};
    bool                                    echo_cmd        {false};
    int                                     timeout_secs    {0};   // 0 = no limit
    std::function<void(std::string_view)>   stdout_callback {};    // line-by-line
};

// ── Process — static utility class ────────────────────────────────────────────
class Process {
public:
    Process() = delete;

    // Run a command with args
    [[nodiscard]] static ProcessResult run(
        std::string_view               executable,
        const std::vector<std::string>& args = {},
        const ProcessOptions&          opts = {});

    // Run a shell command string (via /bin/sh -c)
    [[nodiscard]] static ProcessResult shell(
        std::string_view        cmd,
        const ProcessOptions&   opts = {});

    // Run a command inside a chroot
    [[nodiscard]] static ProcessResult chroot_run(
        const std::filesystem::path&    chroot_dir,
        std::string_view                executable,
        const std::vector<std::string>& args = {},
        const ProcessOptions&           opts = {});

    // Run a script inside a chroot
    [[nodiscard]] static ProcessResult chroot_script(
        const std::filesystem::path&    chroot_dir,
        const std::filesystem::path&    script_path,
        const std::vector<std::string>& args = {},
        const ProcessOptions&           opts = {});

    // Check if a binary is available on PATH
    [[nodiscard]] static bool which(std::string_view name);

    // Find full path to binary
    [[nodiscard]] static std::optional<std::filesystem::path>
        find_binary(std::string_view name);

    // Mount / umount helpers used by builder
    [[nodiscard]] static VoidResult bind_mount(
        const std::filesystem::path& src,
        const std::filesystem::path& dst,
        bool read_only = false);

    [[nodiscard]] static VoidResult umount(
        const std::filesystem::path& dst,
        bool lazy = false);

    // Ensure /proc /sys /dev are mounted inside chroot
    [[nodiscard]] static VoidResult mount_chroot_vfs(
        const std::filesystem::path& chroot_dir);

    [[nodiscard]] static VoidResult umount_chroot_vfs(
        const std::filesystem::path& chroot_dir);

private:
    [[nodiscard]] static ProcessResult exec_impl(
        const std::vector<std::string>& argv,
        const ProcessOptions&           opts);
};

} // namespace nexus
