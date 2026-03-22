#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include "utils/manifest.hpp"

using namespace nexus;
namespace fs = std::filesystem;

TEST_CASE("Manifest: build from directory", "[unit][manifest]") {
    auto dir = fs::temp_directory_path() / "nexus_manifest_test";
    fs::create_directories(dir);

    // Create fake artifacts
    { std::ofstream f(dir / "NexusOS-test.iso"); f << "fake iso content"; }
    { std::ofstream f(dir / "NexusOS-test.iso.sha256"); f << "abc123  NexusOS-test.iso\n"; }

    auto r = Manifest::build(dir, "analyst", "lab", "0.1.0");
    REQUIRE(r.is_ok());
    auto& m = r.value();
    CHECK(m.profile == "analyst");
    CHECK(m.mode == "lab");
    CHECK(m.version == "0.1.0");
    CHECK(!m.build_date.empty());
    // Should have exactly one artifact (the iso — sidecar .sha256 is skipped)
    CHECK(m.artifacts.size() == 1);
    CHECK(m.artifacts[0].filename == "NexusOS-test.iso");

    fs::remove_all(dir);
}

TEST_CASE("Manifest: write and read JSON", "[unit][manifest]") {
    auto dir = fs::temp_directory_path() / "nexus_manifest_json";
    fs::create_directories(dir);

    BuildManifest m;
    m.profile    = "forensic";
    m.mode       = "hardened";
    m.version    = "0.2.0";
    m.build_date = "2026-01-01T00:00:00Z";
    m.packages   = {"bash", "nmap", "wireshark"};
    ManifestEntry e;
    e.filename    = "NexusOS.iso";
    e.sha256      = "deadbeef";
    e.size_bytes  = 1234;
    e.type        = "iso";
    m.artifacts.push_back(e);

    auto out = dir / "test.manifest.json";
    auto w = Manifest::write_json(m, out);
    REQUIRE(w.is_ok());
    REQUIRE(fs::exists(out));

    auto r2 = Manifest::read_json(out);
    REQUIRE(r2.is_ok());
    auto& m2 = r2.value();
    CHECK(m2.profile == "forensic");
    CHECK(m2.mode == "hardened");
    CHECK(m2.version == "0.2.0");
    CHECK(m2.packages.size() == 3);
    CHECK(m2.artifacts.size() == 1);
    CHECK(m2.artifacts[0].sha256 == "deadbeef");

    fs::remove_all(dir);
}

TEST_CASE("Manifest: error on missing directory", "[unit][manifest]") {
    auto r = Manifest::build("/nonexistent/dir", "x", "y");
    CHECK(r.is_err());
}
