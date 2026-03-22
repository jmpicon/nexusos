# NexusOS — Hardening Reference

## Design Philosophy

NexusOS implements **two modes** rather than one static hardening level,
because the same ISO often serves both production analysis and lab use.

| Aspect              | Hardened mode                       | Lab mode                        |
|---------------------|-------------------------------------|---------------------------------|
| nftables policy     | Deny all inbound + forward          | Accept (stateful tracking)      |
| sysctl ptrace       | scope=2 (admin only)                | scope=1 (children only)         |
| sysctl BPF          | unprivileged disabled               | no restriction                  |
| root login          | locked (passwd --lock)              | locked                          |
| Sudo                | password required                   | passwordless (NOPASSWD)         |
| SSH auth            | keys only                           | password allowed                |
| auditd rules        | immutable (-e 2)                    | active, not immutable           |
| AppArmor            | enforcing                           | enabled, permissive profiles    |

---

## sysctl Hardening

**Common overlay** (`overlays/common/etc/sysctl.d/99-nexus-hardening.conf`):
Applied to all profiles.

Key settings:
```
kernel.dmesg_restrict = 1       # unprivileged dmesg blocked
kernel.randomize_va_space = 2   # full ASLR
kernel.yama.ptrace_scope = 1    # ptrace only to children
fs.protected_hardlinks = 1      # prevent hardlink attacks
fs.protected_symlinks = 1       # prevent symlink attacks
fs.suid_dumpable = 0            # no core dumps from SUID binaries
net.ipv4.tcp_syncookies = 1     # SYN flood protection
net.ipv4.conf.all.rp_filter = 1 # reverse path filtering
```

**Hardened extra** (`overlays/hardened/etc/sysctl.d/99-nexus-hardened-extra.conf`):
```
kernel.kptr_restrict = 2        # kernel pointer leak prevention
kernel.perf_event_paranoid = 3  # perf events restricted
kernel.yama.ptrace_scope = 2    # admin-only ptrace
kernel.unprivileged_bpf_disabled = 1  # BPF restricted to root
kernel.sysrq = 0                # magic SysRq disabled
net.ipv4.ip_forward = 0         # no routing
vm.mmap_rnd_bits = 32           # maximum ASLR entropy
kernel.core_pattern = |/bin/false  # disable core dumps
```

---

## nftables Firewall

### Hardened mode (`overlays/hardened/etc/...`)
```
table inet nexus_filter {
    chain input  { policy drop; stateful+loopback+ICMP }
    chain forward { policy drop; }
    chain output  { policy accept; }
}
```

### Lab mode
```
table inet nexus_filter {
    chain input  { policy accept; stateful tracking }
    chain forward { policy accept; for virtual networking }
    chain output  { policy accept; }
}
```

The nftables service is enabled and starts at boot in both modes.

---

## AppArmor

AppArmor is enabled in both modes. The `apparmor` service starts at boot.
Boot parameter `apparmor=1 security=apparmor` is passed via GRUB.

Profiles in `/etc/apparmor.d/` cover the most sensitive system utilities.
In hardened mode, enforcement is stricter; in lab mode, complain mode is
used for most application profiles to avoid workflow disruption.

---

## auditd

### Rule categories (both modes)
- Identity changes: `/etc/passwd`, `/etc/shadow`, `/etc/sudoers`
- Privileged execution: `execve` by EUID=0 with AUID from user space
- Module loading: `insmod`, `rmmod`, `modprobe`
- Mount/unmount operations
- Cron modifications
- Unauthorized file access (EACCES, EPERM)

### Hardened mode additions
- Rule set made immutable (`-e 2`): rules cannot be changed after boot
  without reboot
- Buffer size increased to 16384

### Log rotation
`/var/log/audit/audit.log` rotates at 32 MB, keeping 10 files.
`/etc/logrotate.d/auditd` manages the rotation.

---

## SSH Hardening

Applied by `HardeningEngine::configure_sshd()` in both modes:
```
Protocol 2
PermitRootLogin no
X11Forwarding no
AllowAgentForwarding no
MaxAuthTries 3
LoginGraceTime 30
ClientAliveInterval 300
Ciphers chacha20-poly1305@openssh.com,aes256-gcm@openssh.com
MACs hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com
KexAlgorithms curve25519-sha256,curve25519-sha256@libssh.org
```

In hardened mode additionally: `PasswordAuthentication no`
In lab mode: `PasswordAuthentication yes` (for VM-to-VM convenience)

---

## User and PAM

- Default user: `nexus` (UID 1000)
- Root password: locked (`passwd --lock root`)
- No direct root console login (empty `/etc/securetty`)
- `umask 027` in `/etc/login.defs` and `/etc/profile.d/nexus-env.sh`
- In hardened mode: `PASS_MAX_DAYS 90`, `PASS_MIN_DAYS 1`

---

## Disabled Services

Services disabled in both modes (not needed for security work):
- `apt-daily.timer`, `apt-daily-upgrade.timer`
- `motd-news.service`
- `bluetooth.service`
- `ModemManager.service`
- `avahi-daemon.service`

---

## Secure Boot

NexusOS does not include signed GRUB shims or kernel signatures.
For Secure Boot-capable deployments:
1. Enrol the build host's MOK key
2. Sign the kernel and GRUB bootloader with `sbsign`
3. Use `grub-install --uefi-secure-boot`

This is documented as a future roadmap item.
