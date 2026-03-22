#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <functional>
#include <chrono>
#include <filesystem>

namespace nexus {

enum class LogLevel : int {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5,
};

[[nodiscard]] std::string_view level_name(LogLevel l) noexcept;
[[nodiscard]] std::string_view level_color(LogLevel l) noexcept;

class Logger {
public:
    using Sink = std::function<void(LogLevel, std::string_view)>;

    static Logger& instance();

    void set_level(LogLevel l)     noexcept { min_level_ = l; }
    void set_color(bool on)        noexcept { color_ = on; }
    void set_quiet(bool q)         noexcept { quiet_ = q; }
    void set_log_file(const std::filesystem::path& p);
    void add_sink(Sink s)                   { sinks_.push_back(std::move(s)); }

    void log(LogLevel level, std::string_view msg,
             std::string_view file = "", int line = 0);

    void trace(std::string_view m) { log(LogLevel::TRACE, m); }
    void debug(std::string_view m) { log(LogLevel::DEBUG, m); }
    void info (std::string_view m) { log(LogLevel::INFO,  m); }
    void warn (std::string_view m) { log(LogLevel::WARN,  m); }
    void error(std::string_view m) { log(LogLevel::ERROR, m); }
    void fatal(std::string_view m) { log(LogLevel::FATAL, m); }

    // ── progress helpers ──────────────────────────────────────────────────────
    void step(std::string_view step_name, std::string_view detail = "");
    void ok  (std::string_view msg);
    void fail(std::string_view msg);

private:
    Logger() = default;
    ~Logger();

    LogLevel               min_level_ {LogLevel::INFO};
    bool                   color_     {true};
    bool                   quiet_     {false};
    std::mutex             mtx_;
    std::ofstream          file_;
    std::vector<Sink>      sinks_;
};

// ── Free convenience functions (use Logger::instance() internally) ─────────────
#define NEXUS_LOG_LOC(level, msg) \
    ::nexus::Logger::instance().log(level, msg, __FILE__, __LINE__)

inline void log_trace(std::string_view m) { Logger::instance().trace(m); }
inline void log_debug(std::string_view m) { Logger::instance().debug(m); }
inline void log_info (std::string_view m) { Logger::instance().info(m);  }
inline void log_warn (std::string_view m) { Logger::instance().warn(m);  }
inline void log_error(std::string_view m) { Logger::instance().error(m); }
inline void log_fatal(std::string_view m) { Logger::instance().fatal(m); }
inline void log_step (std::string_view s, std::string_view d = "") {
    Logger::instance().step(s, d);
}
inline void log_ok   (std::string_view m) { Logger::instance().ok(m);    }
inline void log_fail (std::string_view m) { Logger::instance().fail(m);  }

} // namespace nexus
