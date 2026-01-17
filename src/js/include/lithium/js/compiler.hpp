#pragma once

#include "lithium/js/ast.hpp"
#include "lithium/js/bytecode.hpp"

namespace lithium::js {

// ============================================================================
// Compiler: AST -> bytecode Module
// ============================================================================

class Compiler {
public:
    struct Result {
        ModuleBytecode module;
        std::vector<String> errors;
    };

    [[nodiscard]] Result compile(const Program& program);
};

} // namespace lithium::js
