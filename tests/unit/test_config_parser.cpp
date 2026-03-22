#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <fstream>

#include "config/config_parser.hpp"

using namespace nexus;
namespace fs = std::filesystem;

TEST_CASE("ConfigParser: defaults when no file present", "[unit][config]") {
    auto r = ConfigParser::load("/nonexistent/path/nexus.yaml");
    REQUIRE(r.is_ok());
    auto& cfg = r.value();
    CHECK(cfg.distro_name == "NexusOS");
    CHECK(!cfg.debian_mirror.empty());
    CHECK(!cfg.debian_suite.empty());
    CHECK(cfg.arch == "amd64");
    CHECK(cfg.jobs >= 1);
}

TEST_CASE("ConfigParser: load from file", "[unit][config]") {
    auto tmp = fs::temp_directory_path() / "nexus_test_config.yaml";
    {
        std::ofstream ofs(tmp);
        ofs << R"(
distro_name: "TestOS"
distro_codename: "Test"
version: "1.2.3"
base:
  mirror: "http://test.mirror.example/"
  suite: "buster"
  arch: "arm64"
build:
  jobs: 8
  use_cache: false
)";
    }

    auto r = ConfigParser::load(tmp);
    REQUIRE(r.is_ok());
    auto& cfg = r.value();
    CHECK(cfg.distro_name == "TestOS");
    CHECK(cfg.distro_codename == "Test");
    CHECK(cfg.version == "1.2.3");
    CHECK(cfg.debian_mirror == "http://test.mirror.example/");
    CHECK(cfg.debian_suite == "buster");
    CHECK(cfg.arch == "arm64");
    CHECK(cfg.jobs == 8);
    CHECK_FALSE(cfg.use_cache);

    fs::remove(tmp);
}

TEST_CASE("ConfigParser: write and re-read default", "[unit][config]") {
    auto tmp = fs::temp_directory_path() / "nexus_default_write.yaml";
    fs::remove(tmp);

    auto w = ConfigParser::write_default(tmp);
    REQUIRE(w.is_ok());
    REQUIRE(fs::exists(tmp));

    auto r = ConfigParser::load(tmp);
    REQUIRE(r.is_ok());
    CHECK(r.value().distro_name == "NexusOS");

    // Should fail if file already exists
    auto w2 = ConfigParser::write_default(tmp);
    CHECK(w2.is_err());

    fs::remove(tmp);
}

TEST_CASE("ConfigParser: validate catches bad config", "[unit][config]") {
    NexusConfig bad;
    bad.distro_name = "";  // empty name is invalid
    bad.jobs = 0;          // invalid jobs

    auto issues = ConfigParser::validate(bad);
    REQUIRE(!issues.empty());
    bool found_name = false, found_jobs = false;
    for (auto& i : issues) {
        if (i.find("distro_name") != std::string::npos) found_name = true;
        if (i.find("jobs") != std::string::npos)        found_jobs = true;
    }
    CHECK(found_name);
    CHECK(found_jobs);
}

TEST_CASE("ConfigParser: env overrides", "[unit][config]") {
    setenv("NEXUS_SUITE", "bullseye", 1);
    setenv("NEXUS_JOBS", "16", 1);

    NexusConfig cfg;
    ConfigParser::apply_env_overrides(cfg);

    CHECK(cfg.debian_suite == "bullseye");
    CHECK(cfg.jobs == 16);

    unsetenv("NEXUS_SUITE");
    unsetenv("NEXUS_JOBS");
}
