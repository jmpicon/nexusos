#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "core/orchestrator.hpp"
#include "config/config_parser.hpp"
#include "profiles/profile_manager.hpp"
#include "utils/checksum.hpp"
#include "utils/manifest.hpp"

using namespace nexus;
namespace fs = std::filesystem;

// ── Integration tests — these test the pipeline logic but do NOT execute
// actual debootstrap/ISO builds. They verify pipeline coordination,
// config loading, and file I/O.

TEST_CASE("Integration: doctor returns meaningful result", "[integration][doctor]") {
    BuildOptions opts;
    opts.profile = "core";
    Orchestrator orch(opts);
    // doctor() may succeed or fail depending on host environment
    // but should not throw
    auto r = orch.doctor();
    // If it fails, the error message must be meaningful
    if (r.is_err())
        CHECK(!r.error().empty());
}

TEST_CASE("Integration: config loads and feeds orchestrator", "[integration][config]") {
    auto tmp_dir = fs::temp_directory_path() / "nexus_int_test";
    fs::create_directories(tmp_dir);
    auto cfg_path = tmp_dir / "nexus.yaml";

    {
        std::ofstream f(cfg_path);
        f << R"(
distro_name: "IntTestOS"
version: "0.0.1"
base:
  mirror: "http://deb.debian.org/debian"
  suite: "bookworm"
  arch: "amd64"
build:
  jobs: 2
)";
    }

    setenv("NEXUS_WORKSPACE", (tmp_dir / "workspace").c_str(), 1);
    auto r = ConfigParser::load(cfg_path);
    REQUIRE(r.is_ok());
    CHECK(r.value().distro_name == "IntTestOS");
    CHECK(r.value().jobs == 2);
    unsetenv("NEXUS_WORKSPACE");

    fs::remove_all(tmp_dir);
}

TEST_CASE("Integration: manifest round-trip with checksum", "[integration][manifest]") {
    auto dir = fs::temp_directory_path() / "nexus_int_manifest";
    fs::create_directories(dir);

    // Write a fake ISO
    auto iso = dir / "NexusOS-test-0.1.0-amd64.iso";
    { std::ofstream f(iso); f.write("ISO\x01\x43\x44\x30\x30\x31", 9); }

    auto w = Checksum::write_sha256_file(iso);
    REQUIRE(w.is_ok());

    auto mr = Manifest::build(dir, "core", "lab");
    REQUIRE(mr.is_ok());
    auto& m = mr.value();
    CHECK(!m.build_date.empty());

    auto manifest_path = dir / "NexusOS-test.manifest.json";
    auto mw = Manifest::write_json(m, manifest_path);
    REQUIRE(mw.is_ok());
    REQUIRE(fs::exists(manifest_path));

    auto read_back = Manifest::read_json(manifest_path);
    REQUIRE(read_back.is_ok());
    CHECK(read_back.value().profile == "core");

    fs::remove_all(dir);
}

TEST_CASE("Integration: profile resolution with inheritance", "[integration][profile]") {
    auto dir = fs::temp_directory_path() / "nexus_int_profiles";
    fs::create_directories(dir);

    { std::ofstream f(dir/"base.yaml");
      f << "name: base\npackages:\n  - bash\n  - coreutils\noverlays:\n  - common\n"; }
    { std::ofstream f(dir/"forensic.yaml");
      f << "name: forensic\nextends:\n  - base\npackages:\n  - sleuthkit\n  - yara\n"
           "packages_hardened:\n  - aide\noverlays:\n  - forensic\n"; }

    ProfileManager pm(dir);
    auto pkgs = pm.get_packages("forensic", BuildMode::Hardened);
    REQUIRE(pkgs.is_ok());
    auto& p = pkgs.value();
    CHECK(std::find(p.begin(), p.end(), "bash")       != p.end());
    CHECK(std::find(p.begin(), p.end(), "sleuthkit")  != p.end());
    CHECK(std::find(p.begin(), p.end(), "aide")       != p.end());

    auto overlays = pm.get_overlays("forensic", BuildMode::Hardened);
    REQUIRE(overlays.is_ok());
    CHECK(overlays.value().front() == "common");

    fs::remove_all(dir);
}
