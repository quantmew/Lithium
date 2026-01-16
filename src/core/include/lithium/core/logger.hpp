#pragma once

#include "types.hpp"
#include "string.hpp"
#include <string_view>
#include <functional>
#include <source_location>
#include <chrono>
#include <format>

namespace lithium {

// ============================================================================
// Log levels
// ============================================================================

enum class LogLevel : u8 {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5,
    Off = 6
};

[[nodiscard]] constexpr std::string_view log_level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        case LogLevel::Off:   return "OFF";
    }
    return "UNKNOWN";
}

// ============================================================================
// Log record
// ============================================================================

struct LogRecord {
    LogLevel level;
    std::string_view message;
    std::string_view logger_name;
    std::source_location location;
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Log sink interface
// ============================================================================

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(const LogRecord& record) = 0;
    virtual void flush() = 0;
};

// Console sink (writes to stdout/stderr)
class ConsoleSink : public LogSink {
public:
    ConsoleSink(bool use_colors = true);
    void write(const LogRecord& record) override;
    void flush() override;

private:
    bool m_use_colors;
};

// File sink
class FileSink : public LogSink {
public:
    explicit FileSink(const char* filename);
    ~FileSink() override;

    void write(const LogRecord& record) override;
    void flush() override;

private:
    void* m_file{nullptr};
};

// ============================================================================
// Logger
// ============================================================================

class Logger {
public:
    explicit Logger(std::string_view name);

    // Log methods
    template<typename... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Trace, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Debug, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args,
              std::source_location loc = std::source_location::current()) {
        log(LogLevel::Info, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args,
              std::source_location loc = std::source_location::current()) {
        log(LogLevel::Warn, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Error, fmt, std::forward<Args>(args)..., loc);
    }

    template<typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Fatal, fmt, std::forward<Args>(args)..., loc);
    }

    // Simple string logging
    void trace(std::string_view msg, std::source_location loc = std::source_location::current());
    void debug(std::string_view msg, std::source_location loc = std::source_location::current());
    void info(std::string_view msg, std::source_location loc = std::source_location::current());
    void warn(std::string_view msg, std::source_location loc = std::source_location::current());
    void error(std::string_view msg, std::source_location loc = std::source_location::current());
    void fatal(std::string_view msg, std::source_location loc = std::source_location::current());

    // Configuration
    void set_level(LogLevel level) { m_level = level; }
    [[nodiscard]] LogLevel level() const { return m_level; }
    [[nodiscard]] std::string_view name() const { return m_name; }

    [[nodiscard]] bool is_enabled(LogLevel level) const {
        return level >= m_level;
    }

private:
    template<typename... Args>
    void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args,
             std::source_location loc) {
        if (!is_enabled(level)) return;
        std::string message = std::format(fmt, std::forward<Args>(args)...);
        log_impl(level, message, loc);
    }

    void log_impl(LogLevel level, std::string_view message, std::source_location loc);

    std::string m_name;
    LogLevel m_level{LogLevel::Info};
};

// ============================================================================
// Global logging configuration
// ============================================================================

namespace logging {

// Initialize logging system with default console sink
void init();

// Initialize with custom sinks
void init(std::vector<std::unique_ptr<LogSink>> sinks);

// Shutdown logging
void shutdown();

// Add a sink
void add_sink(std::unique_ptr<LogSink> sink);

// Set global minimum level
void set_level(LogLevel level);

// Get global level
[[nodiscard]] LogLevel level();

// Get or create a logger
[[nodiscard]] Logger& get(std::string_view name);

// Default logger
[[nodiscard]] Logger& default_logger();

// Flush all sinks
void flush();

} // namespace logging

// ============================================================================
// Convenience macros
// ============================================================================

#define LITHIUM_LOG_TRACE(...) ::lithium::logging::default_logger().trace(__VA_ARGS__)
#define LITHIUM_LOG_DEBUG(...) ::lithium::logging::default_logger().debug(__VA_ARGS__)
#define LITHIUM_LOG_INFO(...)  ::lithium::logging::default_logger().info(__VA_ARGS__)
#define LITHIUM_LOG_WARN(...)  ::lithium::logging::default_logger().warn(__VA_ARGS__)
#define LITHIUM_LOG_ERROR(...) ::lithium::logging::default_logger().error(__VA_ARGS__)
#define LITHIUM_LOG_FATAL(...) ::lithium::logging::default_logger().fatal(__VA_ARGS__)

// Debug-only logging
#ifdef NDEBUG
    #define LITHIUM_DEBUG_LOG(...) ((void)0)
#else
    #define LITHIUM_DEBUG_LOG(...) LITHIUM_LOG_DEBUG(__VA_ARGS__)
#endif

// Assertions
#define LITHIUM_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            LITHIUM_LOG_FATAL("Assertion failed: " #condition " - " __VA_ARGS__); \
            std::abort(); \
        } \
    } while (false)

#define LITHIUM_UNREACHABLE() \
    do { \
        LITHIUM_LOG_FATAL("Unreachable code reached"); \
        std::abort(); \
    } while (false)

} // namespace lithium
