#!/usr/bin/env bash
# NexusOS — Validate rootfs structure
# Usage: validate-rootfs.sh <rootfs_dir>
set -euo pipefail

RESET='\033[0m'; GREEN='\033[32m'; YELLOW='\033[33m'; RED='\033[31m'; BOLD='\033[1m'

ROOTFS="${1:?Usage: $0 <rootfs_dir>}"
PASS=0; FAIL=0; WARN=0

check_path() {
    local path="$1"
    local desc="$2"
    local required="${3:-true}"

    if [[ -e "${ROOTFS}/${path}" ]]; then
        echo -e "${GREEN}  ✓${RESET} ${desc}"
        ((PASS++))
    elif [[ "$required" == "true" ]]; then
        echo -e "${RED}  ✗${RESET} ${desc} [MISSING: ${path}]"
        ((FAIL++))
    else
        echo -e "${YELLOW}  !${RESET} ${desc} (optional, missing)"
        ((WARN++))
    fi
}

check_file_contains() {
    local path="$1"
    local pattern="$2"
    local desc="$3"
    if [[ -f "${ROOTFS}/${path}" ]] && grep -q "$pattern" "${ROOTFS}/${path}"; then
        echo -e "${GREEN}  ✓${RESET} ${desc}"
        ((PASS++))
    else
        echo -e "${YELLOW}  !${RESET} ${desc}"
        ((WARN++))
    fi
}

echo ""
echo -e "${BOLD}NexusOS Rootfs Validation: ${ROOTFS}${RESET}"
echo "════════════════════════════════════════"
echo ""

echo -e "${BOLD}Required filesystem structure:${RESET}"
check_path "bin"            "bin directory"
check_path "etc"            "etc directory"
check_path "lib"            "lib directory"
check_path "usr"            "usr directory"
check_path "var"            "var directory"
check_path "boot"           "boot directory"
check_path "home"           "home directory"
check_path "proc"           "proc mountpoint"
check_path "sys"            "sys mountpoint"
check_path "dev"            "dev mountpoint"
check_path "tmp"            "tmp directory"

echo ""
echo -e "${BOLD}Debian base:${RESET}"
check_path "etc/debian_version"  "debian_version"
check_path "usr/bin/bash"        "bash"
check_path "usr/bin/dpkg"        "dpkg"
check_path "usr/bin/apt-get"     "apt-get"
check_path "usr/lib/systemd"     "systemd"

echo ""
echo -e "${BOLD}NexusOS configuration:${RESET}"
check_path "etc/os-release"      "os-release"
check_path "etc/hostname"        "hostname"
check_path "etc/passwd"          "passwd"
check_path "etc/shadow"          "shadow"
check_path "etc/nftables.conf"   "nftables.conf"     false
check_path "etc/audit"           "audit config"      false
check_path "etc/apparmor"        "apparmor config"   false
check_path "etc/motd"            "motd"
check_path "home/nexus"          "nexus home directory"
check_path "usr/share/nexus"     "nexus data directory" false

echo ""
echo -e "${BOLD}Kernel:${RESET}"
KERNEL_FOUND=false
for kver_path in "${ROOTFS}"/boot/vmlinuz-*; do
    if [[ -f "$kver_path" ]]; then
        KERNEL_FOUND=true
        echo -e "${GREEN}  ✓${RESET} Kernel: $(basename "$kver_path")"
        ((PASS++))
        break
    fi
done
if [[ "$KERNEL_FOUND" == "false" ]]; then
    echo -e "${RED}  ✗${RESET} No kernel found in /boot"
    ((FAIL++))
fi

for initrd_path in "${ROOTFS}"/boot/initrd.img-*; do
    if [[ -f "$initrd_path" ]]; then
        echo -e "${GREEN}  ✓${RESET} Initrd: $(basename "$initrd_path")"
        ((PASS++))
        break
    fi
done

echo ""
echo -e "${BOLD}Content checks:${RESET}"
check_file_contains "etc/os-release" "NexusOS" "os-release mentions NexusOS"
check_file_contains "etc/hostname" "nexus" "hostname is 'nexus'"

echo ""
echo -e "${BOLD}Summary:${RESET}"
echo -e "  ${GREEN}Passed: ${PASS}${RESET}  ${RED}Failed: ${FAIL}${RESET}  ${YELLOW}Warnings: ${WARN}${RESET}"
echo ""

if [[ $FAIL -gt 0 ]]; then
    echo -e "${RED}Rootfs validation FAILED${RESET}"
    exit 1
fi

echo -e "${GREEN}Rootfs validation PASSED${RESET}"
echo ""
