add_library(nexus_branding STATIC
    branding_manager.cpp
)
target_include_directories(nexus_branding PUBLIC
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(nexus_branding PUBLIC nexus_config fmt::fmt)
