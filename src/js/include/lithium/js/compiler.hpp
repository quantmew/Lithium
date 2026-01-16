#pragma once

#include "ast.hpp"
#include "bytecode.hpp"
#include <memory>
#include <unordered_map>

namespace lithium::js {

// ============================================================================
// Compiler - Compiles AST to bytecode
// ============================================================================

class Compiler {
public:
    using ErrorCallback = std::function<void(const String& message, usize line, usize column)>;

    Compiler();

    // Compile a program
    [[nodiscard]] std::shared_ptr<Function> compile(const Program& program);

    // Compile a single expression (for REPL)
    [[nodiscard]] std::shared_ptr<Function> compile_expression(const Expression& expr);

    // Error handling
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }
    [[nodiscard]] const std::vector<String>& errors() const { return m_errors; }
    [[nodiscard]] bool has_errors() const { return !m_errors.empty(); }

private:
    // Compilation context
    struct Local {
        String name;
        i32 depth;
        bool is_captured{false};
        bool is_const{false};
    };

    struct CompilerState {
        CompilerState* enclosing{nullptr};
        std::shared_ptr<Function> function;
        std::vector<Local> locals;
        std::vector<Upvalue> upvalues;
        i32 scope_depth{0};
        bool is_initializer{false};

        enum class Type { Script, Function, Method, Initializer } type{Type::Script};
    };

    // Current state
    CompilerState* m_current{nullptr};

    // Error handling
    std::vector<String> m_errors;
    ErrorCallback m_error_callback;
    bool m_had_error{false};
    bool m_panic_mode{false};

    // Helpers
    Chunk& current_chunk() { return m_current->function->chunk(); }
    void emit(OpCode op);
    void emit(u8 byte);
    void emit_u16(u16 value);
    void emit_constant(f64 value);
    void emit_constant(const String& value);
    usize emit_jump(OpCode op);
    void patch_jump(usize offset);
    void emit_loop(usize loop_start);
    u16 make_constant(const String& value);

    // Scope management
    void begin_scope();
    void end_scope();
    void declare_variable(const String& name, bool is_const = false);
    void define_variable(u16 global_idx);
    u16 identifier_constant(const String& name);
    i32 resolve_local(CompilerState* state, const String& name);
    i32 resolve_upvalue(CompilerState* state, const String& name);
    void add_local(const String& name, bool is_const = false);
    void mark_initialized();

    // Statement compilation
    void compile_statement(const Statement& stmt);
    void compile_expression_statement(const ExpressionStatement& stmt);
    void compile_block_statement(const BlockStatement& stmt);
    void compile_variable_declaration(const VariableDeclaration& decl);
    void compile_function_declaration(const FunctionDeclaration& decl);
    void compile_class_declaration(const ClassDeclaration& decl);
    void compile_if_statement(const IfStatement& stmt);
    void compile_while_statement(const WhileStatement& stmt);
    void compile_do_while_statement(const DoWhileStatement& stmt);
    void compile_for_statement(const ForStatement& stmt);
    void compile_for_in_statement(const ForInStatement& stmt);
    void compile_for_of_statement(const ForOfStatement& stmt);
    void compile_switch_statement(const SwitchStatement& stmt);
    void compile_try_statement(const TryStatement& stmt);
    void compile_return_statement(const ReturnStatement& stmt);
    void compile_throw_statement(const ThrowStatement& stmt);
    void compile_break_statement(const BreakStatement& stmt);
    void compile_continue_statement(const ContinueStatement& stmt);

    // Expression compilation
    void compile_expression(const Expression& expr);
    void compile_literal(const NullLiteral& lit);
    void compile_literal(const BooleanLiteral& lit);
    void compile_literal(const NumericLiteral& lit);
    void compile_literal(const StringLiteral& lit);
    void compile_template_literal(const TemplateLiteral& lit);
    void compile_identifier(const Identifier& id);
    void compile_this_expression(const ThisExpression& expr);
    void compile_array_expression(const ArrayExpression& expr);
    void compile_object_expression(const ObjectExpression& expr);
    void compile_function_expression(const FunctionExpression& expr);
    void compile_arrow_function(const ArrowFunctionExpression& expr);
    void compile_class_expression(const ClassExpression& expr);
    void compile_member_expression(const MemberExpression& expr);
    void compile_call_expression(const CallExpression& expr);
    void compile_new_expression(const NewExpression& expr);
    void compile_unary_expression(const UnaryExpression& expr);
    void compile_update_expression(const UpdateExpression& expr);
    void compile_binary_expression(const BinaryExpression& expr);
    void compile_logical_expression(const LogicalExpression& expr);
    void compile_conditional_expression(const ConditionalExpression& expr);
    void compile_assignment_expression(const AssignmentExpression& expr);
    void compile_sequence_expression(const SequenceExpression& expr);

    // Pattern compilation (destructuring)
    void compile_pattern_assignment(const Pattern& pattern);
    void compile_array_pattern(const ArrayPattern& pattern);
    void compile_object_pattern(const ObjectPattern& pattern);

    // Function compilation
    void compile_function_body(const String& name, const std::vector<PatternPtr>& params,
                               const Statement& body, CompilerState::Type type);

    // Error reporting
    void error(const String& message);
    void error_at(const SourceLocation& loc, const String& message);

    // Break/continue tracking
    struct LoopContext {
        usize loop_start;
        std::vector<usize> break_jumps;
        std::vector<usize> continue_jumps;
        i32 scope_depth;
    };
    std::vector<LoopContext> m_loop_stack;

    void push_loop(usize start);
    void pop_loop();
    void emit_break();
    void emit_continue();
};

// ============================================================================
// Convenience functions
// ============================================================================

[[nodiscard]] std::shared_ptr<Function> compile(const String& source);
[[nodiscard]] std::shared_ptr<Function> compile(const Program& program);

} // namespace lithium::js
