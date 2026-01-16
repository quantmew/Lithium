#pragma once

#include "types.hpp"
#include "string.hpp"
#include <string_view>
#include <functional>
#include <chrono>
#include <sstream>
#include <vector>
#include <memory>

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
// Source location (GCC 9 compatible)
// ============================================================================

struct SourceLocation {
    const char* file;
    int line;
    const char* function;

    static SourceLocation current(const char* file = __builtin_FILE(),
                                   int line = __builtin_LINE(),
                                   const char* func = __builtin_FUNCTION()) {
        return {file, line, func};
    }

    [[nodiscard]] const char* file_name() const { return file; }
    [[nodiscard]] int line_number() const { return line; }
    [[nodiscard]] const char* function_name() const { return function; }
};

// ============================================================================
// Log record
// ============================================================================

struct LogRecord {
    LogLevel level;
    std::string_view message;
    std::string_view logger_name;
    SourceLocation location;
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
    explicit ConsoleSink(bool use_colors = true);
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

    // Simple string logging
    void trace(std::string_view msg, SourceLocation loc = SourceLocation::current());
    void debug(std::string_view msg, SourceLocation loc = SourceLocation::current());
    void info(std::string_view msg, SourceLocation loc = SourceLocation::current());
    void warn(std::string_view msg, SourceLocation loc = SourceLocation::current());
    void error(std::string_view msg, SourceLocation loc = SourceLocation::current());
    void fatal(std::string_view msg, SourceLocation loc = SourceLocation::current());

    // Configuration
    void set_level(LogLevel level) { m_level = level; }
    [[nodiscard]] LogLevel level() const { return m_level; }
    [[nodiscard]] std::string_view name() const { return m_name; }

    [[nodiscard]] bool is_enabled(LogLevel level) const {
        return level >= m_level;
    }

private:
    void log_impl(LogLevel level, std::string_view message, SourceLocation loc);

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

#define LITHIUM_LOG_TRACE(msg) ::lithium::logging::default_logger().trace(msg)
#define LITHIUM_LOG_DEBUG(msg) ::lithium::logging::default_logger().debug(msg)
#define LITHIUM_LOG_INFO(msg)  ::lithium::logging::default_logger().info(msg)
#define LITHIUM_LOG_WARN(msg)  ::lithium::logging::default_logger().warn(msg)
#define LITHIUM_LOG_ERROR(msg) ::lithium::logging::default_logger().error(msg)
#define LITHIUM_LOG_FATAL(msg) ::lithium::logging::default_logger().fatal(msg)

// Debug-only logging
#ifdef NDEBUG
    #define LITHIUM_DEBUG_LOG(msg) ((void)0)
#else
    #define LITHIUM_DEBUG_LOG(msg) LITHIUM_LOG_DEBUG(msg)
#endif

// Assertions
#define LITHIUM_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            LITHIUM_LOG_FATAL("Assertion failed: " #condition " - " msg); \
            std::abort(); \
        } \
    } while (false)

#define LITHIUM_UNREACHABLE() \
    do { \
        LITHIUM_LOG_FATAL("Unreachable code reached"); \
        std::abort(); \
    } while (false)

} // namespace lithium
