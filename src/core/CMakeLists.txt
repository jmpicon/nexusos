add_library(nexus_core STATIC
    orchestrator.cpp
)
target_include_directories(nexus_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(nexus_core PUBLIC
    nexus_utils
    nexus_config
    nexus_profiles
    nexus_packaging
    nexus_builder
    nexus_security
    nexus_branding
    nexus_testing
    fmt::fmt
)
