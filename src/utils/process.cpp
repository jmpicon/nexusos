#include "process.hpp"
#include "logger.hpp"

#include <cstring>
#include <cerrno>
#include <array>
#include <stdexcept>
#include <sstream>
#include <algorithm>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <fmt/core.h>

namespace nexus {

namespace {

// Build environment for child process
std::vector<std::string> build_env(const ProcessOptions& opts) {
    std::vector<std::string> env_vec;

    if (opts.inherit_env) {
        for (char** e = environ; e && *e; ++e)
            env_vec.emplace_back(*e);
    }

    // Apply overrides / additions
    for (auto& [k, v] : opts.env) {
        std::string entry = k + "=" + v;
        // Replace if key exists
        auto it = std::find_if(env_vec.begin(), env_vec.end(),
            [&k](const std::string& e){ return e.substr(0, k.size()+1) == k+"="; });
        if (it != env_vec.end())
            *it = entry;
        else
            env_vec.push_back(entry);
    }
    return env_vec;
}

} // anonymous namespace

ProcessResult Process::exec_impl(const std::vector<std::string>& argv,
                                 const ProcessOptions& opts)
{
    if (argv.empty()) return {-1, "", "Empty command"};

    if (opts.echo_cmd) {
        std::string cmd_str;
        for (auto& a : argv) cmd_str += a + " ";
        log_debug(fmt::format("exec: {}", cmd_str));
    }

    // Prepare argv for execvp
    std::vector<const char*> c_argv;
    c_argv.reserve(argv.size() + 1);
    for (auto& s : argv) c_argv.push_back(s.c_str());
    c_argv.push_back(nullptr);

    // Prepare envp
    auto env_vec = build_env(opts);
    std::vector<const char*> c_env;
    c_env.reserve(env_vec.size() + 1);
    for (auto& s : env_vec) c_env.push_back(s.c_str());
    c_env.push_back(nullptr);

    // Pipes: parent reads child stdout/stderr
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};

    if (opts.capture_stdout && pipe(stdout_pipe) < 0) return {-1, "", strerror(errno)};
    if (opts.capture_stderr && pipe(stderr_pipe) < 0) return {-1, "", strerror(errno)};

    pid_t pid = fork();
    if (pid < 0) return {-1, "", strerror(errno)};

    if (pid == 0) {
        // ── Child ──────────────────────────────────────────────────────────
        if (!opts.working_dir.empty()) {
            if (chdir(opts.working_dir.c_str()) < 0) _exit(127);
        }

        if (opts.capture_stdout) {
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[0]); close(stdout_pipe[1]);
        }
        if (opts.capture_stderr) {
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[0]); close(stderr_pipe[1]);
        }

        execvpe(c_argv[0],
                const_cast<char* const*>(c_argv.data()),
                const_cast<char* const*>(c_env.data()));
        _exit(127); // exec failed
    }

    // ── Parent ────────────────────────────────────────────────────────────────
    if (opts.capture_stdout) close(stdout_pipe[1]);
    if (opts.capture_stderr) close(stderr_pipe[1]);

    ProcessResult result;
    auto read_pipe = [](int fd, std::string& out,
                        const std::function<void(std::string_view)>& cb) {
        std::array<char, 4096> buf;
        ssize_t n;
        std::string line_buf;
        while ((n = read(fd, buf.data(), buf.size())) > 0) {
            out.append(buf.data(), n);
            if (cb) {
                line_buf.append(buf.data(), n);
                size_t pos;
                while ((pos = line_buf.find('\n')) != std::string::npos) {
                    cb(std::string_view(line_buf.data(), pos));
                    line_buf.erase(0, pos + 1);
                }
            }
        }
        close(fd);
    };

    // Read in separate flow (simple sequential read — sufficient for CLI tool)
    if (opts.capture_stdout)
        read_pipe(stdout_pipe[0], result.stdout_out, opts.stdout_callback);
    if (opts.capture_stderr)
        read_pipe(stderr_pipe[0], result.stderr_out, nullptr);

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status))
        result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        result.exit_code = 128 + WTERMSIG(status);

    return result;
}

ProcessResult Process::run(std::string_view executable,
                           const std::vector<std::string>& args,
                           const ProcessOptions& opts)
{
    std::vector<std::string> argv;
    argv.push_back(std::string(executable));
    argv.insert(argv.end(), args.begin(), args.end());
    return exec_impl(argv, opts);
}

ProcessResult Process::shell(std::string_view cmd, const ProcessOptions& opts) {
    return exec_impl({"/bin/sh", "-c", std::string(cmd)}, opts);
}

ProcessResult Process::chroot_run(const std::filesystem::path& chroot_dir,
                                  std::string_view executable,
                                  const std::vector<std::string>& args,
                                  const ProcessOptions& opts)
{
    std::vector<std::string> argv = {
        "/usr/sbin/chroot",
        chroot_dir.string(),
        std::string(executable)
    };
    argv.insert(argv.end(), args.begin(), args.end());
    return exec_impl(argv, opts);
}

ProcessResult Process::chroot_script(const std::filesystem::path& chroot_dir,
                                     const std::filesystem::path& script_path,
                                     const std::vector<std::string>& args,
                                     const ProcessOptions& opts)
{
    // Copy script into chroot, execute, remove
    auto chroot_script_path = chroot_dir / "tmp" / script_path.filename();
    std::filesystem::copy_file(script_path, chroot_script_path,
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(chroot_script_path,
        std::filesystem::perms::owner_exec |
        std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write,
        std::filesystem::perm_options::add);

    auto r = chroot_run(chroot_dir,
        "/tmp/" + script_path.filename().string(),
        args, opts);

    std::error_code ec;
    std::filesystem::remove(chroot_script_path, ec);
    return r;
}

bool Process::which(std::string_view name) {
    auto r = shell(fmt::format("which {} >/dev/null 2>&1", name));
    return r.exit_code == 0;
}

std::optional<std::filesystem::path> Process::find_binary(std::string_view name) {
    auto r = shell(fmt::format("which {}", name));
    if (r.exit_code != 0) return std::nullopt;
    // strip trailing newline
    auto p = r.stdout_out;
    while (!p.empty() && (p.back() == '\n' || p.back() == '\r')) p.pop_back();
    return std::filesystem::path(p);
}

VoidResult Process::bind_mount(const std::filesystem::path& src,
                               const std::filesystem::path& dst,
                               bool read_only)
{
    std::vector<std::string> args = {"--bind", src.string(), dst.string()};
    auto r = run("mount", args);
    if (!r.success())
        return VoidResult::err(fmt::format("bind_mount({} -> {}) failed: {}",
            src.string(), dst.string(), r.stderr_out));
    if (read_only) {
        auto r2 = run("mount", {"--bind", "-o", "remount,ro", dst.string()});
        if (!r2.success())
            return VoidResult::err(fmt::format("remount ro {}: {}", dst.string(), r2.stderr_out));
    }
    return VoidResult::ok();
}

VoidResult Process::umount(const std::filesystem::path& dst, bool lazy) {
    std::vector<std::string> args;
    if (lazy) args.push_back("-l");
    args.push_back(dst.string());
    auto r = run("umount", args);
    if (!r.success())
        return VoidResult::err(fmt::format("umount {}: {}", dst.string(), r.stderr_out));
    return VoidResult::ok();
}

VoidResult Process::mount_chroot_vfs(const std::filesystem::path& chroot) {
    struct Mount { std::string type; std::string src; std::string dst; };
    std::vector<Mount> mounts = {
        {"proc",  "proc",     (chroot/"proc").string()},
        {"sysfs", "sys",      (chroot/"sys").string()},
        {"devtmpfs","dev",    (chroot/"dev").string()},
        {"devpts","devpts",   (chroot/"dev/pts").string()},
    };

    for (auto& m : mounts) {
        std::filesystem::create_directories(m.dst);
        auto r = run("mount", {"-t", m.type, m.src, m.dst});
        if (!r.success())
            return VoidResult::err(fmt::format("mount {} -> {}: {}", m.src, m.dst, r.stderr_out));
    }
    return VoidResult::ok();
}

VoidResult Process::umount_chroot_vfs(const std::filesystem::path& chroot) {
    std::vector<std::string> points = {
        (chroot/"dev/pts").string(),
        (chroot/"dev").string(),
        (chroot/"sys").string(),
        (chroot/"proc").string(),
    };
    for (auto& p : points) {
        // lazy umount — best effort, ignore result
        (void)run("umount", {"-l", p});
    }
    return VoidResult::ok();
}

} // namespace nexus
