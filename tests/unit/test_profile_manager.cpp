#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "profiles/profile_manager.hpp"

using namespace nexus;
namespace fs = std::filesystem;

static fs::path create_test_profiles_dir() {
    auto dir = fs::temp_directory_path() / "nexus_test_profiles";
    fs::create_directories(dir);

    // base.yaml
    {
        std::ofstream f(dir / "base.yaml");
        f << R"(
name: base
description: "Base packages"
packages:
  - bash
  - coreutils
  - systemd
overlays:
  - common
)";
    }
    // analyst.yaml (extends base)
    {
        std::ofstream f(dir / "analyst.yaml");
        f << R"(
name: analyst
description: "Analyst profile"
extends:
  - base
packages:
  - nmap
  - wireshark
  - tcpdump
packages_hardened:
  - aide
packages_lab:
  - strace
overlays:
  - analyst
)";
    }
    return dir;
}

TEST_CASE("ProfileManager: list profiles", "[unit][profile]") {
    auto dir = create_test_profiles_dir();
    ProfileManager pm(dir);
    auto names = pm.list();
    REQUIRE(!names.empty());
    CHECK(std::find(names.begin(), names.end(), "analyst") != names.end());
    CHECK(std::find(names.begin(), names.end(), "base") != names.end());
    fs::remove_all(dir);
}

TEST_CASE("ProfileManager: load profile", "[unit][profile]") {
    auto dir = create_test_profiles_dir();
    ProfileManager pm(dir);

    auto r = pm.load("analyst");
    REQUIRE(r.is_ok());
    auto& p = r.value();
    CHECK(p.name == "analyst");
    CHECK(!p.description.empty());
    // Should contain both analyst and base packages (inheritance)
    auto it_nmap  = std::find(p.packages.begin(), p.packages.end(), "nmap");
    auto it_bash  = std::find(p.packages.begin(), p.packages.end(), "bash");
    CHECK(it_nmap  != p.packages.end());
    CHECK(it_bash  != p.packages.end());
    fs::remove_all(dir);
}

TEST_CASE("ProfileManager: get_packages for mode", "[unit][profile]") {
    auto dir = create_test_profiles_dir();
    ProfileManager pm(dir);

    auto hardened = pm.get_packages("analyst", BuildMode::Hardened);
    REQUIRE(hardened.is_ok());
    auto& hp = hardened.value();
    CHECK(std::find(hp.begin(), hp.end(), "aide") != hp.end());

    auto lab = pm.get_packages("analyst", BuildMode::Lab);
    REQUIRE(lab.is_ok());
    auto& lp = lab.value();
    CHECK(std::find(lp.begin(), lp.end(), "strace") != lp.end());

    fs::remove_all(dir);
}

TEST_CASE("ProfileManager: missing profile returns error", "[unit][profile]") {
    auto dir = create_test_profiles_dir();
    ProfileManager pm(dir);
    auto r = pm.load("nonexistent_profile_xyz");
    CHECK(r.is_err());
    fs::remove_all(dir);
}

TEST_CASE("ProfileManager: get_overlays includes common first", "[unit][profile]") {
    auto dir = create_test_profiles_dir();
    ProfileManager pm(dir);
    auto r = pm.get_overlays("analyst", BuildMode::Lab);
    REQUIRE(r.is_ok());
    auto& overlays = r.value();
    REQUIRE(!overlays.empty());
    CHECK(overlays.front() == "common");
    fs::remove_all(dir);
}
