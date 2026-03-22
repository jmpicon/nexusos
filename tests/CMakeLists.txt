include(CTest)
include(Catch2)

# ── Unit tests ────────────────────────────────────────────────────────────────
add_executable(nexus_unit_tests
    unit/test_config_parser.cpp
    unit/test_profile_manager.cpp
    unit/test_manifest.cpp
    unit/test_checksum.cpp
    unit/test_orchestrator.cpp
)

target_include_directories(nexus_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nexus_unit_tests PRIVATE
    nexus_core
    Catch2::Catch2WithMain
    fmt::fmt
)

catch_discover_tests(nexus_unit_tests
    TEST_SPEC "[unit]"
    PROPERTIES LABELS "unit"
)

# ── Integration tests ─────────────────────────────────────────────────────────
add_executable(nexus_integration_tests
    integration/test_build_pipeline.cpp
)

target_include_directories(nexus_integration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nexus_integration_tests PRIVATE
    nexus_core
    Catch2::Catch2WithMain
    fmt::fmt
)

catch_discover_tests(nexus_integration_tests
    TEST_SPEC "[integration]"
    PROPERTIES LABELS "integration"
)
