#!/usr/bin/env bash
# NexusOS — Generate checksums for all release artifacts
# Usage: generate-checksums.sh <release_dir>
set -euo pipefail

RELEASE_DIR="${1:-./release}"

if [[ ! -d "$RELEASE_DIR" ]]; then
    echo "Error: release directory not found: $RELEASE_DIR"
    exit 1
fi

CHECKSUMS_FILE="${RELEASE_DIR}/CHECKSUMS.sha256"
> "$CHECKSUMS_FILE"  # truncate

for f in "${RELEASE_DIR}"/*; do
    [[ -f "$f" ]] || continue
    case "$(basename "$f")" in
        *.sha256|CHECKSUMS.sha256) continue ;;
    esac
    HASH=$(sha256sum "$f" | awk '{print $1}')
    echo "${HASH}  $(basename "$f")" >> "$CHECKSUMS_FILE"
    echo "  ✓ $(basename "$f"): ${HASH:0:16}..."
done

echo ""
echo "Checksums written to: $CHECKSUMS_FILE"
