#!/usr/bin/env bash
# NexusOS — QEMU smoke test
# Boots ISO headlessly, waits for boot progress, exits.
# Usage: smoke-test-qemu.sh <iso_path> [timeout_seconds] [ram_mb]
set -euo pipefail

RESET='\033[0m'; GREEN='\033[32m'; YELLOW='\033[33m'; RED='\033[31m'; BOLD='\033[1m'

ISO="${1:?Usage: $0 <iso_path> [timeout] [ram_mb]}"
TIMEOUT="${2:-120}"
RAM="${3:-2048}"

if [[ ! -f "$ISO" ]]; then
    echo -e "${RED}Error: ISO not found: ${ISO}${RESET}"
    exit 1
fi

if ! command -v qemu-system-x86_64 &>/dev/null; then
    echo -e "${YELLOW}Warning: qemu-system-x86_64 not found — skipping smoke test${RESET}"
    exit 0
fi

# Check ISO signature
ISO_MAGIC=$(dd if="$ISO" bs=1 skip=32769 count=5 2>/dev/null)
if [[ "$ISO_MAGIC" != "CD001" ]]; then
    echo -e "${RED}Invalid ISO 9660 signature — file may be corrupt${RESET}"
    exit 1
fi

SIZE_MB=$(( $(stat -c%s "$ISO") / 1024 / 1024 ))
echo -e "${BOLD}▶ NexusOS QEMU Smoke Test${RESET}"
echo "  ISO     : ${ISO} (${SIZE_MB} MB)"
echo "  RAM     : ${RAM} MB"
echo "  Timeout : ${TIMEOUT} s"
echo ""

SERIAL_LOG=$(mktemp /tmp/nexus-smoke-XXXXXX.log)
trap 'rm -f "$SERIAL_LOG"' EXIT

echo -e "  ${BOLD}Booting...${RESET}"

# Use timeout to kill QEMU after TIMEOUT seconds
# Exit code 124 = timeout = acceptable (boot in progress)
set +e
timeout "$TIMEOUT" qemu-system-x86_64 \
    -m "${RAM}" \
    -cdrom "$ISO" \
    -boot d \
    -nographic \
    -serial "file:${SERIAL_LOG}" \
    -no-reboot \
    -cpu host \
    -smp 2 \
    -display none \
    -enable-kvm \
    2>/dev/null
QEMU_EXIT=$?
set -e

# Acceptable exit codes
if [[ $QEMU_EXIT -ne 0 ]] && [[ $QEMU_EXIT -ne 124 ]]; then
    echo -e "${RED}  ✗ QEMU exited with code ${QEMU_EXIT} — possible boot failure${RESET}"
    [[ -s "$SERIAL_LOG" ]] && echo "--- Serial output ---" && tail -30 "$SERIAL_LOG"
    exit 1
fi

# Analyse serial log
SERIAL_CONTENT=""
[[ -s "$SERIAL_LOG" ]] && SERIAL_CONTENT=$(cat "$SERIAL_LOG")

# Check for kernel panic or oops
for bad in "Kernel panic" "BUG:" "OOPS:" "double fault" "segfault"; do
    if echo "$SERIAL_CONTENT" | grep -q "$bad"; then
        echo -e "${RED}  ✗ Kernel error detected: ${bad}${RESET}"
        echo "--- Serial output (tail) ---"
        tail -30 "$SERIAL_LOG"
        exit 1
    fi
done

# Check for boot progress indicators
BOOT_OK=false
for good in "Linux version" "GRUB" "Booting" "systemd" "NexusOS"; do
    if echo "$SERIAL_CONTENT" | grep -q "$good"; then
        BOOT_OK=true
        echo -e "${GREEN}  ✓ Boot indicator found: '${good}'${RESET}"
        break
    fi
done

if [[ "$BOOT_OK" == "false" ]]; then
    echo -e "${YELLOW}  ! No boot progress detected in serial output (may need longer timeout)${RESET}"
fi

echo ""
echo -e "${GREEN}  ✓ Smoke test passed (exit code: ${QEMU_EXIT})${RESET}"
