add_library(nexus_packaging STATIC
    package_manager.cpp
)
target_include_directories(nexus_packaging PUBLIC
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(nexus_packaging PUBLIC nexus_config fmt::fmt)
