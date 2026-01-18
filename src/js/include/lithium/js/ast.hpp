#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <memory>
#include <optional>
#include <vector>

namespace lithium::js {

// ============================================================================
// Source Location
// ============================================================================

struct SourceLocation {
    usize start_line{0};
    usize start_column{0};
    usize end_line{0};
    usize end_column{0};
};

// Forward declarations
struct Statement;
using StatementPtr = std::unique_ptr<Statement>;

// ============================================================================
// AST Node Base
// ============================================================================

struct ASTNode {
    SourceLocation location;
    virtual ~ASTNode() = default;
};

// ============================================================================
// Expressions
// ============================================================================

struct Expression : ASTNode {
    virtual ~Expression() = default;
};

using ExpressionPtr = std::unique_ptr<Expression>;

// Literals
struct NullLiteral : Expression {};
struct BooleanLiteral : Expression { bool value{false}; };
struct NumericLiteral : Expression { f64 value{0}; };
struct StringLiteral : Expression { String value; };
struct RegExpLiteral : Expression {
    String pattern;
    String flags;
};

struct ThisExpression : Expression {};

// Identifier
struct Identifier : Expression {
    String name;
};

// Arrays and objects
struct ArrayExpression : Expression {
    std::vector<ExpressionPtr> elements;
};

struct ObjectProperty : ASTNode {
    String key;
    ExpressionPtr value;
    ExpressionPtr computed_key;
    bool computed{false};
    bool spread{false};
};

struct ObjectExpression : Expression {
    std::vector<ObjectProperty> properties;
};

// Member / Call
struct MemberExpression : Expression {
    ExpressionPtr object;
    ExpressionPtr property;
    bool computed{false};
    bool optional{false};
};

struct CallExpression : Expression {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
    bool optional{false};
};

// Unary / Binary / Logical
struct UnaryExpression : Expression {
    enum class Operator { Plus, Minus, Not, Typeof, Void, Delete, Await, BitwiseNot };
    Operator op{Operator::Plus};
    ExpressionPtr argument;
};

struct BinaryExpression : Expression {
    enum class Operator {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Equal,
        NotEqual,
        StrictEqual,
        StrictNotEqual,
        LessThan,
        LessEqual,
        GreaterThan,
        GreaterEqual,
        LeftShift,
        RightShift,
        UnsignedRightShift,
        BitwiseAnd,
        BitwiseOr,
        BitwiseXor,
        Exponent,
        Instanceof,
        In
    };
    Operator op{Operator::Add};
    ExpressionPtr left;
    ExpressionPtr right;
};

struct LogicalExpression : Expression {
    enum class Operator { And, Or, NullishCoalescing };
    Operator op{Operator::And};
    ExpressionPtr left;
    ExpressionPtr right;
};

struct AssignmentExpression : Expression {
    enum class Operator {
        Assign,
        AddAssign,
        SubtractAssign,
        MultiplyAssign,
        DivideAssign,
        ModuloAssign,
        ExponentAssign,
        LeftShiftAssign,
        RightShiftAssign,
        UnsignedRightShiftAssign,
        BitwiseAndAssign,
        BitwiseOrAssign,
        BitwiseXorAssign,
        LogicalAndAssign,
        LogicalOrAssign,
        NullishAssign
    };
    Operator op{Operator::Assign};
    ExpressionPtr left;
    ExpressionPtr right;
};

struct ConditionalExpression : Expression {
    ExpressionPtr test;
    ExpressionPtr consequent;
    ExpressionPtr alternate;
};

// Function expressions (including arrow functions)
struct FunctionExpression : Expression {
    std::optional<String> name;
    std::vector<String> params;
    std::vector<StatementPtr> body;
    bool is_arrow{false};
    bool expression_body{false};
    ExpressionPtr concise_body;
};

struct UpdateExpression : Expression {
    enum class Operator { Increment, Decrement };
    Operator op{Operator::Increment};
    ExpressionPtr argument;
    bool prefix{false};
};

struct NewExpression : Expression {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
};

struct SpreadElement : Expression {
    ExpressionPtr argument;
};

struct TemplateElement : ASTNode {
    String value;
    bool tail{false};
};

struct TemplateLiteral : Expression {
    std::vector<TemplateElement> quasis;
    std::vector<ExpressionPtr> expressions;
};

// ============================================================================
// Statements
// ============================================================================

struct Statement : ASTNode {
    virtual ~Statement() = default;
};

struct EmptyStatement : Statement {};

struct ExpressionStatement : Statement {
    ExpressionPtr expression;
};

struct BlockStatement : Statement {
    std::vector<StatementPtr> body;
};

struct VariableDeclarator : ASTNode {
    String id;
    ExpressionPtr init;
};

struct VariableDeclaration : Statement {
    enum class Kind { Var, Let, Const };
    Kind kind{Kind::Var};
    std::vector<VariableDeclarator> declarations;
};

struct FunctionDeclaration : Statement {
    String name;
    std::vector<String> params;
    std::vector<StatementPtr> body;
};

struct ReturnStatement : Statement {
    ExpressionPtr argument;
};

struct BreakStatement : Statement {
    std::optional<String> label;
};

struct ContinueStatement : Statement {
    std::optional<String> label;
};

struct IfStatement : Statement {
    ExpressionPtr test;
    StatementPtr consequent;
    StatementPtr alternate;
};

struct WhileStatement : Statement {
    ExpressionPtr test;
    StatementPtr body;
};

struct DoWhileStatement : Statement {
    StatementPtr body;
    ExpressionPtr test;
};

struct ForStatement : Statement {
    StatementPtr init_statement;
    ExpressionPtr init_expression;
    ExpressionPtr test;
    ExpressionPtr update;
    StatementPtr body;
};

struct ForInStatement : Statement {
    String variable;                    // 迭代变量名 (如 "key")
    bool use_let{false};                // true: let, false: var
    bool use_const{false};              // true: const
    ExpressionPtr object;               // 被迭代的对象
    StatementPtr body;                  // 循环体
};

struct SwitchCase : ASTNode {
    ExpressionPtr test; // null for default
    std::vector<StatementPtr> consequent;
};

struct SwitchStatement : Statement {
    ExpressionPtr discriminant;
    std::vector<SwitchCase> cases;
};

struct ThrowStatement : Statement {
    ExpressionPtr argument;
};

struct TryStatement : Statement {
    StatementPtr block;
    String handler_param;
    StatementPtr handler;
    StatementPtr finalizer;
};

struct WithStatement : Statement {
    ExpressionPtr object;
    StatementPtr body;
};

struct ClassMethod : ASTNode {
    String key;
    std::vector<String> params;
    std::vector<StatementPtr> body;
    bool is_static{false};
};

struct ClassDeclaration : Statement {
    String name;
    ExpressionPtr super_class;
    std::vector<ClassMethod> body;
};

struct ImportSpecifier : ASTNode {
    String imported;
    String local;
    bool is_default{false};
    bool is_namespace{false};
};

struct ImportDeclaration : Statement {
    std::vector<ImportSpecifier> specifiers;
    String source;
};

struct ExportSpecifier : ASTNode {
    String local;
    String exported;
};

struct ExportNamedDeclaration : Statement {
    std::vector<ExportSpecifier> specifiers;
    StatementPtr declaration;
    String source;
};

struct ExportDefaultDeclaration : Statement {
    StatementPtr declaration;
    ExpressionPtr expression;
};

struct ExportAllDeclaration : Statement {
    String source;
    String exported_as;
};

// ============================================================================
// Program (root node)
// ============================================================================

struct Program : ASTNode {
    std::vector<StatementPtr> body;
};

} // namespace lithium::js
