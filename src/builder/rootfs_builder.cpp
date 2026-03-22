#include "rootfs_builder.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"

#include <fstream>
#include <fmt/core.h>

namespace nexus {

RootfsBuilder::RootfsBuilder(const NexusConfig& cfg, PackageManager& pkg_mgr)
    : cfg_(cfg), pkg_mgr_(pkg_mgr)
{}

VoidResult RootfsBuilder::debootstrap(
    const std::filesystem::path& rootfs,
    const std::string& mirror,
    const std::string& suite,
    const std::string& arch)
{
    // If rootfs already has a valid debootstrap marker, skip
    auto marker = rootfs / "etc" / "debian_version";
    if (std::filesystem::exists(marker)) {
        log_warn("Rootfs already exists — skipping debootstrap (use 'clean' to rebuild)");
        return VoidResult::ok();
    }

    std::filesystem::create_directories(rootfs);
    log_info(fmt::format("debootstrap {} {} {}", suite, arch, mirror));

    ProcessOptions opts;
    opts.echo_cmd = true;
    opts.stdout_callback = [](std::string_view line) { log_debug(line); };

    auto r = Process::run("debootstrap", {
        "--arch=" + arch,
        "--variant=minbase",
        "--include=ca-certificates,apt-transport-https,gnupg2,lsb-release,"
                  "systemd,systemd-sysv,dbus,locales,tzdata",
        suite,
        rootfs.string(),
        mirror
    });

    if (!r.success())
        return VoidResult::err(fmt::format("debootstrap failed (exit {}): {}",
            r.exit_code, r.stderr_out));

    log_ok("debootstrap complete");
    return VoidResult::ok();
}

VoidResult RootfsBuilder::write_apt_sources(
    const std::filesystem::path& rootfs,
    const std::string& mirror,
    const std::string& suite)
{
    auto sources_path = rootfs / "etc" / "apt" / "sources.list";
    std::ofstream ofs(sources_path);
    if (!ofs) return VoidResult::err(
        fmt::format("Cannot write: {}", sources_path.string()));

    ofs << fmt::format(
        "deb {} {} main contrib non-free non-free-firmware\n"
        "deb {} {}-updates main contrib non-free non-free-firmware\n"
        "deb {} {}-security main contrib non-free non-free-firmware\n",
        mirror, suite,
        mirror, suite,
        "http://security.debian.org/debian-security", suite);

    // Extra repos from config
    for (auto& repo : cfg_.extra_repos)
        ofs << repo << "\n";

    return VoidResult::ok();
}

VoidResult RootfsBuilder::configure_locale(const std::filesystem::path& rootfs) {
    // Write locale.gen
    auto locale_gen = rootfs / "etc" / "locale.gen";
    std::ofstream ofs(locale_gen, std::ios::app);
    ofs << "en_US.UTF-8 UTF-8\nes_ES.UTF-8 UTF-8\n";
    ofs.close();

    // Set default locale
    auto locale_conf = rootfs / "etc" / "locale.conf";
    std::ofstream lc(locale_conf);
    lc << "LANG=en_US.UTF-8\nLC_ALL=en_US.UTF-8\n";

    auto r = Process::chroot_run(rootfs, "locale-gen", {});
    if (!r.success())
        return VoidResult::err("locale-gen failed: " + r.stderr_out);
    return VoidResult::ok();
}

VoidResult RootfsBuilder::configure_timezone(const std::filesystem::path& rootfs) {
    // Set UTC timezone
    auto tz_path = rootfs / "etc" / "timezone";
    std::ofstream ofs(tz_path);
    ofs << "UTC\n";

    auto localtime = rootfs / "etc" / "localtime";
    std::error_code ec;
    std::filesystem::remove(localtime, ec);
    std::filesystem::create_symlink(
        "/usr/share/zoneinfo/UTC", localtime, ec);
    return VoidResult::ok();
}

VoidResult RootfsBuilder::configure_hostname(const std::filesystem::path& rootfs) {
    auto hostname_path = rootfs / "etc" / "hostname";
    std::ofstream ofs(hostname_path);
    ofs << "nexus\n";

    auto hosts_path = rootfs / "etc" / "hosts";
    std::ofstream hosts(hosts_path);
    hosts << "127.0.0.1  localhost\n"
          << "127.0.1.1  nexus\n"
          << "::1        localhost ip6-localhost ip6-loopback\n"
          << "ff02::1    ip6-allnodes\n"
          << "ff02::2    ip6-allrouters\n";
    return VoidResult::ok();
}

VoidResult RootfsBuilder::configure_fstab(const std::filesystem::path& rootfs) {
    auto fstab = rootfs / "etc" / "fstab";
    std::ofstream ofs(fstab);
    ofs << "# NexusOS fstab — live system\n"
        << "# <fs>  <mountpoint>  <type>  <options>  <dump>  <pass>\n"
        << "tmpfs  /tmp   tmpfs  defaults,nodev,nosuid,noexec,size=256m  0  0\n"
        << "tmpfs  /run   tmpfs  defaults,nodev,nosuid,size=64m  0  0\n";
    return VoidResult::ok();
}

VoidResult RootfsBuilder::configure_network(const std::filesystem::path& rootfs) {
    // NetworkManager is preferred for live systems
    auto nm_dir = rootfs / "etc" / "NetworkManager" / "conf.d";
    std::filesystem::create_directories(nm_dir);

    std::ofstream ofs(nm_dir / "nexus.conf");
    ofs << "[main]\n"
        << "plugins=ifupdown,keyfile\n"
        << "dns=default\n\n"
        << "[ifupdown]\n"
        << "managed=false\n\n"
        << "[device]\n"
        << "wifi.backend=wpa_supplicant\n";
    return VoidResult::ok();
}

VoidResult RootfsBuilder::configure_base(
    const std::filesystem::path& rootfs,
    const std::string& mirror,
    const std::string& suite)
{
    auto r = write_apt_sources(rootfs, mirror, suite);
    if (!r) return r;

    // Update package lists
    auto upd = Process::chroot_run(rootfs, "apt-get",
        {"update", "-qq"});
    if (!upd.success())
        return VoidResult::err("apt-get update failed: " + upd.stderr_out);

    r = configure_locale(rootfs);   if (!r) return r;
    r = configure_timezone(rootfs); if (!r) return r;
    r = configure_hostname(rootfs); if (!r) return r;
    r = configure_fstab(rootfs);    if (!r) return r;
    r = configure_network(rootfs);  if (!r) return r;

    // Prevent services starting during build
    auto policy_path = rootfs / "usr" / "sbin" / "policy-rc.d";
    std::ofstream policy(policy_path);
    policy << "#!/bin/sh\nexit 101\n";
    std::filesystem::permissions(policy_path,
        std::filesystem::perms::owner_exec |
        std::filesystem::perms::owner_read  |
        std::filesystem::perms::group_read  |
        std::filesystem::perms::others_read,
        std::filesystem::perm_options::add);

    return VoidResult::ok();
}

VoidResult RootfsBuilder::apply_overlays(
    const std::filesystem::path& rootfs,
    const std::vector<std::string>& overlays,
    const std::filesystem::path& overlays_dir)
{
    for (auto& overlay_name : overlays) {
        auto overlay_path = overlays_dir / overlay_name;
        if (!std::filesystem::exists(overlay_path)) {
            log_warn(fmt::format("Overlay not found, skipping: {}", overlay_name));
            continue;
        }
        log_info(fmt::format("Applying overlay: {}", overlay_name));
        auto r = Process::run("rsync", {
            "-a", "--chown=root:root",
            overlay_path.string() + "/",
            rootfs.string() + "/"
        });
        if (!r.success())
            return VoidResult::err(fmt::format("rsync overlay {}: {}",
                overlay_name, r.stderr_out));
        log_ok(fmt::format("Overlay applied: {}", overlay_name));
    }
    return VoidResult::ok();
}

VoidResult RootfsBuilder::setup_user(const std::filesystem::path& rootfs) {
    // Create 'nexus' user
    auto r = Process::chroot_run(rootfs, "useradd", {
        "--create-home",
        "--shell", "/bin/bash",
        "--groups", "sudo,audio,video,plugdev,netdev,cdrom,floppy",
        "nexus"
    });
    // User may already exist — ignore that error
    if (!r.success() && r.stderr_out.find("already exists") == std::string::npos)
        return VoidResult::err("useradd failed: " + r.stderr_out);

    // Set password to 'nexus' (PBKDF2 in /etc/shadow via chpasswd)
    auto pw = Process::shell(
        fmt::format("echo 'nexus:nexus' | chroot {} /usr/sbin/chpasswd",
            rootfs.string()));
    if (!pw.success())
        log_warn("chpasswd failed — user may have no password");

    // Write sudoers entry for nexus user
    auto sudoers_path = rootfs / "etc" / "sudoers.d" / "nexus-user";
    std::filesystem::create_directories(sudoers_path.parent_path());
    std::ofstream ofs(sudoers_path);
    ofs << "# NexusOS — default user sudo access\n"
        << "nexus ALL=(ALL:ALL) NOPASSWD: ALL\n";
    std::filesystem::permissions(sudoers_path,
        std::filesystem::perms::owner_read,
        std::filesystem::perm_options::replace);

    log_ok("User 'nexus' created (password: nexus)");
    return VoidResult::ok();
}

VoidResult RootfsBuilder::generate_initramfs(const std::filesystem::path& rootfs) {
    // Find installed kernel version
    auto r = Process::chroot_run(rootfs, "/bin/ls",
        {"/lib/modules/"});
    if (!r.success() || r.stdout_out.empty())
        return VoidResult::err("No kernel modules found in rootfs");

    std::string kver = r.stdout_out;
    while (!kver.empty() && std::isspace(kver.back())) kver.pop_back();

    log_info(fmt::format("Generating initramfs for kernel: {}", kver));

    auto gen = Process::chroot_run(rootfs, "update-initramfs",
        {"-c", "-k", kver});
    if (!gen.success())
        return VoidResult::err("update-initramfs failed: " + gen.stderr_out);

    log_ok(fmt::format("initramfs generated for {}", kver));
    return VoidResult::ok();
}

VoidResult RootfsBuilder::disable_services(const std::filesystem::path& rootfs) {
    static const std::vector<std::string> disabled = {
        "bluetooth.service",
        "cups.service",
        "ModemManager.service",
        "avahi-daemon.service",
    };

    for (auto& svc : disabled) {
        Process::chroot_run(rootfs, "systemctl", {"disable", svc});
    }
    return VoidResult::ok();
}

VoidResult RootfsBuilder::run_hook(
    const std::filesystem::path& rootfs,
    const std::filesystem::path& script)
{
    if (!std::filesystem::exists(script)) {
        log_warn(fmt::format("Hook script not found: {}", script.string()));
        return VoidResult::ok();
    }
    log_info(fmt::format("Running hook: {}", script.filename().string()));
    auto r = Process::chroot_script(rootfs, script);
    if (!r.success())
        return VoidResult::err(fmt::format("Hook {} failed: {}",
            script.filename().string(), r.stderr_out));
    return VoidResult::ok();
}

VoidResult RootfsBuilder::export_kernel(
    const std::filesystem::path& rootfs,
    const std::filesystem::path& dest_dir)
{
    std::filesystem::create_directories(dest_dir);
    // Copy vmlinuz and initrd from rootfs/boot/
    for (auto& entry : std::filesystem::directory_iterator(rootfs / "boot")) {
        auto name = entry.path().filename().string();
        if (name.starts_with("vmlinuz-") || name.starts_with("initrd.img-")) {
            std::filesystem::copy_file(entry.path(), dest_dir / name,
                std::filesystem::copy_options::overwrite_existing);
        }
    }
    // Create stable symlinks
    for (auto& entry : std::filesystem::directory_iterator(dest_dir)) {
        auto name = entry.path().filename().string();
        if (name.starts_with("vmlinuz-")) {
            std::error_code ec;
            std::filesystem::create_symlink(name, dest_dir / "vmlinuz", ec);
        }
        if (name.starts_with("initrd.img-")) {
            std::error_code ec;
            std::filesystem::create_symlink(name, dest_dir / "initrd.img", ec);
        }
    }
    return VoidResult::ok();
}

} // namespace nexus
