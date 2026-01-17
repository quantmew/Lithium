/**
 * JavaScript Compiler - core entry points and error handling
 */

#include "lithium/js/compiler.hpp"
#include "lithium/js/parser.hpp"

namespace lithium::js {

Compiler::Compiler() = default;

std::shared_ptr<Function> Compiler::compile(const Program& program) {
    m_errors.clear();
    m_had_error = false;
    m_panic_mode = false;

    // Create top-level function
    auto function = std::make_shared<Function>(""_s, 0);
    CompilerState state;
    state.function = function;
    state.type = CompilerState::Type::Script;
    m_current = &state;

    // Add implicit local for script (slot 0)
    state.locals.push_back({"", 0, false, false});

    // Compile all statements
    for (const auto& stmt : program.body) {
        compile_statement(*stmt);
        if (m_had_error && m_panic_mode) {
            break;
        }
    }

    // Emit return
    emit(OpCode::LoadUndefined);
    emit(OpCode::Return);

    m_current = nullptr;

    if (m_had_error) {
        return nullptr;
    }

    function->set_local_count(static_cast<u8>(state.locals.size()));
    return function;
}

std::shared_ptr<Function> Compiler::compile_expression(const Expression& expr) {
    m_errors.clear();
    m_had_error = false;

    auto function = std::make_shared<Function>("(expr)"_s, 0);
    CompilerState state;
    state.function = function;
    state.type = CompilerState::Type::Script;
    m_current = &state;

    state.locals.push_back({"", 0, false, false});

    compile_expression(expr);
    emit(OpCode::Return);

    m_current = nullptr;

    if (m_had_error) {
        return nullptr;
    }

    function->set_local_count(static_cast<u8>(state.locals.size()));
    return function;
}

void Compiler::error(const String& message) {
    if (m_panic_mode) return;
    m_panic_mode = true;
    m_had_error = true;
    m_errors.push_back(message);
    if (m_error_callback) {
        m_error_callback(message, 0, 0);
    }
}

void Compiler::error_at(const SourceLocation& loc, const String& message) {
    if (m_panic_mode) return;
    m_panic_mode = true;
    m_had_error = true;
    m_errors.push_back(message);
    if (m_error_callback) {
        m_error_callback(message, loc.line, loc.column);
    }
}

std::shared_ptr<Function> compile(const String& source) {
    Parser parser;
    parser.set_input(source);
    auto program = parser.parse_program();

    if (parser.has_errors()) {
        return nullptr;
    }

    Compiler compiler;
    return compiler.compile(*program);
}

std::shared_ptr<Function> compile(const Program& program) {
    Compiler compiler;
    return compiler.compile(program);
}

} // namespace lithium::js
