#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include "utils/checksum.hpp"

using namespace nexus;
namespace fs = std::filesystem;

TEST_CASE("Checksum: sha256 of known string", "[unit][checksum]") {
    // echo -n "hello" | sha256sum = 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824
    auto r = Checksum::sha256_string("hello");
    REQUIRE(r.is_ok());
    CHECK(r.value() == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST_CASE("Checksum: sha256 of file", "[unit][checksum]") {
    auto tmp = fs::temp_directory_path() / "nexus_checksum_test.txt";
    {
        std::ofstream ofs(tmp);
        ofs << "hello";
    }
    auto r = Checksum::sha256_file(tmp);
    REQUIRE(r.is_ok());
    CHECK(r.value() == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    fs::remove(tmp);
}

TEST_CASE("Checksum: write and verify sidecar", "[unit][checksum]") {
    auto tmp = fs::temp_directory_path() / "nexus_sidecar_test.bin";
    {
        std::ofstream ofs(tmp);
        ofs << "test content for sidecar";
    }
    auto w = Checksum::write_sha256_file(tmp);
    REQUIRE(w.is_ok());

    auto sidecar = fs::path(tmp.string() + ".sha256");
    REQUIRE(fs::exists(sidecar));

    auto v = Checksum::verify_sha256_file(tmp);
    REQUIRE(v.is_ok());
    CHECK(v.value() == true);

    // Corrupt the file
    {
        std::ofstream ofs(tmp, std::ios::app);
        ofs << "corrupted";
    }
    auto v2 = Checksum::verify_sha256_file(tmp);
    REQUIRE(v2.is_ok());
    CHECK(v2.value() == false);

    fs::remove(tmp);
    fs::remove(sidecar);
}

TEST_CASE("Checksum: error on missing file", "[unit][checksum]") {
    auto r = Checksum::sha256_file("/nonexistent/path/file.iso");
    CHECK(r.is_err());
}
