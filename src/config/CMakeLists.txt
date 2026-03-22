add_library(nexus_config STATIC
    config_parser.cpp
)

target_include_directories(nexus_config PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nexus_config PUBLIC
    nexus_utils
    yaml-cpp::yaml-cpp
    fmt::fmt
)
