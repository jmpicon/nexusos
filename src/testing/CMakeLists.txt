add_library(nexus_testing STATIC
    smoke_tester.cpp
)
target_include_directories(nexus_testing PUBLIC
    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(nexus_testing PUBLIC nexus_config fmt::fmt)
