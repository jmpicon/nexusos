add_library(nexus_utils STATIC
    logger.cpp
    process.cpp
    checksum.cpp
    manifest.cpp
)

target_include_directories(nexus_utils PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nexus_utils PUBLIC
    fmt::fmt
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
)
