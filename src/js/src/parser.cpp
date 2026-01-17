/**
 * JavaScript Parser implementation
 */

#include "lithium/js/parser.hpp"

namespace lithium::js {

Parser::Parser() = default;

std::unique_ptr<Program> Parser::parse(const String& source) {
    m_errors.clear();
    m_lexer.set_input(source);
    advance();

    auto program = std::make_unique<Program>();
    while (!at_end()) {
        program->body.push_back(parse_statement());
    }
    return program;
}

std::unique_ptr<Program> Parser::parse(std::string_view source) {
    m_errors.clear();
    m_lexer.set_input(source);
    advance();

    auto program = std::make_unique<Program>();
    while (!at_end()) {
        program->body.push_back(parse_statement());
    }
    return program;
}

ExpressionPtr Parser::parse_expression(const String& source) {
    m_errors.clear();
    m_lexer.set_input(source);
    advance();
    return parse_expression_impl();
}

const Token& Parser::current_token() const {
    return m_current;
}

void Parser::advance() {
    m_previous = m_current;
    m_lexer.set_allow_regexp(allows_regexp_after(m_previous));
    m_current = m_lexer.next_token();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenType type) const {
    return m_current.type == type;
}

Token Parser::expect(TokenType type, const String& message) {
    if (!check(type)) {
        error(message);
        return m_current;
    }
    advance();
    return m_previous;
}

bool Parser::at_end() const {
    return m_current.type == TokenType::EndOfFile;
}

void Parser::error(const String& message) {
    m_errors.push_back(message);
    if (m_error_callback) {
        m_error_callback(message, m_current.line, m_current.column);
    }
}

void Parser::synchronize() {
    advance();
    while (!at_end()) {
        if (m_previous.type == TokenType::Semicolon) return;

        switch (m_current.type) {
            case TokenType::Function:
            case TokenType::Var:
            case TokenType::Let:
            case TokenType::Const:
            case TokenType::If:
            case TokenType::While:
            case TokenType::Return:
                return;
            default:
                break;
        }
        advance();
    }
}

// ============================================================================
// Statements
// ============================================================================

StatementPtr Parser::parse_statement() {
    if (match(TokenType::OpenBrace)) {
        return parse_block_statement();
    }
    if (match(TokenType::Import)) {
        return parse_import_declaration();
    }
    if (match(TokenType::Export)) {
        return parse_export_declaration();
    }
    if (match(TokenType::Class)) {
        return parse_class_declaration();
    }
    if (match(TokenType::Function)) {
        return parse_function_declaration();
    }
    if (match(TokenType::For)) {
        return parse_for_statement();
    }
    if (match(TokenType::Do)) {
        return parse_do_while_statement();
    }
    if (match(TokenType::Var)) {
        return parse_variable_statement(VariableDeclaration::Kind::Var);
    }
    if (match(TokenType::Let)) {
        return parse_variable_statement(VariableDeclaration::Kind::Let);
    }
    if (match(TokenType::Const)) {
        return parse_variable_statement(VariableDeclaration::Kind::Const);
    }
    if (match(TokenType::Return)) {
        return parse_return_statement();
    }
    if (match(TokenType::Break)) {
        return parse_break_statement();
    }
    if (match(TokenType::Continue)) {
        return parse_continue_statement();
    }
    if (match(TokenType::If)) {
        return parse_if_statement();
    }
    if (match(TokenType::While)) {
        return parse_while_statement();
    }
    if (match(TokenType::Switch)) {
        return parse_switch_statement();
    }
    if (match(TokenType::Try)) {
        return parse_try_statement();
    }
    if (match(TokenType::Throw)) {
        return parse_throw_statement();
    }
    if (match(TokenType::With)) {
        return parse_with_statement();
    }
    if (match(TokenType::Semicolon)) {
        return std::make_unique<EmptyStatement>();
    }

    return parse_expression_statement();
}

StatementPtr Parser::parse_block_statement() {
    auto block = std::make_unique<BlockStatement>();
    while (!check(TokenType::CloseBrace) && !at_end()) {
        block->body.push_back(parse_statement());
    }
    expect(TokenType::CloseBrace, "Expected '}' after block"_s);
    return block;
}

StatementPtr Parser::parse_class_declaration() {
    auto cls = std::make_unique<ClassDeclaration>();
    auto name = expect(TokenType::Identifier, "Expected class name"_s);
    cls->name = name.value;

    if (match(TokenType::Extends)) {
        cls->super_class = parse_expression_impl();
    }

    expect(TokenType::OpenBrace, "Expected '{' to start class body"_s);
    while (!check(TokenType::CloseBrace) && !at_end()) {
        bool is_static = false;
        if (check(TokenType::Identifier) && current_token().value == "static"_s) {
            advance();
            is_static = true;
        }

        Token method_name = expect(TokenType::Identifier, "Expected method name"_s);
        expect(TokenType::OpenParen, "Expected '(' after method name"_s);
        std::vector<String> params;
        if (!check(TokenType::CloseParen)) {
            do {
                auto param = expect(TokenType::Identifier, "Expected parameter name"_s);
                params.push_back(param.value);
            } while (match(TokenType::Comma));
        }
        expect(TokenType::CloseParen, "Expected ')' after parameters"_s);
        expect(TokenType::OpenBrace, "Expected '{' before method body"_s);
        std::vector<StatementPtr> body;
        while (!check(TokenType::CloseBrace) && !at_end()) {
            body.push_back(parse_statement());
        }
        expect(TokenType::CloseBrace, "Expected '}' after method body"_s);

        ClassMethod method;
        method.key = method_name.value;
        method.params = std::move(params);
        method.body = std::move(body);
        method.is_static = is_static;
        cls->body.push_back(std::move(method));
    }
    expect(TokenType::CloseBrace, "Expected '}' after class body"_s);
    return cls;
}

StatementPtr Parser::parse_import_declaration() {
    auto decl = std::make_unique<ImportDeclaration>();

    if (check(TokenType::String)) {
        auto source = expect(TokenType::String, "Expected module specifier"_s);
        decl->source = source.value;
        consume_semicolon_if_present();
        return decl;
    }

    if (check(TokenType::Identifier) && current_token().value != "from"_s) {
        auto def = expect(TokenType::Identifier, "Expected default import name"_s);
        ImportSpecifier spec;
        spec.imported = "default"_s;
        spec.local = def.value;
        spec.is_default = true;
        decl->specifiers.push_back(std::move(spec));
        match(TokenType::Comma);
    }

    if (match(TokenType::Star)) {
        if (check(TokenType::Identifier) && current_token().value == "as"_s) {
            advance();
            auto ns = expect(TokenType::Identifier, "Expected namespace identifier"_s);
            ImportSpecifier spec;
            spec.imported = "*"_s;
            spec.local = ns.value;
            spec.is_namespace = true;
            decl->specifiers.push_back(std::move(spec));
        } else {
            error("Expected 'as' after '*' in import"_s);
        }
    } else if (match(TokenType::OpenBrace)) {
        if (!check(TokenType::CloseBrace)) {
            do {
                Token imported = expect(TokenType::Identifier, "Expected imported binding name"_s);
                String imported_name = imported.value;
                String local_name = imported_name;
                if (check(TokenType::Identifier) && current_token().value == "as"_s) {
                    advance();
                    Token local_tok = expect(TokenType::Identifier, "Expected local name after 'as'"_s);
                    local_name = local_tok.value;
                }
                ImportSpecifier spec;
                spec.imported = imported_name;
                spec.local = local_name;
                decl->specifiers.push_back(std::move(spec));
            } while (match(TokenType::Comma));
        }
        expect(TokenType::CloseBrace, "Expected '}' after import specifiers"_s);
    }

    if (check(TokenType::Identifier) && current_token().value == "from"_s) {
        advance();
        auto source = expect(TokenType::String, "Expected module specifier after 'from'"_s);
        decl->source = source.value;
    } else {
        error("Expected 'from' in import statement"_s);
    }

    consume_semicolon_if_present();
    return decl;
}

StatementPtr Parser::parse_export_declaration() {
    if (match(TokenType::Default)) {
        auto decl = std::make_unique<ExportDefaultDeclaration>();
        if (check(TokenType::Function)) {
            advance();
            decl->declaration = parse_function_declaration();
        } else if (check(TokenType::Class)) {
            advance();
            decl->declaration = parse_class_declaration();
        } else {
            decl->expression = parse_assignment_expression();
            consume_semicolon_if_present();
        }
        return decl;
    }

    if (match(TokenType::Star)) {
        auto decl = std::make_unique<ExportAllDeclaration>();
        if (check(TokenType::Identifier) && current_token().value == "as"_s) {
            advance();
            auto alias = expect(TokenType::Identifier, "Expected identifier after 'as'"_s);
            decl->exported_as = alias.value;
        }
        if (check(TokenType::Identifier) && current_token().value == "from"_s) {
            advance();
            auto source = expect(TokenType::String, "Expected module specifier after 'from'"_s);
            decl->source = source.value;
        }
        consume_semicolon_if_present();
        return decl;
    }

    if (match(TokenType::OpenBrace)) {
        auto decl = std::make_unique<ExportNamedDeclaration>();
        if (!check(TokenType::CloseBrace)) {
            do {
                auto local = expect(TokenType::Identifier, "Expected exported name"_s);
                String exported = local.value;
                if (check(TokenType::Identifier) && current_token().value == "as"_s) {
                    advance();
                    auto alias = expect(TokenType::Identifier, "Expected export alias"_s);
                    exported = alias.value;
                }
                ExportSpecifier spec;
                spec.local = local.value;
                spec.exported = exported;
                decl->specifiers.push_back(std::move(spec));
            } while (match(TokenType::Comma));
        }
        expect(TokenType::CloseBrace, "Expected '}' after export list"_s);
        if (check(TokenType::Identifier) && current_token().value == "from"_s) {
            advance();
            auto source = expect(TokenType::String, "Expected module specifier after 'from'"_s);
            decl->source = source.value;
        }
        consume_semicolon_if_present();
        return decl;
    }

    auto decl = std::make_unique<ExportNamedDeclaration>();
    decl->declaration = parse_statement();
    return decl;
}

StatementPtr Parser::parse_variable_statement(VariableDeclaration::Kind kind) {
    auto decl = std::make_unique<VariableDeclaration>();
    decl->kind = kind;

    do {
        if (!check(TokenType::Identifier)) {
            error("Expected identifier in variable declaration"_s);
            break;
        }
        VariableDeclarator var;
        var.id = m_current.value;
        advance();
        if (match(TokenType::Assign)) {
            var.init = parse_assignment_expression();
        } else if (kind == VariableDeclaration::Kind::Const) {
            error("Const declarations require an initializer"_s);
        }
        decl->declarations.push_back(std::move(var));
    } while (match(TokenType::Comma));

    consume_semicolon_if_present();
    return decl;
}

StatementPtr Parser::parse_function_declaration() {
    auto fn = std::make_unique<FunctionDeclaration>();
    auto name = expect(TokenType::Identifier, "Expected function name"_s);
    fn->name = name.value;

    expect(TokenType::OpenParen, "Expected '(' after function name"_s);
    if (!check(TokenType::CloseParen)) {
        do {
            auto param = expect(TokenType::Identifier, "Expected parameter name"_s);
            fn->params.push_back(param.value);
        } while (match(TokenType::Comma));
    }
    expect(TokenType::CloseParen, "Expected ')' after parameters"_s);
    expect(TokenType::OpenBrace, "Expected '{' before function body"_s);

    while (!check(TokenType::CloseBrace) && !at_end()) {
        fn->body.push_back(parse_statement());
    }
    expect(TokenType::CloseBrace, "Expected '}' after function body"_s);
    return fn;
}

StatementPtr Parser::parse_return_statement() {
    auto stmt = std::make_unique<ReturnStatement>();

    if (check(TokenType::Semicolon) || current_token().preceded_by_line_terminator || check(TokenType::EndOfFile)) {
        consume_semicolon_if_present();
        return stmt;
    }

    stmt->argument = parse_expression_impl();
    consume_semicolon_if_present();
    return stmt;
}

StatementPtr Parser::parse_break_statement() {
    auto stmt = std::make_unique<BreakStatement>();
    if (!current_token().preceded_by_line_terminator && check(TokenType::Identifier)) {
        stmt->label = current_token().value;
        advance();
    }
    consume_semicolon_if_present();
    return stmt;
}

StatementPtr Parser::parse_continue_statement() {
    auto stmt = std::make_unique<ContinueStatement>();
    if (!current_token().preceded_by_line_terminator && check(TokenType::Identifier)) {
        stmt->label = current_token().value;
        advance();
    }
    consume_semicolon_if_present();
    return stmt;
}

StatementPtr Parser::parse_if_statement() {
    expect(TokenType::OpenParen, "Expected '(' after if"_s);
    auto test = parse_expression_impl();
    expect(TokenType::CloseParen, "Expected ')' after condition"_s);
    auto consequent = parse_statement();

    StatementPtr alternate;
    if (match(TokenType::Else)) {
        alternate = parse_statement();
    }

    auto stmt = std::make_unique<IfStatement>();
    stmt->test = std::move(test);
    stmt->consequent = std::move(consequent);
    stmt->alternate = std::move(alternate);
    return stmt;
}

StatementPtr Parser::parse_while_statement() {
    expect(TokenType::OpenParen, "Expected '(' after while"_s);
    auto test = parse_expression_impl();
    expect(TokenType::CloseParen, "Expected ')' after condition"_s);
    auto body = parse_statement();

    auto stmt = std::make_unique<WhileStatement>();
    stmt->test = std::move(test);
    stmt->body = std::move(body);
    return stmt;
}

StatementPtr Parser::parse_do_while_statement() {
    auto stmt = std::make_unique<DoWhileStatement>();
    stmt->body = parse_statement();
    expect(TokenType::While, "Expected 'while' after do-while body"_s);
    expect(TokenType::OpenParen, "Expected '(' after while"_s);
    stmt->test = parse_expression_impl();
    expect(TokenType::CloseParen, "Expected ')' after condition"_s);
    consume_semicolon_if_present();
    return stmt;
}

StatementPtr Parser::parse_for_statement() {
    expect(TokenType::OpenParen, "Expected '(' after for"_s);
    auto stmt = std::make_unique<ForStatement>();

    // init
    if (match(TokenType::Semicolon)) {
    } else if (match(TokenType::Var)) {
        stmt->init_statement = parse_variable_statement(VariableDeclaration::Kind::Var);
    } else if (match(TokenType::Let)) {
        stmt->init_statement = parse_variable_statement(VariableDeclaration::Kind::Let);
    } else if (match(TokenType::Const)) {
        stmt->init_statement = parse_variable_statement(VariableDeclaration::Kind::Const);
    } else {
        stmt->init_expression = parse_expression_impl();
        expect(TokenType::Semicolon, "Expected ';' after for init expression"_s);
    }

    // test
    if (!check(TokenType::Semicolon)) {
        stmt->test = parse_expression_impl();
    }
    expect(TokenType::Semicolon, "Expected ';' after for test"_s);

    // update
    if (!check(TokenType::CloseParen)) {
        stmt->update = parse_expression_impl();
    }
    expect(TokenType::CloseParen, "Expected ')' after for clauses"_s);

    stmt->body = parse_statement();
    return stmt;
}

StatementPtr Parser::parse_switch_statement() {
    expect(TokenType::OpenParen, "Expected '(' after switch"_s);
    auto discriminant = parse_expression_impl();
    expect(TokenType::CloseParen, "Expected ')' after discriminant"_s);
    expect(TokenType::OpenBrace, "Expected '{' to start switch"_s);

    auto stmt = std::make_unique<SwitchStatement>();
    stmt->discriminant = std::move(discriminant);

    while (!check(TokenType::CloseBrace) && !at_end()) {
        SwitchCase switch_case;
        if (match(TokenType::Case)) {
            switch_case.test = parse_expression_impl();
            expect(TokenType::Colon, "Expected ':' after case test"_s);
        } else if (match(TokenType::Default)) {
            switch_case.test = nullptr;
            expect(TokenType::Colon, "Expected ':' after default"_s);
        } else {
            error("Expected 'case' or 'default' in switch"_s);
            break;
        }

        while (!check(TokenType::Case) && !check(TokenType::Default) && !check(TokenType::CloseBrace) && !at_end()) {
            switch_case.consequent.push_back(parse_statement());
        }
        stmt->cases.push_back(std::move(switch_case));
    }

    expect(TokenType::CloseBrace, "Expected '}' after switch"_s);
    return stmt;
}

StatementPtr Parser::parse_try_statement() {
    auto stmt = std::make_unique<TryStatement>();
    expect(TokenType::OpenBrace, "Expected '{' after try"_s);
    stmt->block = parse_block_statement();

    if (match(TokenType::Catch)) {
        expect(TokenType::OpenParen, "Expected '(' after catch"_s);
        auto ident = expect(TokenType::Identifier, "Expected identifier after catch("_s);
        stmt->handler_param = ident.value;
        expect(TokenType::CloseParen, "Expected ')' after catch parameter"_s);
        expect(TokenType::OpenBrace, "Expected '{' after catch clause"_s);
        stmt->handler = parse_block_statement();
    }

    if (match(TokenType::Finally)) {
        expect(TokenType::OpenBrace, "Expected '{' after finally"_s);
        stmt->finalizer = parse_block_statement();
    }

    if (!stmt->handler && !stmt->finalizer) {
        error("Expected catch or finally after try"_s);
    }

    return stmt;
}

StatementPtr Parser::parse_throw_statement() {
    auto stmt = std::make_unique<ThrowStatement>();
    if (current_token().preceded_by_line_terminator) {
        error("Illegal newline after throw"_s);
        return stmt;
    }
    stmt->argument = parse_expression_impl();
    consume_semicolon_if_present();
    return stmt;
}

StatementPtr Parser::parse_with_statement() {
    auto stmt = std::make_unique<WithStatement>();
    expect(TokenType::OpenParen, "Expected '(' after with"_s);
    stmt->object = parse_expression_impl();
    expect(TokenType::CloseParen, "Expected ')' after with object"_s);
    stmt->body = parse_statement();
    return stmt;
}

StatementPtr Parser::parse_expression_statement() {
    auto expr = parse_expression_impl();
    consume_semicolon_if_present();
    auto stmt = std::make_unique<ExpressionStatement>();
    stmt->expression = std::move(expr);
    return stmt;
}

// ============================================================================
// Expressions
// ============================================================================

ExpressionPtr Parser::parse_expression_impl() {
    return parse_assignment_expression();
}

ExpressionPtr Parser::parse_assignment_expression() {
    auto left = parse_conditional_expression();

    if (is_assignment_operator(current_token().type)) {
        auto op = to_assignment_operator(current_token().type);
        advance();
        auto right = parse_assignment_expression();

        auto assign = std::make_unique<AssignmentExpression>();
        assign->op = op;
        assign->left = std::move(left);
        assign->right = std::move(right);
        return assign;
    }

    return left;
}

ExpressionPtr Parser::parse_conditional_expression() {
    auto expr = parse_logical_or_expression();
    if (match(TokenType::Question)) {
        auto consequent = parse_assignment_expression();
        expect(TokenType::Colon, "Expected ':' in conditional expression"_s);
        auto alternate = parse_assignment_expression();
        auto conditional = std::make_unique<ConditionalExpression>();
        conditional->test = std::move(expr);
        conditional->consequent = std::move(consequent);
        conditional->alternate = std::move(alternate);
        return conditional;
    }
    return expr;
}

ExpressionPtr Parser::parse_logical_or_expression() {
    auto expr = parse_logical_and_expression();

    while (match(TokenType::PipePipe)) {
        if (contains_nullish_coalescing(expr.get())) {
            error("Cannot mix '??' with '||' without parentheses"_s);
        }
        auto right = parse_logical_and_expression();
        if (contains_nullish_coalescing(right.get())) {
            error("Cannot mix '??' with '||' without parentheses"_s);
        }
        auto logical = std::make_unique<LogicalExpression>();
        logical->op = LogicalExpression::Operator::Or;
        logical->left = std::move(expr);
        logical->right = std::move(right);
        expr = std::move(logical);
    }
    return expr;
}

ExpressionPtr Parser::parse_logical_and_expression() {
    auto expr = parse_nullish_expression();

    while (match(TokenType::AmpersandAmpersand)) {
        if (contains_nullish_coalescing(expr.get())) {
            error("Cannot mix '??' with '&&' without parentheses"_s);
        }
        auto right = parse_nullish_expression();
        if (contains_nullish_coalescing(right.get())) {
            error("Cannot mix '??' with '&&' without parentheses"_s);
        }
        auto logical = std::make_unique<LogicalExpression>();
        logical->op = LogicalExpression::Operator::And;
        logical->left = std::move(expr);
        logical->right = std::move(right);
        expr = std::move(logical);
    }
    return expr;
}

ExpressionPtr Parser::parse_nullish_expression() {
    auto expr = parse_bitwise_or_expression();

    while (match(TokenType::QuestionQuestion)) {
        auto right = parse_bitwise_or_expression();
        auto logical = std::make_unique<LogicalExpression>();
        logical->op = LogicalExpression::Operator::NullishCoalescing;
        logical->left = std::move(expr);
        logical->right = std::move(right);
        expr = std::move(logical);
    }
    return expr;
}

ExpressionPtr Parser::parse_bitwise_or_expression() {
    auto expr = parse_bitwise_xor_expression();

    while (match(TokenType::Pipe)) {
        auto right = parse_bitwise_xor_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = BinaryExpression::Operator::BitwiseOr;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_bitwise_xor_expression() {
    auto expr = parse_bitwise_and_expression();

    while (match(TokenType::Caret)) {
        auto right = parse_bitwise_and_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = BinaryExpression::Operator::BitwiseXor;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_bitwise_and_expression() {
    auto expr = parse_equality_expression();

    while (match(TokenType::Ampersand)) {
        auto right = parse_equality_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = BinaryExpression::Operator::BitwiseAnd;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_equality_expression() {
    auto expr = parse_relational_expression();

    while (true) {
        auto op = binary_operator_for(current_token().type);
        if (!op || (*op != BinaryExpression::Operator::Equal &&
                    *op != BinaryExpression::Operator::NotEqual &&
                    *op != BinaryExpression::Operator::StrictEqual &&
                    *op != BinaryExpression::Operator::StrictNotEqual)) {
            break;
        }
        advance();
        auto right = parse_relational_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = *op;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_relational_expression(bool allow_in) {
    auto expr = parse_shift_expression(allow_in);

    while (true) {
        auto op = binary_operator_for(current_token().type);
        if (!op || (*op != BinaryExpression::Operator::LessThan &&
                    *op != BinaryExpression::Operator::LessEqual &&
                    *op != BinaryExpression::Operator::GreaterThan &&
                    *op != BinaryExpression::Operator::GreaterEqual &&
                    *op != BinaryExpression::Operator::Instanceof &&
                    *op != BinaryExpression::Operator::In) ||
            (!allow_in && *op == BinaryExpression::Operator::In)) {
            break;
        }
        advance();
        auto right = parse_shift_expression(allow_in);
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = *op;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_shift_expression(bool allow_in) {
    auto expr = parse_additive_expression();

    while (true) {
        auto op = binary_operator_for(current_token().type);
        if (!op || (*op != BinaryExpression::Operator::LeftShift &&
                    *op != BinaryExpression::Operator::RightShift &&
                    *op != BinaryExpression::Operator::UnsignedRightShift)) {
            break;
        }
        advance();
        auto right = parse_additive_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = *op;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_additive_expression() {
    auto expr = parse_multiplicative_expression();

    while (true) {
        auto op = binary_operator_for(current_token().type);
        if (!op || (*op != BinaryExpression::Operator::Add && *op != BinaryExpression::Operator::Subtract)) {
            break;
        }
        advance();
        auto right = parse_multiplicative_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = *op;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_multiplicative_expression() {
    auto expr = parse_exponentiation_expression();

    while (true) {
        auto op = binary_operator_for(current_token().type);
        if (!op || (*op != BinaryExpression::Operator::Multiply &&
                    *op != BinaryExpression::Operator::Divide &&
                    *op != BinaryExpression::Operator::Modulo)) {
            break;
        }
        advance();
        auto right = parse_exponentiation_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = *op;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        expr = std::move(binary);
    }
    return expr;
}

ExpressionPtr Parser::parse_exponentiation_expression() {
    auto expr = parse_unary_expression();
    if (match(TokenType::StarStar)) {
        auto right = parse_exponentiation_expression();
        auto binary = std::make_unique<BinaryExpression>();
        binary->op = BinaryExpression::Operator::Exponent;
        binary->left = std::move(expr);
        binary->right = std::move(right);
        return binary;
    }
    return expr;
}

ExpressionPtr Parser::parse_unary_expression() {
    if (match(TokenType::Exclamation)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Not;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Minus)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Minus;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Plus)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Plus;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Typeof)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Typeof;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Void)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Void;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Delete)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Delete;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Await)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::Await;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::Tilde)) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = UnaryExpression::Operator::BitwiseNot;
        expr->argument = parse_unary_expression();
        return expr;
    }
    if (match(TokenType::PlusPlus)) {
        auto update = std::make_unique<UpdateExpression>();
        update->op = UpdateExpression::Operator::Increment;
        update->prefix = true;
        update->argument = parse_unary_expression();
        return update;
    }
    if (match(TokenType::MinusMinus)) {
        auto update = std::make_unique<UpdateExpression>();
        update->op = UpdateExpression::Operator::Decrement;
        update->prefix = true;
        update->argument = parse_unary_expression();
        return update;
    }

    return parse_update_expression();
}

ExpressionPtr Parser::parse_update_expression() {
    auto expr = parse_new_expression();
    if (!current_token().preceded_by_line_terminator && match(TokenType::PlusPlus)) {
        auto update = std::make_unique<UpdateExpression>();
        update->op = UpdateExpression::Operator::Increment;
        update->argument = std::move(expr);
        update->prefix = false;
        return update;
    }
    if (!current_token().preceded_by_line_terminator && match(TokenType::MinusMinus)) {
        auto update = std::make_unique<UpdateExpression>();
        update->op = UpdateExpression::Operator::Decrement;
        update->argument = std::move(expr);
        update->prefix = false;
        return update;
    }
    return expr;
}

ExpressionPtr Parser::parse_new_expression() {
    if (match(TokenType::New)) {
        auto expr = std::make_unique<NewExpression>();
        expr->callee = parse_new_expression();
        if (match(TokenType::OpenParen)) {
            if (!check(TokenType::CloseParen)) {
                do {
                    expr->arguments.push_back(parse_assignment_expression());
                } while (match(TokenType::Comma));
            }
            expect(TokenType::CloseParen, "Expected ')' after arguments"_s);
        }
        return expr;
    }
    return parse_call_member_expression();
}

ExpressionPtr Parser::parse_call_member_expression() {
    // Arrow function with single identifier parameter
    if (check(TokenType::Identifier)) {
        Token ident = current_token();
        Token next = m_lexer.peek_token();
        if (next.type == TokenType::Arrow) {
            advance(); // consume identifier
            advance(); // consume arrow
            std::vector<String> params{ident.value};
            return parse_arrow_function_after_params(std::move(params), true);
        }
    }

    auto expr = parse_primary_expression();

    while (true) {
        if (match(TokenType::OptionalChain)) {
            if (match(TokenType::OpenParen)) {
                auto call = std::make_unique<CallExpression>();
                call->callee = std::move(expr);
                call->optional = true;
                if (!check(TokenType::CloseParen)) {
                    do {
                        call->arguments.push_back(parse_assignment_expression());
                    } while (match(TokenType::Comma));
                }
                expect(TokenType::CloseParen, "Expected ')' after arguments"_s);
                expr = std::move(call);
            } else if (match(TokenType::OpenBracket)) {
                auto property = parse_expression_impl();
                expect(TokenType::CloseBracket, "Expected ']' after computed property"_s);
                auto member = std::make_unique<MemberExpression>();
                member->object = std::move(expr);
                member->property = std::move(property);
                member->computed = true;
                member->optional = true;
                expr = std::move(member);
            } else {
                Token prop = expect(TokenType::Identifier, "Expected property name after '?.'"_s);
                auto member = std::make_unique<MemberExpression>();
                member->object = std::move(expr);
                member->computed = false;
                member->optional = true;
                auto id = std::make_unique<Identifier>();
                id->name = prop.value;
                member->property = std::move(id);
                expr = std::move(member);
            }
        } else if (match(TokenType::OpenParen)) {
            auto call = std::make_unique<CallExpression>();
            call->callee = std::move(expr);
            if (!check(TokenType::CloseParen)) {
                do {
                    call->arguments.push_back(parse_assignment_expression());
                } while (match(TokenType::Comma));
            }
            expect(TokenType::CloseParen, "Expected ')' after arguments"_s);
            expr = std::move(call);
        } else if (match(TokenType::Dot)) {
            Token prop = expect(TokenType::Identifier, "Expected property name after '.'"_s);
            auto member = std::make_unique<MemberExpression>();
            member->object = std::move(expr);
            member->computed = false;
            auto id = std::make_unique<Identifier>();
            id->name = prop.value;
            member->property = std::move(id);
            expr = std::move(member);
        } else if (match(TokenType::OpenBracket)) {
            auto property = parse_expression_impl();
            expect(TokenType::CloseBracket, "Expected ']' after computed property"_s);
            auto member = std::make_unique<MemberExpression>();
            member->object = std::move(expr);
            member->property = std::move(property);
            member->computed = true;
            expr = std::move(member);
        } else {
            break;
        }
    }

    return expr;
}

ExpressionPtr Parser::parse_template_literal(const Token& head) {
    auto literal = std::make_unique<TemplateLiteral>();
    TemplateElement head_elem;
    head_elem.value = head.value;
    head_elem.tail = false;
    literal->quasis.push_back(head_elem);

    while (true) {
        literal->expressions.push_back(parse_expression_impl());
        if (!check(TokenType::CloseBrace)) {
            error("Expected '}' in template expression"_s);
            break;
        }
        m_lexer.set_template_mode(true);
        advance(); // consume } and load next template chunk
        if (match(TokenType::TemplateTail)) {
            TemplateElement tail;
            tail.value = m_previous.value;
            tail.tail = true;
            literal->quasis.push_back(tail);
            break;
        }
        if (match(TokenType::TemplateMiddle)) {
            TemplateElement middle;
            middle.value = m_previous.value;
            middle.tail = false;
            literal->quasis.push_back(middle);
            continue;
        }
        error("Unterminated template literal"_s);
        break;
    }
    return literal;
}

ExpressionPtr Parser::parse_primary_expression() {
    if (match(TokenType::Null)) {
        return std::make_unique<NullLiteral>();
    }
    if (match(TokenType::True)) {
        auto lit = std::make_unique<BooleanLiteral>();
        lit->value = true;
        return lit;
    }
    if (match(TokenType::False)) {
        auto lit = std::make_unique<BooleanLiteral>();
        lit->value = false;
        return lit;
    }
    if (match(TokenType::Number)) {
        auto lit = std::make_unique<NumericLiteral>();
        lit->value = m_previous.number_value;
        return lit;
    }
    if (match(TokenType::String)) {
        auto lit = std::make_unique<StringLiteral>();
        lit->value = m_previous.value;
        return lit;
    }
    if (match(TokenType::RegExp)) {
        auto lit = std::make_unique<RegExpLiteral>();
        lit->pattern = m_previous.value;
        lit->flags = m_previous.regex_flags;
        return lit;
    }
    if (match(TokenType::This)) {
        return std::make_unique<ThisExpression>();
    }
    if (match(TokenType::Identifier)) {
        auto id = std::make_unique<Identifier>();
        id->name = m_previous.value;
        return id;
    }
    if (match(TokenType::OpenParen)) {
        auto expr = parse_expression_impl();
        expect(TokenType::CloseParen, "Expected ')' after expression"_s);
        return expr;
    }
    if (match(TokenType::OpenBracket)) {
        return parse_array_expression();
    }
    if (match(TokenType::OpenBrace)) {
        return parse_object_expression();
    }
    if (match(TokenType::Function)) {
        return parse_function_expression();
    }
    if (match(TokenType::TemplateHead)) {
        return parse_template_literal(m_previous);
    }
    if (match(TokenType::NoSubstitutionTemplate)) {
        auto lit = std::make_unique<TemplateLiteral>();
        TemplateElement elem;
        elem.value = m_previous.value;
        elem.tail = true;
        lit->quasis.push_back(elem);
        return lit;
    }

    error("Unexpected token in expression"_s);
    return std::make_unique<NullLiteral>();
}

// ============================================================================
// Helpers
// ============================================================================

bool Parser::is_assignment_operator(TokenType type) const {
    switch (type) {
        case TokenType::Assign:
        case TokenType::PlusAssign:
        case TokenType::MinusAssign:
        case TokenType::StarAssign:
        case TokenType::SlashAssign:
        case TokenType::PercentAssign:
        case TokenType::StarStarAssign:
        case TokenType::LeftShiftAssign:
        case TokenType::RightShiftAssign:
        case TokenType::UnsignedRightShiftAssign:
        case TokenType::AmpersandAssign:
        case TokenType::PipeAssign:
        case TokenType::CaretAssign:
        case TokenType::AmpersandAmpersandAssign:
        case TokenType::PipePipeAssign:
        case TokenType::QuestionQuestionAssign:
            return true;
        default:
            return false;
    }
}

AssignmentExpression::Operator Parser::to_assignment_operator(TokenType type) const {
    switch (type) {
        case TokenType::Assign: return AssignmentExpression::Operator::Assign;
        case TokenType::PlusAssign: return AssignmentExpression::Operator::AddAssign;
        case TokenType::MinusAssign: return AssignmentExpression::Operator::SubtractAssign;
        case TokenType::StarAssign: return AssignmentExpression::Operator::MultiplyAssign;
        case TokenType::SlashAssign: return AssignmentExpression::Operator::DivideAssign;
        case TokenType::PercentAssign: return AssignmentExpression::Operator::ModuloAssign;
        case TokenType::StarStarAssign: return AssignmentExpression::Operator::ExponentAssign;
        case TokenType::LeftShiftAssign: return AssignmentExpression::Operator::LeftShiftAssign;
        case TokenType::RightShiftAssign: return AssignmentExpression::Operator::RightShiftAssign;
        case TokenType::UnsignedRightShiftAssign: return AssignmentExpression::Operator::UnsignedRightShiftAssign;
        case TokenType::AmpersandAssign: return AssignmentExpression::Operator::BitwiseAndAssign;
        case TokenType::PipeAssign: return AssignmentExpression::Operator::BitwiseOrAssign;
        case TokenType::CaretAssign: return AssignmentExpression::Operator::BitwiseXorAssign;
        case TokenType::AmpersandAmpersandAssign: return AssignmentExpression::Operator::LogicalAndAssign;
        case TokenType::PipePipeAssign: return AssignmentExpression::Operator::LogicalOrAssign;
        case TokenType::QuestionQuestionAssign: return AssignmentExpression::Operator::NullishAssign;
        default: return AssignmentExpression::Operator::Assign;
    }
}

std::optional<BinaryExpression::Operator> Parser::binary_operator_for(TokenType type) const {
    switch (type) {
        case TokenType::Plus: return BinaryExpression::Operator::Add;
        case TokenType::Minus: return BinaryExpression::Operator::Subtract;
        case TokenType::Star: return BinaryExpression::Operator::Multiply;
        case TokenType::Slash: return BinaryExpression::Operator::Divide;
        case TokenType::Percent: return BinaryExpression::Operator::Modulo;
        case TokenType::StarStar: return BinaryExpression::Operator::Exponent;
        case TokenType::Equal: return BinaryExpression::Operator::Equal;
        case TokenType::NotEqual: return BinaryExpression::Operator::NotEqual;
        case TokenType::StrictEqual: return BinaryExpression::Operator::StrictEqual;
        case TokenType::StrictNotEqual: return BinaryExpression::Operator::StrictNotEqual;
        case TokenType::LessThan: return BinaryExpression::Operator::LessThan;
        case TokenType::LessEqual: return BinaryExpression::Operator::LessEqual;
        case TokenType::GreaterThan: return BinaryExpression::Operator::GreaterThan;
        case TokenType::GreaterEqual: return BinaryExpression::Operator::GreaterEqual;
        case TokenType::LeftShift: return BinaryExpression::Operator::LeftShift;
        case TokenType::RightShift: return BinaryExpression::Operator::RightShift;
        case TokenType::UnsignedRightShift: return BinaryExpression::Operator::UnsignedRightShift;
        case TokenType::Ampersand: return BinaryExpression::Operator::BitwiseAnd;
        case TokenType::Pipe: return BinaryExpression::Operator::BitwiseOr;
        case TokenType::Caret: return BinaryExpression::Operator::BitwiseXor;
        case TokenType::Instanceof: return BinaryExpression::Operator::Instanceof;
        case TokenType::In: return BinaryExpression::Operator::In;
        default:
            return std::nullopt;
    }
}

std::optional<LogicalExpression::Operator> Parser::logical_operator_for(TokenType type) const {
    switch (type) {
        case TokenType::PipePipe: return LogicalExpression::Operator::Or;
        case TokenType::AmpersandAmpersand: return LogicalExpression::Operator::And;
        case TokenType::QuestionQuestion: return LogicalExpression::Operator::NullishCoalescing;
        default:
            return std::nullopt;
    }
}

void Parser::consume_semicolon_if_present() {
    if (match(TokenType::Semicolon)) {
        return;
    }
    if (current_token().preceded_by_line_terminator) {
        return;
    }
    switch (current_token().type) {
        case TokenType::EndOfFile:
        case TokenType::CloseBrace:
            return;
        default:
            break;
    }
}

bool Parser::contains_nullish_coalescing(const Expression* expr) const {
    if (!expr) return false;
    if (auto logical = dynamic_cast<const LogicalExpression*>(expr)) {
        if (logical->op == LogicalExpression::Operator::NullishCoalescing) return true;
        return contains_nullish_coalescing(logical->left.get()) || contains_nullish_coalescing(logical->right.get());
    }
    if (auto binary = dynamic_cast<const BinaryExpression*>(expr)) {
        return contains_nullish_coalescing(binary->left.get()) || contains_nullish_coalescing(binary->right.get());
    }
    if (auto conditional = dynamic_cast<const ConditionalExpression*>(expr)) {
        return contains_nullish_coalescing(conditional->test.get()) ||
               contains_nullish_coalescing(conditional->consequent.get()) ||
               contains_nullish_coalescing(conditional->alternate.get());
    }
    if (auto assign = dynamic_cast<const AssignmentExpression*>(expr)) {
        return contains_nullish_coalescing(assign->left.get()) || contains_nullish_coalescing(assign->right.get());
    }
    if (auto call = dynamic_cast<const CallExpression*>(expr)) {
        if (contains_nullish_coalescing(call->callee.get())) return true;
        for (const auto& arg : call->arguments) {
            if (contains_nullish_coalescing(arg.get())) return true;
        }
    }
    if (auto member = dynamic_cast<const MemberExpression*>(expr)) {
        return contains_nullish_coalescing(member->object.get()) || contains_nullish_coalescing(member->property.get());
    }
    if (auto tmpl = dynamic_cast<const TemplateLiteral*>(expr)) {
        for (const auto& part : tmpl->expressions) {
            if (contains_nullish_coalescing(part.get())) return true;
        }
    }
    if (auto array = dynamic_cast<const ArrayExpression*>(expr)) {
        for (const auto& el : array->elements) {
            if (contains_nullish_coalescing(el.get())) return true;
        }
    }
    if (auto obj = dynamic_cast<const ObjectExpression*>(expr)) {
        for (const auto& prop : obj->properties) {
            if (contains_nullish_coalescing(prop.value.get()) || contains_nullish_coalescing(prop.computed_key.get())) {
                return true;
            }
        }
    }
    if (auto func = dynamic_cast<const FunctionExpression*>(expr)) {
        for (const auto& stmt : func->body) {
            // statements are not inspected deeply for brevity
            (void)stmt;
        }
    }
    return false;
}

bool Parser::allows_regexp_after(const Token& token) const {
    switch (token.type) {
        case TokenType::Identifier:
        case TokenType::Number:
        case TokenType::String:
        case TokenType::True:
        case TokenType::False:
        case TokenType::Null:
        case TokenType::This:
        case TokenType::RegExp:
        case TokenType::CloseParen:
        case TokenType::CloseBracket:
        case TokenType::CloseBrace:
        case TokenType::PlusPlus:
        case TokenType::MinusMinus:
        case TokenType::TemplateTail:
        case TokenType::NoSubstitutionTemplate:
            return false;
        default:
            return true;
    }
}

ExpressionPtr Parser::parse_array_expression() {
    auto arr = std::make_unique<ArrayExpression>();
    if (!check(TokenType::CloseBracket)) {
        do {
            if (check(TokenType::CloseBracket)) break;
            if (match(TokenType::Ellipsis)) {
                auto spread = std::make_unique<SpreadElement>();
                spread->argument = parse_assignment_expression();
                arr->elements.push_back(std::move(spread));
            } else {
                arr->elements.push_back(parse_assignment_expression());
            }
        } while (match(TokenType::Comma));
    }
    expect(TokenType::CloseBracket, "Expected ']' after array literal"_s);
    return arr;
}

ExpressionPtr Parser::parse_object_expression() {
    auto obj = std::make_unique<ObjectExpression>();
    if (!check(TokenType::CloseBrace)) {
        do {
            if (check(TokenType::CloseBrace)) break;

            ObjectProperty prop;
            bool computed = false;
            String key;
            ExpressionPtr computed_key;

            if (match(TokenType::Ellipsis)) {
                prop.spread = true;
                prop.value = parse_expression_impl();
                obj->properties.push_back(std::move(prop));
                continue;
            }

            if (match(TokenType::Identifier)) {
                key = m_previous.value;
            } else if (match(TokenType::String)) {
                key = m_previous.value;
            } else if (match(TokenType::Number)) {
                key = m_previous.value;
            } else if (match(TokenType::OpenBracket)) {
                computed = true;
                computed_key = parse_expression_impl();
                expect(TokenType::CloseBracket, "Expected ']' after computed key"_s);
            } else {
                error("Expected property name in object literal"_s);
                break;
            }

            if (match(TokenType::Colon)) {
                prop.value = parse_expression_impl();
            } else if (!computed) {
                auto id_expr = std::make_unique<Identifier>();
                id_expr->name = key;
                prop.value = std::move(id_expr);
            } else {
                error("Computed properties require a value"_s);
            }

            prop.key = key;
            prop.computed = computed;
            prop.computed_key = std::move(computed_key);
            obj->properties.push_back(std::move(prop));
        } while (match(TokenType::Comma));
    }
    expect(TokenType::CloseBrace, "Expected '}' after object literal"_s);
    return obj;
}

ExpressionPtr Parser::parse_function_expression(bool is_arrow, std::vector<String> params) {
    auto func = std::make_unique<FunctionExpression>();
    func->is_arrow = is_arrow;

    if (!is_arrow) {
        if (check(TokenType::Identifier)) {
            func->name = m_current.value;
            advance();
        }
        expect(TokenType::OpenParen, "Expected '(' after function keyword"_s);
        if (!check(TokenType::CloseParen)) {
            do {
                auto param = expect(TokenType::Identifier, "Expected parameter name"_s);
                params.push_back(param.value);
            } while (match(TokenType::Comma));
        }
        expect(TokenType::CloseParen, "Expected ')' after parameters"_s);
    }

    func->params = std::move(params);

    expect(TokenType::OpenBrace, "Expected '{' before function body"_s);
    while (!check(TokenType::CloseBrace) && !at_end()) {
        func->body.push_back(parse_statement());
    }
    expect(TokenType::CloseBrace, "Expected '}' after function body"_s);
    return func;
}

ExpressionPtr Parser::parse_arrow_function_after_params(std::vector<String> params, bool /*single_param*/) {
    auto func = std::make_unique<FunctionExpression>();
    func->is_arrow = true;
    func->params = std::move(params);

    if (match(TokenType::OpenBrace)) {
        while (!check(TokenType::CloseBrace) && !at_end()) {
            func->body.push_back(parse_statement());
        }
        expect(TokenType::CloseBrace, "Expected '}' after arrow function body"_s);
    } else {
        func->expression_body = true;
        func->concise_body = parse_assignment_expression();
    }
    return func;
}

// ============================================================================
// Convenience wrappers
// ============================================================================

std::unique_ptr<Program> parse_script(const String& source) {
    Parser parser;
    return parser.parse(source);
}

std::unique_ptr<Program> parse_script(std::string_view source) {
    Parser parser;
    return parser.parse(source);
}

} // namespace lithium::js
