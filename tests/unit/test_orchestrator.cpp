#include <catch2/catch_test_macros.hpp>
#include "core/orchestrator.hpp"
#include "nexus/common.hpp"

using namespace nexus;

TEST_CASE("BuildOptions: defaults are valid", "[unit][orchestrator]") {
    BuildOptions opts;
    CHECK(!opts.profile.empty());
    CHECK(opts.jobs > 0);
    CHECK(opts.mode == BuildMode::Lab);
}

TEST_CASE("BuildMode: string conversion round-trip", "[unit][orchestrator]") {
    CHECK(to_string(BuildMode::Hardened) == "hardened");
    CHECK(to_string(BuildMode::Lab) == "lab");

    auto h = build_mode_from_string("hardened");
    REQUIRE(h.has_value());
    CHECK(*h == BuildMode::Hardened);

    auto l = build_mode_from_string("lab");
    REQUIRE(l.has_value());
    CHECK(*l == BuildMode::Lab);

    auto n = build_mode_from_string("unknown");
    CHECK(!n.has_value());
}

TEST_CASE("Result<T>: ok and err", "[unit][result]") {
    auto ok_r = Result<int>::ok(42);
    REQUIRE(ok_r.is_ok());
    CHECK(ok_r.value() == 42);
    CHECK(bool(ok_r));

    auto err_r = Result<int>::err("something failed");
    REQUIRE(err_r.is_err());
    CHECK(err_r.error() == "something failed");
    CHECK(!bool(err_r));
}

TEST_CASE("Result<void>: ok and err", "[unit][result]") {
    auto ok_r = VoidResult::ok();
    CHECK(ok_r.is_ok());
    CHECK(bool(ok_r));

    auto err_r = VoidResult::err("pipeline failed");
    CHECK(err_r.is_err());
    CHECK(err_r.error() == "pipeline failed");
    CHECK(!bool(err_r));
}

TEST_CASE("Orchestrator: constructs without crash", "[unit][orchestrator]") {
    // Just verify it constructs with default options
    BuildOptions opts;
    opts.profile = "core";
    REQUIRE_NOTHROW([&]{ Orchestrator orch(opts); }());
}
