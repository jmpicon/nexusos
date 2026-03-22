#include "branding_manager.hpp"
#include "utils/logger.hpp"
#include "nexus/version_gen.hpp"

#include <fstream>
#include <fmt/core.h>

namespace nexus {

BrandingManager::BrandingManager(const NexusConfig& cfg) : cfg_(cfg) {}

VoidResult BrandingManager::write_motd(
    const std::filesystem::path& rootfs,
    const std::string& profile)
{
    auto motd_path = rootfs / "etc" / "motd";
    std::ofstream ofs(motd_path);
    if (!ofs) return VoidResult::err("Cannot write /etc/motd");

    ofs << fmt::format(R"(
  ███╗   ██╗███████╗██╗  ██╗██╗   ██╗███████╗
  ████╗  ██║██╔════╝╚██╗██╔╝██║   ██║██╔════╝
  ██╔██╗ ██║█████╗   ╚███╔╝ ██║   ██║███████╗
  ██║╚██╗██║██╔══╝   ██╔██╗ ██║   ██║╚════██║
  ██║ ╚████║███████╗██╔╝ ██╗╚██████╔╝███████║
  ╚═╝  ╚═══╝╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝

  NexusOS {} "{}" — {} profile
  Based on {}
  For authorized use only.

  Documentation: /usr/share/nexus/docs/
  Tools index:   nexus-tools
  Quick help:    nexus-help

)", VERSION_STRING, DISTRO_CODENAME, profile, BASE_DISTRO);

    return VoidResult::ok();
}

VoidResult BrandingManager::write_issue(const std::filesystem::path& rootfs) {
    for (auto& name : {"issue", "issue.net"}) {
        std::ofstream ofs(rootfs / "etc" / name);
        ofs << "NexusOS " << VERSION_STRING << " \\n \\l\n"
            << "For authorized cybersecurity use only.\n";
    }
    return VoidResult::ok();
}

VoidResult BrandingManager::write_os_release(const std::filesystem::path& rootfs) {
    auto path = rootfs / "etc" / "os-release";
    std::ofstream ofs(path);
    if (!ofs) return VoidResult::err("Cannot write /etc/os-release");

    ofs << fmt::format(
        R"(NAME="NexusOS"
VERSION="{0} ({1})"
ID=nexusos
ID_LIKE=debian
PRETTY_NAME="NexusOS {0} {1}"
VERSION_ID="{0}"
HOME_URL="https://github.com/nexusos/nexus"
SUPPORT_URL="https://github.com/nexusos/nexus/issues"
BUG_REPORT_URL="https://github.com/nexusos/nexus/issues"
ANSI_COLOR="1;36"
)",
        VERSION_STRING, DISTRO_CODENAME);

    // Also write lsb_release
    auto lsb = rootfs / "etc" / "lsb-release";
    std::ofstream lfs(lsb);
    lfs << fmt::format(
        "DISTRIB_ID=NexusOS\n"
        "DISTRIB_RELEASE={}\n"
        "DISTRIB_CODENAME={}\n"
        "DISTRIB_DESCRIPTION=\"NexusOS {} {}\"\n",
        VERSION_STRING, DISTRO_CODENAME,
        VERSION_STRING, DISTRO_CODENAME);

    return VoidResult::ok();
}

VoidResult BrandingManager::install_wallpaper(const std::filesystem::path& rootfs) {
    auto wp_dir = rootfs / "usr" / "share" / "nexus" / "wallpapers";
    std::filesystem::create_directories(wp_dir);

    // Copy wallpapers from assets if they exist
    auto assets_wp = cfg_.assets_dir / "wallpapers";
    if (std::filesystem::exists(assets_wp)) {
        for (auto& e : std::filesystem::directory_iterator(assets_wp)) {
            std::error_code ec;
            std::filesystem::copy_file(e.path(), wp_dir / e.path().filename(), ec);
        }
    }
    return VoidResult::ok();
}

VoidResult BrandingManager::write_bash_profile(const std::filesystem::path& rootfs) {
    // Write a useful shell profile for the nexus user
    auto profile_d = rootfs / "etc" / "profile.d";
    std::filesystem::create_directories(profile_d);

    // Main NexusOS profile
    std::ofstream env(profile_d / "nexus-env.sh");
    env << R"(#!/bin/sh
# NexusOS environment setup

export NEXUS_HOME="/usr/share/nexus"
export NEXUS_TOOLS="/usr/share/nexus/tools"
export PATH="$PATH:/usr/share/nexus/bin"

# Secure umask
umask 027

# Terminal color support
export TERM="${TERM:-xterm-256color}"
)";

    // Aliases
    std::ofstream aliases(profile_d / "nexus-aliases.sh");
    aliases << R"(#!/bin/sh
# NexusOS useful aliases for security work

# ── General ────────────────────────────────────────────────────────────────
alias ll='ls -lah --color=auto'
alias la='ls -lA --color=auto'
alias l='ls -CF --color=auto'
alias grep='grep --color=auto'
alias diff='diff --color=auto'
alias ip='ip --color=auto'
alias ..='cd ..'
alias ...='cd ../..'

# ── Network ────────────────────────────────────────────────────────────────
alias myip='ip route get 1.1.1.1 | grep uid | awk "{print \$7}"'
alias ports='ss -tulpn'
alias netstat='ss -tupan'
alias scan='nmap -sV --script=default'
alias quickscan='nmap -T4 -F'

# ── System ─────────────────────────────────────────────────────────────────
alias mem='free -h'
alias disk='df -h'
alias top='btop'
alias ps='ps auxf'
alias open-files='lsof -n'
alias syslog='journalctl -f'
alias auditlog='journalctl -u auditd -f'

# ── DFIR / Forensic ────────────────────────────────────────────────────────
alias fhash='sha256sum'
alias strings-safe='strings -n 6'
alias hexdump-safe='xxd'
alias file-info='file -b'

# ── Tooling shortcuts ──────────────────────────────────────────────────────
alias r2='radare2'
alias vol3='python3 -m volatility3'
alias yarascan='yara -r'
alias tshark-quick='tshark -i any -T fields -e ip.src -e ip.dst -e tcp.port'
alias cap='tcpdump -i any -w /tmp/capture-$(date +%Y%m%d-%H%M%S).pcap'

# ── Safety ─────────────────────────────────────────────────────────────────
alias rm='rm -i'
alias cp='cp -i'
alias mv='mv -i'
)";

    return VoidResult::ok();
}

VoidResult BrandingManager::apply(
    const std::filesystem::path& rootfs,
    const std::string& profile)
{
    auto r = write_motd(rootfs, profile);        if (!r) return r;
    r = write_issue(rootfs);                     if (!r) return r;
    r = write_os_release(rootfs);                if (!r) return r;
    r = install_wallpaper(rootfs);               if (!r) return r;
    r = write_bash_profile(rootfs);              if (!r) return r;

    // Create NexusOS docs directory
    auto docs_dir = rootfs / "usr" / "share" / "nexus" / "docs";
    std::filesystem::create_directories(docs_dir);

    // Write quick tools index
    auto tools_idx = docs_dir / "tools-index.txt";
    std::ofstream ofs(tools_idx);
    ofs << "NexusOS Tools Index\n"
        << "===================\n\n"
        << "Network & Recon:  nmap, tshark, wireshark, tcpdump, arp-scan\n"
        << "DFIR / Forensics: sleuthkit, volatility3, yara, binwalk, foremost\n"
        << "Reversing:        ghidra, radare2, gdb, strace, ltrace\n"
        << "Auditing:         lynis, osquery, aide, auditd\n"
        << "Cloud / Containers: trivy, grype, syft, kubectl, helm\n"
        << "Monitoring:       zeek, suricata, btop, htop\n"
        << "OSINT:            recon-ng, maltego-casefile, theHarvester\n\n"
        << "Type 'nexus-tools' for interactive tool browser\n"
        << "Type 'nexus-help' for NexusOS documentation\n";

    log_ok("Branding applied");
    return VoidResult::ok();
}

} // namespace nexus
