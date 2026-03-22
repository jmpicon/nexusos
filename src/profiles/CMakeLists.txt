add_library(nexus_profiles STATIC
    profile_manager.cpp
)

target_include_directories(nexus_profiles PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nexus_profiles PUBLIC
    nexus_config
    yaml-cpp::yaml-cpp
    fmt::fmt
)
