/**
 * JavaScript Parser implementation (stub)
 */

#include "lithium/js/parser.hpp"
#include "lithium/js/ast.hpp"

namespace lithium::js {

Parser::Parser() = default;

void Parser::set_input(const String& source) {
    m_lexer.set_input(source);
}

void Parser::set_input(std::string_view source) {
    m_lexer.set_input(source);
}

std::unique_ptr<Program> Parser::parse_program() {
    auto program = std::make_unique<Program>();
    // Simplified: basic statement parsing
    while (!at_end()) {
        if (auto stmt = parse_statement()) {
            program->body.push_back(std::move(stmt));
        } else {
            break;
        }
    }
    return program;
}

std::unique_ptr<Expression> Parser::parse_expression() {
    return parse_assignment_expression();
}

std::unique_ptr<Statement> Parser::parse_statement() {
    auto& token = current_token();

    switch (token.type) {
        case TokenType::Var:
        case TokenType::Let:
        case TokenType::Const:
            return parse_variable_declaration();
        case TokenType::Function:
            return parse_function_declaration();
        case TokenType::Class:
            return parse_class_declaration();
        case TokenType::If:
            return parse_if_statement();
        case TokenType::While:
            return parse_while_statement();
        case TokenType::For:
            return parse_for_statement();
        case TokenType::Return:
            return parse_return_statement();
        case TokenType::Break:
            consume_token();
            consume_semicolon();
            return std::make_unique<BreakStatement>();
        case TokenType::Continue:
            consume_token();
            consume_semicolon();
            return std::make_unique<ContinueStatement>();
        case TokenType::Throw:
            return parse_throw_statement();
        case TokenType::Try:
            return parse_try_statement();
        case TokenType::OpenBrace:
            return parse_block_statement();
        default:
            return parse_expression_statement();
    }
}

// Stub implementations
std::unique_ptr<Statement> Parser::parse_variable_declaration() {
    auto stmt = std::make_unique<VariableDeclaration>();
    auto& token = current_token();
    if (token.type == TokenType::Var) stmt->kind = VariableDeclaration::Kind::Var;
    else if (token.type == TokenType::Let) stmt->kind = VariableDeclaration::Kind::Let;
    else stmt->kind = VariableDeclaration::Kind::Const;
    consume_token();

    // Parse declarators
    do {
        VariableDeclarator decl;
        if (current_token().type == TokenType::Identifier) {
            decl.id = std::make_unique<Identifier>();
            static_cast<Identifier*>(decl.id.get())->name = current_token().value;
            consume_token();
        }
        if (current_token().type == TokenType::Assign) {
            consume_token();
            decl.init = parse_assignment_expression();
        }
        stmt->declarations.push_back(std::move(decl));
    } while (consume_if(TokenType::Comma));

    consume_semicolon();
    return stmt;
}

std::unique_ptr<Statement> Parser::parse_function_declaration() {
    consume_token(); // function
    auto func = std::make_unique<FunctionDeclaration>();

    if (current_token().type == TokenType::Identifier) {
        func->id = std::make_unique<Identifier>();
        static_cast<Identifier*>(func->id.get())->name = current_token().value;
        consume_token();
    }

    expect(TokenType::OpenParen);
    func->params = parse_parameters();
    expect(TokenType::CloseParen);
    func->body = parse_block_statement();

    return func;
}

std::unique_ptr<Statement> Parser::parse_class_declaration() {
    consume_token(); // class
    auto cls = std::make_unique<ClassDeclaration>();

    if (current_token().type == TokenType::Identifier) {
        cls->id = std::make_unique<Identifier>();
        static_cast<Identifier*>(cls->id.get())->name = current_token().value;
        consume_token();
    }

    if (consume_if(TokenType::Extends)) {
        cls->superClass = parse_expression();
    }

    cls->body = parse_class_body();
    return cls;
}

std::unique_ptr<Statement> Parser::parse_if_statement() {
    consume_token(); // if
    auto stmt = std::make_unique<IfStatement>();

    expect(TokenType::OpenParen);
    stmt->test = parse_expression();
    expect(TokenType::CloseParen);
    stmt->consequent = parse_statement();

    if (consume_if(TokenType::Else)) {
        stmt->alternate = parse_statement();
    }

    return stmt;
}

std::unique_ptr<Statement> Parser::parse_while_statement() {
    consume_token(); // while
    auto stmt = std::make_unique<WhileStatement>();

    expect(TokenType::OpenParen);
    stmt->test = parse_expression();
    expect(TokenType::CloseParen);
    stmt->body = parse_statement();

    return stmt;
}

std::unique_ptr<Statement> Parser::parse_for_statement() {
    consume_token(); // for
    auto stmt = std::make_unique<ForStatement>();

    expect(TokenType::OpenParen);

    if (!consume_if(TokenType::Semicolon)) {
        if (current_token().type == TokenType::Var ||
            current_token().type == TokenType::Let ||
            current_token().type == TokenType::Const) {
            stmt->init = parse_variable_declaration();
        } else {
            stmt->init = parse_expression();
            expect(TokenType::Semicolon);
        }
    }

    if (current_token().type != TokenType::Semicolon) {
        stmt->test = parse_expression();
    }
    expect(TokenType::Semicolon);

    if (current_token().type != TokenType::CloseParen) {
        stmt->update = parse_expression();
    }
    expect(TokenType::CloseParen);

    stmt->body = parse_statement();
    return stmt;
}

std::unique_ptr<Statement> Parser::parse_return_statement() {
    consume_token(); // return
    auto stmt = std::make_unique<ReturnStatement>();

    if (current_token().type != TokenType::Semicolon &&
        current_token().type != TokenType::CloseBrace &&
        current_token().type != TokenType::EndOfFile &&
        !current_token().preceded_by_line_terminator) {
        stmt->argument = parse_expression();
    }

    consume_semicolon();
    return stmt;
}

std::unique_ptr<Statement> Parser::parse_throw_statement() {
    consume_token(); // throw
    auto stmt = std::make_unique<ThrowStatement>();
    stmt->argument = parse_expression();
    consume_semicolon();
    return stmt;
}

std::unique_ptr<Statement> Parser::parse_try_statement() {
    consume_token(); // try
    auto stmt = std::make_unique<TryStatement>();

    stmt->block = parse_block_statement();

    if (consume_if(TokenType::Catch)) {
        stmt->handler = std::make_unique<CatchClause>();
        if (consume_if(TokenType::OpenParen)) {
            if (current_token().type == TokenType::Identifier) {
                stmt->handler->param = std::make_unique<Identifier>();
                static_cast<Identifier*>(stmt->handler->param.get())->name = current_token().value;
                consume_token();
            }
            expect(TokenType::CloseParen);
        }
        stmt->handler->body = parse_block_statement();
    }

    if (consume_if(TokenType::Finally)) {
        stmt->finalizer = parse_block_statement();
    }

    return stmt;
}

std::unique_ptr<BlockStatement> Parser::parse_block_statement() {
    expect(TokenType::OpenBrace);
    auto block = std::make_unique<BlockStatement>();

    while (!at_end() && current_token().type != TokenType::CloseBrace) {
        if (auto stmt = parse_statement()) {
            block->body.push_back(std::move(stmt));
        }
    }

    expect(TokenType::CloseBrace);
    return block;
}

std::unique_ptr<Statement> Parser::parse_expression_statement() {
    auto stmt = std::make_unique<ExpressionStatement>();
    stmt->expression = parse_expression();
    consume_semicolon();
    return stmt;
}

// Expression parsing
std::unique_ptr<Expression> Parser::parse_assignment_expression() {
    auto left = parse_conditional_expression();

    if (current_token().is_assignment_operator()) {
        auto expr = std::make_unique<AssignmentExpression>();
        expr->left = std::move(left);
        expr->op = current_token().value.empty() ? "="_s : current_token().value;
        consume_token();
        expr->right = parse_assignment_expression();
        return expr;
    }

    return left;
}

std::unique_ptr<Expression> Parser::parse_conditional_expression() {
    auto test = parse_binary_expression(0);

    if (consume_if(TokenType::Question)) {
        auto expr = std::make_unique<ConditionalExpression>();
        expr->test = std::move(test);
        expr->consequent = parse_assignment_expression();
        expect(TokenType::Colon);
        expr->alternate = parse_assignment_expression();
        return expr;
    }

    return test;
}

std::unique_ptr<Expression> Parser::parse_binary_expression(int precedence) {
    auto left = parse_unary_expression();

    while (current_token().is_binary_operator()) {
        int op_prec = get_precedence(current_token().type);
        if (op_prec < precedence) break;

        auto expr = std::make_unique<BinaryExpression>();
        expr->left = std::move(left);
        expr->op = current_token().value.empty() ? "+"_s : current_token().value;
        consume_token();
        expr->right = parse_binary_expression(op_prec + 1);
        left = std::move(expr);
    }

    return left;
}

std::unique_ptr<Expression> Parser::parse_unary_expression() {
    if (current_token().is_unary_operator()) {
        auto expr = std::make_unique<UnaryExpression>();
        expr->op = current_token().value.empty() ? "!"_s : current_token().value;
        expr->prefix = true;
        consume_token();
        expr->argument = parse_unary_expression();
        return expr;
    }

    return parse_postfix_expression();
}

std::unique_ptr<Expression> Parser::parse_postfix_expression() {
    auto expr = parse_call_expression();

    if (current_token().type == TokenType::PlusPlus ||
        current_token().type == TokenType::MinusMinus) {
        if (!current_token().preceded_by_line_terminator) {
            auto update = std::make_unique<UpdateExpression>();
            update->argument = std::move(expr);
            update->op = current_token().type == TokenType::PlusPlus ? "++"_s : "--"_s;
            update->prefix = false;
            consume_token();
            return update;
        }
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_call_expression() {
    auto expr = parse_member_expression();

    while (true) {
        if (current_token().type == TokenType::OpenParen) {
            auto call = std::make_unique<CallExpression>();
            call->callee = std::move(expr);
            consume_token();
            call->arguments = parse_arguments();
            expect(TokenType::CloseParen);
            expr = std::move(call);
        } else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Expression> Parser::parse_member_expression() {
    auto object = parse_primary_expression();

    while (true) {
        if (consume_if(TokenType::Dot)) {
            auto member = std::make_unique<MemberExpression>();
            member->object = std::move(object);
            member->computed = false;
            if (current_token().type == TokenType::Identifier) {
                member->property = std::make_unique<Identifier>();
                static_cast<Identifier*>(member->property.get())->name = current_token().value;
                consume_token();
            }
            object = std::move(member);
        } else if (consume_if(TokenType::OpenBracket)) {
            auto member = std::make_unique<MemberExpression>();
            member->object = std::move(object);
            member->computed = true;
            member->property = parse_expression();
            expect(TokenType::CloseBracket);
            object = std::move(member);
        } else {
            break;
        }
    }

    return object;
}

std::unique_ptr<Expression> Parser::parse_primary_expression() {
    auto& token = current_token();

    switch (token.type) {
        case TokenType::Null:
            consume_token();
            return std::make_unique<NullLiteral>();

        case TokenType::True:
        case TokenType::False: {
            auto lit = std::make_unique<BooleanLiteral>();
            lit->value = token.type == TokenType::True;
            consume_token();
            return lit;
        }

        case TokenType::Number: {
            auto lit = std::make_unique<NumberLiteral>();
            lit->value = token.number_value;
            consume_token();
            return lit;
        }

        case TokenType::String: {
            auto lit = std::make_unique<StringLiteral>();
            lit->value = token.value;
            consume_token();
            return lit;
        }

        case TokenType::Identifier: {
            auto ident = std::make_unique<Identifier>();
            ident->name = token.value;
            consume_token();
            return ident;
        }

        case TokenType::This:
            consume_token();
            return std::make_unique<ThisExpression>();

        case TokenType::OpenParen: {
            consume_token();
            auto expr = parse_expression();
            expect(TokenType::CloseParen);
            return expr;
        }

        case TokenType::OpenBracket:
            return parse_array_literal();

        case TokenType::OpenBrace:
            return parse_object_literal();

        case TokenType::Function:
            return parse_function_expression();

        case TokenType::New:
            return parse_new_expression();

        default:
            error("Unexpected token"_s);
            consume_token();
            return std::make_unique<NullLiteral>();
    }
}

std::unique_ptr<Expression> Parser::parse_array_literal() {
    expect(TokenType::OpenBracket);
    auto arr = std::make_unique<ArrayExpression>();

    while (current_token().type != TokenType::CloseBracket && !at_end()) {
        if (current_token().type == TokenType::Comma) {
            arr->elements.push_back(nullptr); // Elision
            consume_token();
        } else {
            arr->elements.push_back(parse_assignment_expression());
            if (current_token().type != TokenType::CloseBracket) {
                expect(TokenType::Comma);
            }
        }
    }

    expect(TokenType::CloseBracket);
    return arr;
}

std::unique_ptr<Expression> Parser::parse_object_literal() {
    expect(TokenType::OpenBrace);
    auto obj = std::make_unique<ObjectExpression>();

    while (current_token().type != TokenType::CloseBrace && !at_end()) {
        Property prop;

        // Key
        if (current_token().type == TokenType::Identifier ||
            current_token().type == TokenType::String ||
            current_token().type == TokenType::Number) {
            if (current_token().type == TokenType::Identifier) {
                prop.key = std::make_unique<Identifier>();
                static_cast<Identifier*>(prop.key.get())->name = current_token().value;
            } else if (current_token().type == TokenType::String) {
                auto lit = std::make_unique<StringLiteral>();
                lit->value = current_token().value;
                prop.key = std::move(lit);
            } else {
                auto lit = std::make_unique<NumberLiteral>();
                lit->value = current_token().number_value;
                prop.key = std::move(lit);
            }
            consume_token();
        }

        if (consume_if(TokenType::Colon)) {
            prop.value = parse_assignment_expression();
        } else {
            // Shorthand
            prop.shorthand = true;
            auto ident = std::make_unique<Identifier>();
            if (prop.key) {
                ident->name = static_cast<Identifier*>(prop.key.get())->name;
            }
            prop.value = std::move(ident);
        }

        obj->properties.push_back(std::move(prop));

        if (current_token().type != TokenType::CloseBrace) {
            expect(TokenType::Comma);
        }
    }

    expect(TokenType::CloseBrace);
    return obj;
}

std::unique_ptr<Expression> Parser::parse_function_expression() {
    consume_token(); // function
    auto func = std::make_unique<FunctionExpression>();

    if (current_token().type == TokenType::Identifier) {
        func->id = std::make_unique<Identifier>();
        static_cast<Identifier*>(func->id.get())->name = current_token().value;
        consume_token();
    }

    expect(TokenType::OpenParen);
    func->params = parse_parameters();
    expect(TokenType::CloseParen);
    func->body = parse_block_statement();

    return func;
}

std::unique_ptr<Expression> Parser::parse_arrow_function() {
    auto func = std::make_unique<ArrowFunctionExpression>();

    if (current_token().type == TokenType::Identifier) {
        auto param = std::make_unique<Identifier>();
        param->name = current_token().value;
        func->params.push_back(std::move(param));
        consume_token();
    } else {
        expect(TokenType::OpenParen);
        func->params = parse_parameters();
        expect(TokenType::CloseParen);
    }

    expect(TokenType::Arrow);

    if (current_token().type == TokenType::OpenBrace) {
        func->body = parse_block_statement();
        func->expression = false;
    } else {
        func->body = parse_assignment_expression();
        func->expression = true;
    }

    return func;
}

std::unique_ptr<Expression> Parser::parse_new_expression() {
    consume_token(); // new
    auto expr = std::make_unique<NewExpression>();
    expr->callee = parse_member_expression();

    if (consume_if(TokenType::OpenParen)) {
        expr->arguments = parse_arguments();
        expect(TokenType::CloseParen);
    }

    return expr;
}

std::unique_ptr<ClassBody> Parser::parse_class_body() {
    expect(TokenType::OpenBrace);
    auto body = std::make_unique<ClassBody>();

    while (current_token().type != TokenType::CloseBrace && !at_end()) {
        MethodDefinition method;
        method.kind = MethodDefinition::Kind::Method;

        // Static
        if (current_token().type == TokenType::Identifier &&
            current_token().value == "static"_s) {
            method.isStatic = true;
            consume_token();
        }

        // Getter/setter
        if (current_token().type == TokenType::Identifier) {
            if (current_token().value == "get"_s) {
                method.kind = MethodDefinition::Kind::Get;
                consume_token();
            } else if (current_token().value == "set"_s) {
                method.kind = MethodDefinition::Kind::Set;
                consume_token();
            }
        }

        // Key
        if (current_token().type == TokenType::Identifier) {
            method.key = std::make_unique<Identifier>();
            static_cast<Identifier*>(method.key.get())->name = current_token().value;
            if (current_token().value == "constructor"_s) {
                method.kind = MethodDefinition::Kind::Constructor;
            }
            consume_token();
        }

        // Method
        expect(TokenType::OpenParen);
        auto func = std::make_unique<FunctionExpression>();
        func->params = parse_parameters();
        expect(TokenType::CloseParen);
        func->body = parse_block_statement();
        method.value = std::move(func);

        body->body.push_back(std::move(method));
    }

    expect(TokenType::CloseBrace);
    return body;
}

std::vector<std::unique_ptr<Pattern>> Parser::parse_parameters() {
    std::vector<std::unique_ptr<Pattern>> params;

    while (current_token().type != TokenType::CloseParen && !at_end()) {
        if (current_token().type == TokenType::Identifier) {
            auto param = std::make_unique<Identifier>();
            param->name = current_token().value;
            consume_token();
            params.push_back(std::move(param));
        }

        if (current_token().type != TokenType::CloseParen) {
            expect(TokenType::Comma);
        }
    }

    return params;
}

std::vector<std::unique_ptr<Expression>> Parser::parse_arguments() {
    std::vector<std::unique_ptr<Expression>> args;

    while (current_token().type != TokenType::CloseParen && !at_end()) {
        args.push_back(parse_assignment_expression());

        if (current_token().type != TokenType::CloseParen) {
            expect(TokenType::Comma);
        }
    }

    return args;
}

// Helpers
const Token& Parser::current_token() const {
    return m_current;
}

void Parser::consume_token() {
    m_current = m_lexer.next_token();
}

bool Parser::consume_if(TokenType type) {
    if (m_current.type == type) {
        consume_token();
        return true;
    }
    return false;
}

void Parser::expect(TokenType type) {
    if (m_current.type != type) {
        error("Unexpected token"_s);
    }
    consume_token();
}

void Parser::consume_semicolon() {
    if (m_current.type == TokenType::Semicolon) {
        consume_token();
    } else if (m_current.preceded_by_line_terminator ||
               m_current.type == TokenType::CloseBrace ||
               m_current.type == TokenType::EndOfFile) {
        // Automatic semicolon insertion
    } else {
        error("Expected semicolon"_s);
    }
}

bool Parser::at_end() const {
    return m_current.type == TokenType::EndOfFile;
}

int Parser::get_precedence(TokenType type) const {
    switch (type) {
        case TokenType::PipePipe: return 1;
        case TokenType::AmpersandAmpersand: return 2;
        case TokenType::Pipe: return 3;
        case TokenType::Caret: return 4;
        case TokenType::Ampersand: return 5;
        case TokenType::Equal:
        case TokenType::NotEqual:
        case TokenType::StrictEqual:
        case TokenType::StrictNotEqual: return 6;
        case TokenType::LessThan:
        case TokenType::GreaterThan:
        case TokenType::LessEqual:
        case TokenType::GreaterEqual:
        case TokenType::Instanceof:
        case TokenType::In: return 7;
        case TokenType::LeftShift:
        case TokenType::RightShift:
        case TokenType::UnsignedRightShift: return 8;
        case TokenType::Plus:
        case TokenType::Minus: return 9;
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Percent: return 10;
        case TokenType::StarStar: return 11;
        default: return 0;
    }
}

void Parser::error(const String& message) {
    if (m_error_callback) {
        m_error_callback(message, m_current.line, m_current.column);
    }
    m_errors.push_back(message);
}

} // namespace lithium::js
