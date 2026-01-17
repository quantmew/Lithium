#pragma once

#include "ast.hpp"
#include "lexer.hpp"
#include <memory>
#include <optional>

namespace lithium::js {

// ============================================================================
// Parser - recursive descent for a practical ES subset
// ============================================================================

class Parser {
public:
    using ErrorCallback = std::function<void(const String& message, usize line, usize column)>;

    Parser();

    [[nodiscard]] std::unique_ptr<Program> parse(const String& source);
    [[nodiscard]] std::unique_ptr<Program> parse(std::string_view source);

    [[nodiscard]] ExpressionPtr parse_expression(const String& source);

    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }
    [[nodiscard]] const std::vector<String>& errors() const { return m_errors; }
    [[nodiscard]] bool has_errors() const { return !m_errors.empty(); }

private:
    // Token utilities
    [[nodiscard]] const Token& current_token() const;
    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    Token expect(TokenType type, const String& message);
    [[nodiscard]] bool at_end() const;

    // Error handling
    void error(const String& message);
    void synchronize();

    // Statements
    StatementPtr parse_statement();
    StatementPtr parse_class_declaration();
    StatementPtr parse_import_declaration();
    StatementPtr parse_export_declaration();
    StatementPtr parse_block_statement();
    StatementPtr parse_variable_statement(VariableDeclaration::Kind kind);
    StatementPtr parse_function_declaration();
    StatementPtr parse_return_statement();
    StatementPtr parse_break_statement();
    StatementPtr parse_continue_statement();
    StatementPtr parse_if_statement();
    StatementPtr parse_while_statement();
    StatementPtr parse_do_while_statement();
    StatementPtr parse_for_statement();
    StatementPtr parse_switch_statement();
    StatementPtr parse_try_statement();
    StatementPtr parse_throw_statement();
    StatementPtr parse_with_statement();
    StatementPtr parse_expression_statement();

    // Expressions (precedence)
    ExpressionPtr parse_expression_impl();
    ExpressionPtr parse_conditional_expression();
    ExpressionPtr parse_assignment_expression();
    ExpressionPtr parse_logical_or_expression();
    ExpressionPtr parse_logical_and_expression();
    ExpressionPtr parse_nullish_expression();
    ExpressionPtr parse_bitwise_or_expression();
    ExpressionPtr parse_bitwise_xor_expression();
    ExpressionPtr parse_bitwise_and_expression();
    ExpressionPtr parse_equality_expression();
    ExpressionPtr parse_relational_expression(bool allow_in = true);
    ExpressionPtr parse_shift_expression(bool allow_in = true);
    ExpressionPtr parse_additive_expression();
    ExpressionPtr parse_multiplicative_expression();
    ExpressionPtr parse_exponentiation_expression();
    ExpressionPtr parse_unary_expression();
    ExpressionPtr parse_update_expression();
    ExpressionPtr parse_new_expression();
    ExpressionPtr parse_call_member_expression();
    ExpressionPtr parse_primary_expression();
    ExpressionPtr parse_template_literal(const Token& head);

    // Helpers
    [[nodiscard]] bool is_assignment_operator(TokenType type) const;
    AssignmentExpression::Operator to_assignment_operator(TokenType type) const;
    [[nodiscard]] std::optional<BinaryExpression::Operator> binary_operator_for(TokenType type) const;
    [[nodiscard]] std::optional<LogicalExpression::Operator> logical_operator_for(TokenType type) const;
    void consume_semicolon_if_present();
    [[nodiscard]] bool contains_nullish_coalescing(const Expression* expr) const;
    [[nodiscard]] bool allows_regexp_after(const Token& token) const;

    // Composite literals / functions
    ExpressionPtr parse_array_expression();
    ExpressionPtr parse_object_expression();
    ExpressionPtr parse_function_expression(bool is_arrow = false, std::vector<String> params = {});
    ExpressionPtr parse_arrow_function_after_params(std::vector<String> params, bool single_param);

    Lexer m_lexer;
    Token m_current;
    Token m_previous;
    std::vector<String> m_errors;
    ErrorCallback m_error_callback;

    bool m_in_function{false};
    bool m_allow_regexp{true};
};

// Convenience helpers
[[nodiscard]] std::unique_ptr<Program> parse_script(const String& source);
[[nodiscard]] std::unique_ptr<Program> parse_script(std::string_view source);

} // namespace lithium::js
