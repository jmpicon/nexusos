#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <optional>
#include <variant>
#include <memory>
#include <functional>
#include <vector>
#include <map>
#include <unordered_map>
#include <stdexcept>
#include <system_error>

namespace nexus {

// ── Result<T,E> — lightweight error propagation ───────────────────────────────
// Used throughout the codebase instead of exceptions in I/O paths.

template<typename T, typename E = std::string>
class Result {
    std::variant<T, E> data_;
    struct ok_tag  {};
    struct err_tag {};
    Result(ok_tag,  T val) : data_(std::in_place_index<0>, std::move(val)) {}
    Result(err_tag, E e)   : data_(std::in_place_index<1>, std::move(e))   {}
public:
    [[nodiscard]] static Result ok(T val) { return Result{ok_tag{},  std::move(val)}; }
    [[nodiscard]] static Result err(E e)  { return Result{err_tag{}, std::move(e)};   }
    [[nodiscard]] bool is_ok()  const noexcept { return data_.index() == 0; }
    [[nodiscard]] bool is_err() const noexcept { return data_.index() == 1; }
    [[nodiscard]] T&       value()       { return std::get<0>(data_); }
    [[nodiscard]] const T& value() const { return std::get<0>(data_); }
    [[nodiscard]] E&       error()       { return std::get<1>(data_); }
    [[nodiscard]] const E& error() const { return std::get<1>(data_); }
    explicit operator bool() const noexcept { return is_ok(); }

    // Monadic chain: if ok, map value; if err, propagate
    template<typename F>
    auto and_then(F&& f) -> decltype(f(std::declval<T>())) {
        if (is_ok()) return f(value());
        using R = decltype(f(std::declval<T>()));
        return R::err(error());
    }
};

// Specialisation for void results (pipeline steps)
template<typename E>
class Result<void, E> {
    std::optional<E> err_;
public:
    [[nodiscard]] static Result ok()    { return Result{}; }
    [[nodiscard]] static Result err(E e){ Result r; r.err_ = std::move(e); return r; }
    [[nodiscard]] bool is_ok()  const noexcept { return !err_.has_value(); }
    [[nodiscard]] bool is_err() const noexcept { return err_.has_value(); }
    [[nodiscard]] E&       error()       { return *err_; }
    [[nodiscard]] const E& error() const { return *err_; }
    explicit operator bool() const noexcept { return is_ok(); }
};

using VoidResult = Result<void, std::string>;

// ── BuildMode ─────────────────────────────────────────────────────────────────
enum class BuildMode {
    Hardened,  // minimal surface, strict policies
    Lab,       // flexible, virtualisation-friendly
};

inline std::string to_string(BuildMode m) {
    switch (m) {
        case BuildMode::Hardened: return "hardened";
        case BuildMode::Lab:      return "lab";
    }
    return "unknown";
}

inline std::optional<BuildMode> build_mode_from_string(std::string_view s) {
    if (s == "hardened") return BuildMode::Hardened;
    if (s == "lab")      return BuildMode::Lab;
    return std::nullopt;
}

// ── BuildOptions — passed to Orchestrator ─────────────────────────────────────
struct BuildOptions {
    std::string              profile      {"core"};
    BuildMode                mode         {BuildMode::Lab};
    std::filesystem::path    workspace    {"/tmp/nexus-workspace"};
    std::filesystem::path    output_dir   {"./release"};
    std::filesystem::path    cache_dir    {"/var/cache/nexus"};
    std::string              arch         {"amd64"};
    std::string              mirror       {"http://deb.debian.org/debian"};
    std::string              suite        {"bookworm"};
    bool                     dry_run      {false};
    bool                     verbose      {false};
    bool                     no_cache     {false};
    bool                     skip_smoke   {false};
    int                      jobs         {4};
};

// ── NexusError — structured error ────────────────────────────────────────────
struct NexusError {
    std::string code;
    std::string message;
    std::string context;

    explicit NexusError(std::string c, std::string m, std::string ctx = "")
        : code(std::move(c)), message(std::move(m)), context(std::move(ctx)) {}

    [[nodiscard]] std::string format() const {
        if (context.empty()) return "[" + code + "] " + message;
        return "[" + code + "] " + message + " (context: " + context + ")";
    }
};

} // namespace nexus
