add_executable(nexus main.cpp)

target_include_directories(nexus PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nexus PRIVATE
    nexus_core
    CLI11::CLI11
    fmt::fmt
)

# Install the binary
install(TARGETS nexus RUNTIME DESTINATION bin)
