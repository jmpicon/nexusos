#include "logger.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <fmt/core.h>
#include <fmt/chrono.h>

namespace nexus {

namespace {
    // ANSI color codes
    constexpr const char* RESET  = "\033[0m";
    constexpr const char* BOLD   = "\033[1m";
    constexpr const char* CYAN   = "\033[36m";
    constexpr const char* GREEN  = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* RED    = "\033[31m";
    constexpr const char* BRED   = "\033[1;31m";
    constexpr const char* GRAY   = "\033[90m";
    constexpr const char* BLUE   = "\033[34m";
    constexpr const char* MAGENTA= "\033[35m";

    std::string timestamp_now() {
        using namespace std::chrono;
        auto now  = system_clock::now();
        auto time = system_clock::to_time_t(now);
        auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        std::tm tm{};
        localtime_r(&time, &tm);
        return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            static_cast<int>(ms.count()));
    }
} // anonymous namespace

std::string_view level_name(LogLevel l) noexcept {
    switch (l) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "?????";
}

std::string_view level_color(LogLevel l) noexcept {
    switch (l) {
        case LogLevel::TRACE: return GRAY;
        case LogLevel::DEBUG: return CYAN;
        case LogLevel::INFO:  return GREEN;
        case LogLevel::WARN:  return YELLOW;
        case LogLevel::ERROR: return RED;
        case LogLevel::FATAL: return BRED;
    }
    return RESET;
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    if (file_.is_open()) file_.close();
}

void Logger::set_log_file(const std::filesystem::path& p) {
    std::lock_guard lock(mtx_);
    if (file_.is_open()) file_.close();
    file_.open(p, std::ios::app);
    if (!file_.is_open())
        std::cerr << "[nexus::Logger] Cannot open log file: " << p << "\n";
}

void Logger::log(LogLevel level, std::string_view msg,
                 std::string_view /*file*/, int /*line*/)
{
    if (level < min_level_) return;

    std::lock_guard lock(mtx_);
    const std::string ts = timestamp_now();

    if (!quiet_) {
        if (color_) {
            fmt::print("{}[{}]{} {} {}{}{}\n",
                level_color(level), level_name(level), RESET,
                GRAY, ts, RESET,
                msg);
        } else {
            fmt::print("[{}] {} {}\n", level_name(level), ts, msg);
        }
        std::cout.flush();
    }

    // File sink (plain text)
    if (file_.is_open()) {
        file_ << "[" << level_name(level) << "] " << ts << " " << msg << "\n";
        file_.flush();
    }

    // External sinks
    for (auto& s : sinks_) s(level, msg);

    if (level == LogLevel::FATAL) {
        std::cerr << BRED << "[FATAL] " << msg << RESET << "\n";
        std::exit(1);
    }
}

void Logger::step(std::string_view step_name, std::string_view detail) {
    if (quiet_) return;
    std::lock_guard lock(mtx_);
    if (color_) {
        if (detail.empty())
            fmt::print("{}▶ {}{}{}\n", BLUE, BOLD, step_name, RESET);
        else
            fmt::print("{}▶ {}{}{} {}{}{}\n",
                BLUE, BOLD, step_name, RESET,
                GRAY, detail, RESET);
    } else {
        fmt::print(">> {} {}\n", step_name, detail);
    }
    std::cout.flush();
}

void Logger::ok(std::string_view msg) {
    if (quiet_) return;
    std::lock_guard lock(mtx_);
    if (color_)
        fmt::print("{}  ✓ {}{}\n", GREEN, msg, RESET);
    else
        fmt::print("  [OK] {}\n", msg);
    std::cout.flush();
}

void Logger::fail(std::string_view msg) {
    std::lock_guard lock(mtx_);
    if (color_)
        fmt::print("{}  ✗ {}{}\n", RED, msg, RESET);
    else
        fmt::print("  [FAIL] {}\n", msg);
    std::cout.flush();
}

} // namespace nexus
