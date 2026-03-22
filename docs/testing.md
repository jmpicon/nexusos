# NexusOS — Testing Guide

## Test Architecture

```
tests/
├── unit/
│   ├── test_config_parser.cpp    — ConfigParser unit tests
│   ├── test_profile_manager.cpp  — ProfileManager unit tests
│   ├── test_manifest.cpp         — Manifest read/write tests
│   ├── test_checksum.cpp         — SHA-256 checksum tests
│   └── test_orchestrator.cpp     — BuildOptions, Result<T>, mode conversion
└── integration/
    └── test_build_pipeline.cpp   — Pipeline coordination (no actual builds)
```

## Running Tests

```bash
# All tests
cmake --preset debug
cmake --build --preset debug
ctest --preset all

# Unit tests only
ctest --preset unit

# Integration tests only
ctest --preset integration

# Via make shortcut
make test
make test-unit
```

## Test Categories (Catch2 tags)

| Tag             | Description                                        |
|-----------------|---------------------------------------------------|
| `[unit]`        | Fast, no filesystem writes, no root needed        |
| `[integration]` | Filesystem I/O, config loading, no actual builds  |
| `[config]`      | ConfigParser tests                                |
| `[profile]`     | ProfileManager tests                              |
| `[checksum]`    | Checksum tests                                    |
| `[manifest]`    | Manifest tests                                    |
| `[orchestrator]`| Orchestrator and Result<T> tests                  |
| `[doctor]`      | doctor() integration test (environment-dependent) |

## Adding Tests

```cpp
// tests/unit/test_new_feature.cpp
#include <catch2/catch_test_macros.hpp>
#include "my_module/my_class.hpp"

TEST_CASE("MyClass: basic operation", "[unit][my-module]") {
    MyClass obj;
    auto result = obj.do_thing("input");
    REQUIRE(result.is_ok());
    CHECK(result.value() == "expected");
}
```

Register in `tests/CMakeLists.txt`:
```cmake
add_executable(nexus_unit_tests
    ...
    unit/test_new_feature.cpp
)
```

## Smoke Test

```bash
# Full QEMU boot test
nexus smoke-test ./release/NexusOS-analyst-0.1.0-amd64.iso

# Direct script (no nexus binary needed)
bash scripts/test/smoke-test-qemu.sh ./release/NexusOS-analyst-0.1.0-amd64.iso 120 2048
```

## Rootfs Validation

```bash
bash scripts/test/validate-rootfs.sh /tmp/nexus-workspace/rootfs
```

Checks for required paths, kernel presence, NexusOS branding, and config files.
