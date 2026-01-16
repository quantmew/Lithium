#include "lithium/core/logger.hpp"
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <cstdio>
#include <iomanip>

namespace lithium {

// ============================================================================
// Global state
// ============================================================================

namespace {

struct LoggingState {
    std::mutex mutex;
    std::vector<std::unique_ptr<LogSink>> sinks;
    std::unordered_map<std::string, std::unique_ptr<Logger>> loggers;
    std::unique_ptr<Logger> default_logger;
    LogLevel global_level{LogLevel::Info};
    bool initialized{false};
};

LoggingState& state() {
    static LoggingState s;
    return s;
}

std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &time);
#else
    localtime_r(&time, &tm_buf);
#endif

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_buf);

    char result[64];
    std::snprintf(result, sizeof(result), "%s.%03d", buffer, static_cast<int>(ms.count()));
    return result;
}

} // anonymous namespace

// ============================================================================
// ConsoleSink implementation
// ============================================================================

ConsoleSink::ConsoleSink(bool use_colors) : m_use_colors(use_colors) {}

void ConsoleSink::write(const LogRecord& record) {
    const char* color_start = "";
    const char* color_end = "";

    if (m_use_colors) {
        switch (record.level) {
            case LogLevel::Trace: color_start = "\033[90m"; break;  // Gray
            case LogLevel::Debug: color_start = "\033[36m"; break;  // Cyan
            case LogLevel::Info:  color_start = "\033[32m"; break;  // Green
            case LogLevel::Warn:  color_start = "\033[33m"; break;  // Yellow
            case LogLevel::Error: color_start = "\033[31m"; break;  // Red
            case LogLevel::Fatal: color_start = "\033[35m"; break;  // Magenta
            default: break;
        }
        color_end = "\033[0m";
    }

    auto timestamp = format_timestamp(record.timestamp);
    auto level_name = log_level_name(record.level);

    // Format: [timestamp] [LEVEL] [logger] message (file:line)
    std::ostream& out = (record.level >= LogLevel::Error) ? std::cerr : std::cout;

    out << "[" << timestamp << "] "
        << color_start << "[" << level_name << "]" << color_end << " ";

    if (!record.logger_name.empty()) {
        out << "[" << record.logger_name << "] ";
    }

    out << record.message;

    // Add source location for debug/trace
    if (record.level <= LogLevel::Debug) {
        out << " (" << record.location.file_name()
            << ":" << record.location.line() << ")";
    }

    out << "\n";
}

void ConsoleSink::flush() {
    std::cout.flush();
    std::cerr.flush();
}

// ============================================================================
// FileSink implementation
// ============================================================================

FileSink::FileSink(const char* filename) {
    m_file = std::fopen(filename, "a");
}

FileSink::~FileSink() {
    if (m_file) {
        std::fclose(static_cast<std::FILE*>(m_file));
    }
}

void FileSink::write(const LogRecord& record) {
    if (!m_file) return;

    auto* file = static_cast<std::FILE*>(m_file);
    auto timestamp = format_timestamp(record.timestamp);
    auto level_name = log_level_name(record.level);

    std::fprintf(file, "[%s] [%.*s] ",
                 timestamp.c_str(),
                 static_cast<int>(level_name.size()), level_name.data());

    if (!record.logger_name.empty()) {
        std::fprintf(file, "[%.*s] ",
                     static_cast<int>(record.logger_name.size()),
                     record.logger_name.data());
    }

    std::fprintf(file, "%.*s (%s:%u)\n",
                 static_cast<int>(record.message.size()), record.message.data(),
                 record.location.file_name(),
                 record.location.line());
}

void FileSink::flush() {
    if (m_file) {
        std::fflush(static_cast<std::FILE*>(m_file));
    }
}

// ============================================================================
// Logger implementation
// ============================================================================

Logger::Logger(std::string_view name) : m_name(name) {}

void Logger::trace(std::string_view msg, std::source_location loc) {
    log_impl(LogLevel::Trace, msg, loc);
}

void Logger::debug(std::string_view msg, std::source_location loc) {
    log_impl(LogLevel::Debug, msg, loc);
}

void Logger::info(std::string_view msg, std::source_location loc) {
    log_impl(LogLevel::Info, msg, loc);
}

void Logger::warn(std::string_view msg, std::source_location loc) {
    log_impl(LogLevel::Warn, msg, loc);
}

void Logger::error(std::string_view msg, std::source_location loc) {
    log_impl(LogLevel::Error, msg, loc);
}

void Logger::fatal(std::string_view msg, std::source_location loc) {
    log_impl(LogLevel::Fatal, msg, loc);
}

void Logger::log_impl(LogLevel level, std::string_view message, std::source_location loc) {
    if (!is_enabled(level)) return;

    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (level < s.global_level) return;

    LogRecord record{
        .level = level,
        .message = message,
        .logger_name = m_name,
        .location = loc,
        .timestamp = std::chrono::system_clock::now()
    };

    for (auto& sink : s.sinks) {
        sink->write(record);
    }
}

// ============================================================================
// Global logging functions
// ============================================================================

namespace logging {

void init() {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (s.initialized) return;

    s.sinks.push_back(std::make_unique<ConsoleSink>());
    s.default_logger = std::make_unique<Logger>("lithium");
    s.initialized = true;
}

void init(std::vector<std::unique_ptr<LogSink>> sinks) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    if (s.initialized) return;

    s.sinks = std::move(sinks);
    s.default_logger = std::make_unique<Logger>("lithium");
    s.initialized = true;
}

void shutdown() {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    flush();
    s.sinks.clear();
    s.loggers.clear();
    s.default_logger.reset();
    s.initialized = false;
}

void add_sink(std::unique_ptr<LogSink> sink) {
    auto& s = state();
    std::lock_guard lock(s.mutex);
    s.sinks.push_back(std::move(sink));
}

void set_level(LogLevel level) {
    auto& s = state();
    s.global_level = level;
}

LogLevel level() {
    return state().global_level;
}

Logger& get(std::string_view name) {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    std::string name_str(name);
    auto it = s.loggers.find(name_str);
    if (it != s.loggers.end()) {
        return *it->second;
    }

    auto logger = std::make_unique<Logger>(name);
    auto& ref = *logger;
    s.loggers.emplace(std::move(name_str), std::move(logger));
    return ref;
}

Logger& default_logger() {
    auto& s = state();

    // Lazy initialization
    if (!s.initialized) {
        init();
    }

    return *s.default_logger;
}

void flush() {
    auto& s = state();
    std::lock_guard lock(s.mutex);

    for (auto& sink : s.sinks) {
        sink->flush();
    }
}

} // namespace logging

} // namespace lithium
