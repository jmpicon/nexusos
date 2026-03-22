cmake_minimum_required(VERSION 3.25)

project(nexus
    VERSION 0.1.0
    DESCRIPTION "NexusOS - Professional Cybersecurity Linux Distribution Builder"
    HOMEPAGE_URL "https://github.com/nexusos/nexus"
    LANGUAGES CXX
)

# ── C++ standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build options ─────────────────────────────────────────────────────────────
option(NEXUS_ENABLE_TESTS    "Build unit and integration tests"   ON)
option(NEXUS_ENABLE_ASAN     "Enable AddressSanitizer"           OFF)
option(NEXUS_ENABLE_UBSAN    "Enable UBSanitizer"                OFF)
option(NEXUS_STATIC_LINK     "Link statically where possible"    OFF)
option(NEXUS_BUILD_CONTAINER "Prepare for container build"       OFF)

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wshadow -Wformat=2
    -fstack-protector-strong
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Release>:-DNDEBUG>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-DNEXUS_DEBUG>
)

if(NEXUS_ENABLE_ASAN)
    add_compile_options(-fsanitize=address,leak)
    add_link_options(-fsanitize=address,leak)
endif()

if(NEXUS_ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# ── CMake module path ─────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

# ── Version header generation ─────────────────────────────────────────────────
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_BINARY_DIR}/generated/nexus/version_gen.hpp"
    @ONLY
)

# ── External dependencies via FetchContent ────────────────────────────────────
include(FetchContent)

# yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# nlohmann/json (single header)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# CLI11 (argument parsing)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(CLI11)

# fmt (text formatting)
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.2.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fmt)

# Catch2 for tests
if(NEXUS_ENABLE_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
endif()

# ── OpenSSL (checksums) ───────────────────────────────────────────────────────
find_package(OpenSSL REQUIRED)

# ── Include directories ───────────────────────────────────────────────────────
include_directories(
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_BINARY_DIR}/generated"
)

# ── Subdirectories ────────────────────────────────────────────────────────────
add_subdirectory(src/utils)
add_subdirectory(src/config)
add_subdirectory(src/profiles)
add_subdirectory(src/core)
add_subdirectory(src/builder)
add_subdirectory(src/security)
add_subdirectory(src/packaging)
add_subdirectory(src/branding)
add_subdirectory(src/testing)
add_subdirectory(src/cli)

if(NEXUS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Install rules ─────────────────────────────────────────────────────────────
install(TARGETS nexus RUNTIME DESTINATION bin)
install(DIRECTORY profiles/ DESTINATION share/nexus/profiles)
install(DIRECTORY packages/lists/ DESTINATION share/nexus/packages/lists)
install(DIRECTORY overlays/ DESTINATION share/nexus/overlays)

# ── Summary ───────────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "══════════════════════════════════════════════")
message(STATUS "  NexusOS Builder — ${PROJECT_VERSION}")
message(STATUS "══════════════════════════════════════════════")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ std     : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests       : ${NEXUS_ENABLE_TESTS}")
message(STATUS "  ASan        : ${NEXUS_ENABLE_ASAN}")
message(STATUS "  UBSan       : ${NEXUS_ENABLE_UBSAN}")
message(STATUS "══════════════════════════════════════════════")
message(STATUS "")
