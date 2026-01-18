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

struct Diagnostic {
    DiagnosticStage stage{DiagnosticStage::VM};
    DiagnosticLevel level{DiagnosticLevel::Error};
    String message;
    String file;
    usize line{0};
    usize column{0};
};

class DiagnosticSink {
public:
    void add(DiagnosticStage stage,
             DiagnosticLevel level,
             String message,
             String file = ""_s,
             usize line = 0,
             usize column = 0) {
        Diagnostic d;
        d.stage = stage;
        d.level = level;
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
