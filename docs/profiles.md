# NexusOS — Profiles Reference

## Profile System

Profiles are YAML files in `profiles/`. They define what gets installed,
which overlays are applied, and which modes are supported.

### Inheritance

Profiles can extend other profiles:
```yaml
extends:
  - analyst  # inherits all packages and overlays from analyst
```

Inheritance is resolved recursively. Package lists are merged (parent first,
deduplicated). The build order is: grandparent → parent → child.

### Package lists

```yaml
packages:          # installed in both modes
packages_hardened: # additionally installed in hardened mode
packages_lab:      # additionally installed in lab mode
packages_exclude:  # removed even if inherited
```

### Overlays

```yaml
overlays:
  - forensic  # overlays/forensic/ is rsync'd into rootfs
```

`common` is always applied first (the C++ code prepends it automatically).
Mode overlays (`hardened`, `lab`) are automatically appended by the resolver.

---

## Profile: `core`

**Use case:** Minimum viable NexusOS — kernel, init, networking, shell.
All other profiles extend this.

**Packages:** bash, coreutils, systemd, iproute2, nmap, openssh-client,
live-boot, linux-image-amd64, nftables, auditd, apparmor

**Modes:** hardened ✓ lab ✓
**Desktop:** none

---

## Profile: `analyst`

**Use case:** Network analyst workstation — traffic capture, scanning,
monitoring, system auditing.

**Extends:** core

**Key tools:** nmap, wireshark, tshark, tcpdump, zeek, suricata, lynis,
osquery, rkhunter, aide, btop, jq, ripgrep

**Modes:** hardened ✓ lab ✓
**Desktop:** XFCE4

---

## Profile: `forensic`

**Use case:** Digital forensics and incident response.

**Extends:** analyst

**Key tools:** sleuthkit, volatility3, yara, binwalk, foremost, scalpel,
dc3dd, testdisk, exiftool, hashdeep, ewf-tools, afflib-tools

**Modes:** hardened ✓ lab ✓
**Desktop:** XFCE4

---

## Profile: `blueteam`

**Use case:** Defensive security operations — monitoring, hardening,
threat hunting, patch management.

**Extends:** analyst

**Key tools:** osquery, lynis, openscap, aide, trivy, suricata, zeek,
prometheus, grafana, filebeat, wazuh-agent, clamav

**Modes:** hardened ✓ lab ✓
**Desktop:** XFCE4

---

## Profile: `reverse`

**Use case:** Reverse engineering and binary analysis.

**Extends:** core (not analyst — lighter base for focused RE work)

**Key tools:** ghidra, radare2, gdb, gdb-multiarch, lldb, binwalk,
strace, ltrace, valgrind, python3-capstone, qemu-user, nasm, checksec

**Modes:** hardened ✓ lab ✓
**Desktop:** XFCE4

Note: Ghidra requires manual installation or an external repo as it is
not in Debian main. The profile installs `default-jdk` as a prerequisite.

---

## Profile: `cloud`

**Use case:** Cloud infrastructure security, container security, Kubernetes.

**Extends:** analyst

**Key tools:** podman, trivy, grype, syft, kubectl, helm, k9s,
terraform, ansible, kube-bench, age, sops, jq, yq, awscli

**Modes:** hardened ✓ lab ✓ (lab also installs kind, minikube)
**Desktop:** XFCE4

---

## Profile: `lab`

**Use case:** Isolated security lab with virtualisation support.
Not intended for production or internet-facing use.

**Extends:** forensic

**Key tools:** qemu-kvm, virt-manager, virt-viewer, libvirt, ovmf,
podman, bubblewrap, firejail, yara, clamav, zeek, tshark

**Modes:** lab only (hardened=false — KVM and nested virt conflict with strict hardening)
**Desktop:** XFCE4

---

## Profile: `full`

**Use case:** Everything. All profiles combined.

**Extends:** analyst + forensic + reverse + cloud (multiple inheritance)

**Estimated size:** ~8 GB uncompressed, ~3-4 GB compressed ISO

**Modes:** hardened ✓ lab ✓
**Desktop:** XFCE4

---

## Creating a Custom Profile

```yaml
# profiles/custom-pentest.yaml
name: custom-pentest
description: "Custom pentesting profile for web assessments"
version: "0.1.0"
desktop: xfce4

extends:
  - analyst

packages:
  - sqlmap
  - wfuzz
  - gobuster
  - ffuf
  - nikto
  - whatweb

packages_lab:
  - zaproxy

overlays:
  - custom-pentest

support_hardened: false
support_lab: true
```

Then:
```bash
sudo nexus build-rootfs --profile custom-pentest --mode lab
sudo nexus build-iso --profile custom-pentest --output ./release
```

---

## Adding Packages Not in Debian Main

Some tools (Ghidra, Velociraptor, etc.) are not in Debian repositories.
Options:

1. **Use the official package from the tool vendor:**
```yaml
# nexus.yaml
extra_repos:
  - "deb [signed-by=/etc/apt/keyrings/example.gpg] https://packages.example.com/apt stable main"
```

2. **Install via a chroot hook:**
```yaml
# profiles/reverse.yaml
hook_post_packages: "install-ghidra.sh"
```
Then create `scripts/chroot/install-ghidra.sh` with the installation logic.

3. **Include a `.deb` in the overlay:**
Place the `.deb` in `overlays/reverse/opt/nexus-debs/` and run `dpkg -i`
in the hook script.
