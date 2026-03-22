# NexusOS Builder — Convenience Makefile
# Wraps CMake presets for common workflows.
# Usage: make <target>

.PHONY: all debug release ci clean distclean test install deps doctor help

BINARY    := build/debug/src/cli/nexus
BUILD_DIR := build

# ── Default target ─────────────────────────────────────────────────────────────
all: debug

# ── Configure + Build ──────────────────────────────────────────────────────────
debug:
	cmake --preset debug
	cmake --build --preset debug

release:
	cmake --preset release
	cmake --build --preset release

ci:
	cmake --preset ci
	cmake --build --preset ci

# ── Testing ────────────────────────────────────────────────────────────────────
test: debug
	ctest --preset all --output-on-failure

test-unit: debug
	ctest --preset unit --output-on-failure

test-integration: debug
	ctest --preset integration --output-on-failure

# ── Bootstrap dependencies ─────────────────────────────────────────────────────
deps:
	bash scripts/bootstrap/install-deps.sh

doctor:
	bash scripts/build/check-env.sh

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf dist release/*.iso release/*.sha256 release/*.manifest release/*.json

# ── Run CLI shortcuts ──────────────────────────────────────────────────────────
check-env: debug
	$(BINARY) doctor

build-rootfs-analyst: debug
	sudo $(BINARY) build-rootfs --profile analyst

build-iso-analyst: debug
	sudo $(BINARY) build-iso --profile analyst --output ./release

smoke-test: debug
	$(BINARY) smoke-test ./release/NexusOS-analyst-0.1.0.iso

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo ""
	@echo "NexusOS Builder — Makefile targets"
	@echo "════════════════════════════════════"
	@echo "  make deps         Bootstrap build dependencies"
	@echo "  make doctor       Validate build environment"
	@echo "  make debug        Build debug binary"
	@echo "  make release      Build release binary"
	@echo "  make test         Run all tests"
	@echo "  make test-unit    Run unit tests only"
	@echo "  make clean        Remove build artifacts"
	@echo "  make distclean    Remove build + release artifacts"
	@echo ""
	@echo "CLI shortcuts (require sudo for build steps):"
	@echo "  make check-env          nexus doctor"
	@echo "  make build-rootfs-analyst"
	@echo "  make build-iso-analyst"
	@echo "  make smoke-test"
	@echo ""
