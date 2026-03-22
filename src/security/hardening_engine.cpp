#include "hardening_engine.hpp"
#include "utils/logger.hpp"
#include "utils/process.hpp"

#include <fstream>
#include <fmt/core.h>

namespace nexus {

HardeningEngine::HardeningEngine(const NexusConfig& cfg) : cfg_(cfg) {}

// ── sysctl ────────────────────────────────────────────────────────────────────
VoidResult HardeningEngine::write_sysctl_hardened(
    const std::filesystem::path& rootfs)
{
    auto path = rootfs / "etc" / "sysctl.d" / "99-nexus-hardening.conf";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err("Cannot write sysctl config: " + path.string());

    ofs << R"(# NexusOS — Hardened sysctl configuration
# Applied by hardening_engine in hardened mode

# ── Kernel hardening ────────────────────────────────────────────────────────
kernel.dmesg_restrict = 1
kernel.kptr_restrict = 2
kernel.perf_event_paranoid = 3
kernel.randomize_va_space = 2
kernel.yama.ptrace_scope = 2
kernel.unprivileged_bpf_disabled = 1
kernel.sysrq = 0
kernel.core_uses_pid = 1
kernel.core_pattern = |/bin/false
kernel.modules_disabled = 0

# ── Network hardening ────────────────────────────────────────────────────────
net.ipv4.ip_forward = 0
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.secure_redirects = 0
net.ipv4.conf.default.secure_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.conf.all.log_martians = 1
net.ipv4.conf.default.log_martians = 1
net.ipv4.icmp_echo_ignore_broadcasts = 1
net.ipv4.icmp_ignore_bogus_error_responses = 1
net.ipv4.tcp_syncookies = 1
net.ipv4.tcp_rfc1337 = 1
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0
net.ipv6.conf.all.accept_source_route = 0
net.ipv6.conf.default.accept_source_route = 0
net.ipv6.conf.all.accept_ra = 0
net.ipv6.conf.default.accept_ra = 0

# ── File system hardening ────────────────────────────────────────────────────
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
fs.protected_fifos = 2
fs.protected_regular = 2
fs.suid_dumpable = 0

# ── Memory hardening ─────────────────────────────────────────────────────────
vm.mmap_rnd_bits = 32
vm.mmap_rnd_compat_bits = 16
vm.swappiness = 10
)";

    // Apply config-level overrides
    for (auto& [key, val] : cfg_.sysctl_overrides)
        ofs << key << " = " << val << "\n";

    return VoidResult::ok();
}

VoidResult HardeningEngine::write_sysctl_lab(
    const std::filesystem::path& rootfs)
{
    auto path = rootfs / "etc" / "sysctl.d" / "99-nexus-lab.conf";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err("Cannot write sysctl config: " + path.string());

    ofs << R"(# NexusOS — Lab sysctl configuration
# Relaxed for virtualisation and lab environments

kernel.dmesg_restrict = 0
kernel.perf_event_paranoid = 1
kernel.yama.ptrace_scope = 1
net.ipv4.ip_forward = 1
net.ipv4.conf.all.rp_filter = 1
net.ipv4.tcp_syncookies = 1
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
fs.suid_dumpable = 0
vm.swappiness = 10
)";
    return VoidResult::ok();
}

VoidResult HardeningEngine::apply_sysctl(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    auto r = (mode == BuildMode::Hardened)
        ? write_sysctl_hardened(rootfs)
        : write_sysctl_lab(rootfs);
    if (!r) return r;
    log_ok(fmt::format("sysctl configured (mode: {})", to_string(mode)));
    return VoidResult::ok();
}

// ── AppArmor ──────────────────────────────────────────────────────────────────
VoidResult HardeningEngine::configure_apparmor(
    const std::filesystem::path& rootfs)
{
    // Ensure AppArmor is enabled in /etc/default/grub equivalent
    // and that apparmor service is enabled
    auto r = Process::chroot_run(rootfs, "systemctl", {"enable", "apparmor"});
    if (!r.success())
        log_warn("Could not enable apparmor service: " + r.stderr_out);

    // Write a basic AppArmor configuration for NexusOS
    auto aa_conf = rootfs / "etc" / "apparmor" / "parser.conf";
    std::filesystem::create_directories(aa_conf.parent_path());
    std::ofstream ofs(aa_conf);
    ofs << "# NexusOS AppArmor parser configuration\n"
        << "Optimize=compress-fast\n"
        << "allow-complain\n";

    log_ok("AppArmor configured");
    return VoidResult::ok();
}

// ── auditd ────────────────────────────────────────────────────────────────────
VoidResult HardeningEngine::write_audit_rules(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    auto rules_path = rootfs / "etc" / "audit" / "rules.d" / "nexus.rules";
    std::filesystem::create_directories(rules_path.parent_path());
    std::ofstream ofs(rules_path);
    if (!ofs) return VoidResult::err("Cannot write audit rules");

    ofs << R"(## NexusOS audit rules
## Generated by nexus hardening engine

# Delete all existing rules and set buffers
-D
-b 8192
--backlog_wait_time 60000

# Make audit config immutable (hardened mode only)
)" << (mode == BuildMode::Hardened ? "-e 2\n" : "# -e 2 (disabled in lab mode)\n");

    ofs << R"(
# ── Authentication & privilege escalation ─────────────────────────────────
-w /etc/passwd -p wa -k identity
-w /etc/shadow -p wa -k identity
-w /etc/sudoers -p wa -k sudoers
-w /etc/sudoers.d/ -p wa -k sudoers
-w /var/log/auth.log -p wa -k auth_log

# ── Privileged command execution ───────────────────────────────────────────
-a always,exit -F arch=b64 -S execve -F euid=0 -F auid>=1000 -F auid!=-1 -k priv_exec

# ── System administration ──────────────────────────────────────────────────
-w /sbin/insmod -p x -k module_load
-w /sbin/rmmod  -p x -k module_load
-w /sbin/modprobe -p x -k module_load

# ── File access monitoring ─────────────────────────────────────────────────
-w /etc/crontab -p wa -k cron
-w /etc/cron.d/ -p wa -k cron
-w /var/spool/cron/ -p wa -k cron

# ── Network configuration changes ─────────────────────────────────────────
-w /etc/hosts -p wa -k network_config
-w /etc/network/ -p wa -k network_config
-w /etc/NetworkManager/ -p wa -k network_config

# ── Kernel module loading ─────────────────────────────────────────────────
-a always,exit -F arch=b64 -S init_module -S delete_module -k kernel_modules

# ── Mount / unmount operations ────────────────────────────────────────────
-a always,exit -F arch=b64 -S mount -k mounts
-a always,exit -F arch=b64 -S umount2 -k mounts

# ── System calls: process creation ────────────────────────────────────────
-a always,exit -F arch=b64 -S fork -S vfork -S clone -k process_creation

# ── Unauthorized file access ──────────────────────────────────────────────
-a always,exit -F arch=b64 -S open -F exit=-EACCES -k access_denied
-a always,exit -F arch=b64 -S open -F exit=-EPERM  -k access_denied
)";

    return VoidResult::ok();
}

VoidResult HardeningEngine::configure_auditd(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    auto r = write_audit_rules(rootfs, mode);
    if (!r) return r;

    // Configure auditd.conf
    auto auditd_conf = rootfs / "etc" / "audit" / "auditd.conf";
    std::ofstream ofs(auditd_conf);
    ofs << "# NexusOS auditd configuration\n"
        << "log_file = /var/log/audit/audit.log\n"
        << "log_format = ENRICHED\n"
        << "log_group = adm\n"
        << "priority_boost = 4\n"
        << "flush = INCREMENTAL_ASYNC\n"
        << "freq = 50\n"
        << "num_logs = 10\n"
        << "disp_qos = lossy\n"
        << "dispatcher = /sbin/audispd\n"
        << "name_format = HOSTNAME\n"
        << "max_log_file = 32\n"
        << "max_log_file_action = ROTATE\n"
        << "space_left = 100\n"
        << "space_left_action = SYSLOG\n"
        << "action_mail_acct = root\n"
        << "admin_space_left = 50\n"
        << "admin_space_left_action = SUSPEND\n"
        << "disk_full_action = SUSPEND\n"
        << "disk_error_action = SUSPEND\n";

    // Enable auditd
    Process::chroot_run(rootfs, "systemctl", {"enable", "auditd"});

    log_ok(fmt::format("auditd configured (mode: {})", to_string(mode)));
    return VoidResult::ok();
}

// ── nftables ──────────────────────────────────────────────────────────────────
VoidResult HardeningEngine::write_nftables_hardened(
    const std::filesystem::path& rootfs)
{
    auto path = rootfs / "etc" / "nftables.conf";
    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err("Cannot write nftables.conf");

    ofs << R"(#!/usr/sbin/nft -f
# NexusOS nftables — hardened mode
# Stateful firewall, deny by default

flush ruleset

table inet nexus_filter {
    chain input {
        type filter hook input priority 0; policy drop;

        # Allow established/related
        ct state established,related accept

        # Allow loopback
        iif "lo" accept

        # Allow ICMPv4 (limited)
        ip protocol icmp icmp type {
            echo-reply, destination-unreachable, echo-request,
            time-exceeded, parameter-problem
        } accept

        # Allow ICMPv6 (necessary for IPv6 operation)
        ip6 nexthdr icmpv6 accept

        # Drop invalid
        ct state invalid drop

        # Log and drop everything else
        limit rate 5/minute burst 10 packets log prefix "[nexus-drop-in] " flags all
    }

    chain forward {
        type filter hook forward priority 0; policy drop;
    }

    chain output {
        type filter hook output priority 0; policy accept;

        # Prevent root from sending to external — comment out if not needed
        # skuid root ip daddr != { 127.0.0.0/8, ::1 } drop
    }
}
)";
    return VoidResult::ok();
}

VoidResult HardeningEngine::write_nftables_lab(
    const std::filesystem::path& rootfs)
{
    auto path = rootfs / "etc" / "nftables.conf";
    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err("Cannot write nftables.conf");

    ofs << R"(#!/usr/sbin/nft -f
# NexusOS nftables — lab mode
# More permissive: allows forwarding for virtualisation

flush ruleset

table inet nexus_filter {
    chain input {
        type filter hook input priority 0; policy accept;

        # Allow established/related
        ct state established,related accept

        # Allow loopback
        iif "lo" accept

        # Drop invalid
        ct state invalid drop
    }

    chain forward {
        type filter hook forward priority 0; policy accept;
    }

    chain output {
        type filter hook output priority 0; policy accept;
    }
}
)";
    return VoidResult::ok();
}

VoidResult HardeningEngine::configure_nftables(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    auto r = (mode == BuildMode::Hardened)
        ? write_nftables_hardened(rootfs)
        : write_nftables_lab(rootfs);
    if (!r) return r;

    Process::chroot_run(rootfs, "systemctl", {"enable", "nftables"});
    log_ok(fmt::format("nftables configured (mode: {})", to_string(mode)));
    return VoidResult::ok();
}

// ── systemd hardening ─────────────────────────────────────────────────────────
VoidResult HardeningEngine::harden_systemd(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    if (mode != BuildMode::Hardened) return VoidResult::ok();

    auto systemd_conf = rootfs / "etc" / "systemd" / "system.conf";
    std::ofstream ofs(systemd_conf, std::ios::app);
    ofs << "\n# NexusOS hardening additions\n"
        << "DefaultLimitCORE=0\n"
        << "DefaultLimitNOFILE=1024\n"
        << "DumpCore=no\n"
        << "CrashReboot=no\n";

    // Disable unnecessary services
    static const std::vector<std::string> disabled = {
        "apt-daily.timer", "apt-daily-upgrade.timer",
        "ModemManager.service", "pppd-dns.service"
    };
    for (auto& svc : disabled)
        Process::chroot_run(rootfs, "systemctl", {"disable", "--now", svc});

    log_ok("systemd hardened");
    return VoidResult::ok();
}

VoidResult HardeningEngine::configure_sshd(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    auto sshd_config = rootfs / "etc" / "ssh" / "sshd_config.d" / "nexus.conf";
    std::filesystem::create_directories(sshd_config.parent_path());
    std::ofstream ofs(sshd_config);
    if (!ofs) return VoidResult::ok(); // SSH may not be installed

    ofs << "# NexusOS SSH hardening\n"
        << "Protocol 2\n"
        << "PermitRootLogin no\n"
        << "PasswordAuthentication "
        << (mode == BuildMode::Hardened ? "no" : "yes") << "\n"
        << "ChallengeResponseAuthentication no\n"
        << "X11Forwarding no\n"
        << "AllowAgentForwarding no\n"
        << "PrintMotd yes\n"
        << "MaxAuthTries 3\n"
        << "LoginGraceTime 30\n"
        << "ClientAliveInterval 300\n"
        << "ClientAliveCountMax 2\n"
        << "Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com\n"
        << "MACs hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com\n"
        << "KexAlgorithms curve25519-sha256,curve25519-sha256@libssh.org\n";

    return VoidResult::ok();
}

VoidResult HardeningEngine::disable_root_login(
    const std::filesystem::path& rootfs)
{
    // Lock root account
    auto r = Process::chroot_run(rootfs, "passwd", {"--lock", "root"});
    if (!r.success())
        log_warn("Could not lock root password: " + r.stderr_out);

    // Disable root in /etc/securetty (empty file = no tty for root)
    auto securetty = rootfs / "etc" / "securetty";
    std::ofstream ofs(securetty);
    ofs << "# NexusOS: root console login disabled\n";

    return VoidResult::ok();
}

VoidResult HardeningEngine::configure_pam_login(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    // Set umask in /etc/login.defs
    auto login_defs = rootfs / "etc" / "login.defs";
    auto r = Process::shell(
        fmt::format("sed -i 's/^UMASK.*/UMASK\\t\\t027/' {}",
            login_defs.string()));
    if (mode == BuildMode::Hardened) {
        Process::shell(fmt::format(
            "sed -i 's/^PASS_MAX_DAYS.*/PASS_MAX_DAYS\\t90/' {}", login_defs.string()));
        Process::shell(fmt::format(
            "sed -i 's/^PASS_MIN_DAYS.*/PASS_MIN_DAYS\\t1/' {}", login_defs.string()));
    }
    return VoidResult::ok();
}

VoidResult HardeningEngine::apply_all(
    const std::filesystem::path& rootfs,
    BuildMode mode)
{
    auto r = apply_sysctl(rootfs, mode);           if (!r) return r;
    r = configure_apparmor(rootfs);                if (!r) return r;
    r = configure_auditd(rootfs, mode);            if (!r) return r;
    r = configure_nftables(rootfs, mode);          if (!r) return r;
    r = harden_systemd(rootfs, mode);              if (!r) return r;
    r = configure_pam_login(rootfs, mode);         if (!r) return r;
    if (mode == BuildMode::Hardened) {
        r = disable_root_login(rootfs);            if (!r) return r;
    }
    r = configure_sshd(rootfs, mode);             if (!r) return r;
    return VoidResult::ok();
}

} // namespace nexus
