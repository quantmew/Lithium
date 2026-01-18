#pragma once

#include "lithium/core/string.hpp"
#include <vector>

namespace lithium::js {

enum class DiagnosticStage {
    Lexer,
    Parser,
    Compiler,
    Runtime,
    VM,
};

enum class DiagnosticLevel {
    Info,
    Warning,
    Error,
};

// JavaScript error types (matching V8/ES spec)
enum class ErrorType {
    None,           // Not a runtime error
    Error,          // Generic Error
    SyntaxError,    // Syntax errors
    ReferenceError, // Undefined variable access
    TypeError,      // Type-related errors (calling non-function, etc.)
    RangeError,     // Out of range errors
    URIError,       // URI handling errors
};

struct StackFrame {
    String function_name;
    String file;
    usize line{0};
    usize column{0};
};

struct Diagnostic {
    DiagnosticStage stage{DiagnosticStage::VM};
    DiagnosticLevel level{DiagnosticLevel::Error};
    ErrorType error_type{ErrorType::None};
    String message;
    String file;
    usize line{0};
    usize column{0};
    String source_line;  // The actual line of source code
    std::vector<StackFrame> stack_trace;
};

class DiagnosticSink {
public:
    void add(DiagnosticStage stage,
             DiagnosticLevel level,
             String message,
             String file = ""_s,
             usize line = 0,
             usize column = 0,
             ErrorType error_type = ErrorType::None) {
        Diagnostic d;
        d.stage = stage;
        d.level = level;
        d.error_type = error_type;
        d.message = std::move(message);
        d.file = std::move(file);
        d.line = line;
        d.column = column;
        m_diags.push_back(std::move(d));
    }

    void add(Diagnostic diag) {
        m_diags.push_back(std::move(diag));
    }

    void clear() { m_diags.clear(); }

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const { return m_diags; }

    [[nodiscard]] bool has_errors() const {
        for (const auto& d : m_diags) {
            if (d.level == DiagnosticLevel::Error) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<Diagnostic> m_diags;
};

} // namespace lithium::js
