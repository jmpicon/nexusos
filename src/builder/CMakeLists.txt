add_library(nexus_builder STATIC
    rootfs_builder.cpp
    iso_builder.cpp
)
target_include_directories(nexus_builder PUBLIC
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(nexus_builder PUBLIC
    nexus_config nexus_packaging fmt::fmt)
