#pragma once

#include "lexer.hpp"
#include "ast.hpp"
#include <memory>

namespace lithium::js {

// ============================================================================
// Parser - Recursive descent parser for JavaScript (partial ES6)
// ============================================================================

class Parser {
public:
    using ErrorCallback = std::function<void(const String& message, usize line, usize column)>;

    Parser();

    // Parse a complete script
    [[nodiscard]] std::unique_ptr<Program> parse(const String& source);
    [[nodiscard]] std::unique_ptr<Program> parse(std::string_view source);

    // Parse a single expression
    [[nodiscard]] ExpressionPtr parse_expression(const String& source);

    // Error handling
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }
    [[nodiscard]] const std::vector<String>& errors() const { return m_errors; }
    [[nodiscard]] bool has_errors() const { return !m_errors.empty(); }

private:
    // Token handling
    [[nodiscard]] const Token& current_token() const;
    [[nodiscard]] const Token& peek_token() const;
    void advance();
    bool match(TokenType type);
    bool check(TokenType type) const;
    Token expect(TokenType type, const String& message);
    [[nodiscard]] bool at_end() const;

    // Error handling
    void error(const String& message);
    void error_at_current(const String& message);
    void synchronize();

    // Statement parsing
    StatementPtr parse_statement();
    StatementPtr parse_declaration();
    StatementPtr parse_variable_declaration();
    StatementPtr parse_function_declaration();
    StatementPtr parse_class_declaration();
    StatementPtr parse_block_statement();
    StatementPtr parse_if_statement();
    StatementPtr parse_while_statement();
    StatementPtr parse_do_while_statement();
    StatementPtr parse_for_statement();
    StatementPtr parse_switch_statement();
    StatementPtr parse_try_statement();
    StatementPtr parse_return_statement();
    StatementPtr parse_throw_statement();
    StatementPtr parse_break_statement();
    StatementPtr parse_continue_statement();
    StatementPtr parse_expression_statement();
    StatementPtr parse_labeled_statement();

    // Expression parsing (precedence climbing)
    ExpressionPtr parse_expression_impl();
    ExpressionPtr parse_assignment_expression();
    ExpressionPtr parse_conditional_expression();
    ExpressionPtr parse_binary_expression(int min_precedence);
    ExpressionPtr parse_unary_expression();
    ExpressionPtr parse_update_expression();
    ExpressionPtr parse_left_hand_side_expression();
    ExpressionPtr parse_call_expression();
    ExpressionPtr parse_member_expression();
    ExpressionPtr parse_new_expression();
    ExpressionPtr parse_primary_expression();

    // Specific expressions
    ExpressionPtr parse_array_expression();
    ExpressionPtr parse_object_expression();
    ExpressionPtr parse_function_expression();
    ExpressionPtr parse_arrow_function_expression(std::vector<PatternPtr> params);
    ExpressionPtr parse_class_expression();
    ExpressionPtr parse_template_literal();

    // Pattern parsing (destructuring)
    PatternPtr parse_pattern();
    PatternPtr parse_array_pattern();
    PatternPtr parse_object_pattern();
    PatternPtr parse_binding_element();

    // Helper methods
    [[nodiscard]] int get_binary_precedence(TokenType type) const;
    [[nodiscard]] BinaryExpression::Operator token_to_binary_op(TokenType type) const;
    [[nodiscard]] AssignmentExpression::Operator token_to_assignment_op(TokenType type) const;
    [[nodiscard]] bool is_assignment_operator(TokenType type) const;
    [[nodiscard]] bool is_declaration_start() const;
    [[nodiscard]] bool could_be_arrow_function() const;

    std::vector<PatternPtr> parse_formal_parameters();
    std::vector<ExpressionPtr> parse_arguments();
    std::unique_ptr<Property> parse_property_definition();

    // State
    Lexer m_lexer;
    Token m_current;
    Token m_previous;
    std::vector<String> m_errors;
    ErrorCallback m_error_callback;

    // Parsing context
    bool m_allow_in{true};
    bool m_allow_yield{false};
    bool m_allow_await{false};
    bool m_in_function{false};
    bool m_in_loop{false};
    bool m_in_switch{false};
};

// ============================================================================
// Convenience functions
// ============================================================================

[[nodiscard]] std::unique_ptr<Program> parse_script(const String& source);
[[nodiscard]] std::unique_ptr<Program> parse_script(std::string_view source);

} // namespace lithium::js
