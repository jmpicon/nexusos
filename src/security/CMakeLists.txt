add_library(nexus_security STATIC
    hardening_engine.cpp
)
target_include_directories(nexus_security PUBLIC
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(nexus_security PUBLIC nexus_config fmt::fmt)
