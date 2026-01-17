#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <memory>
#include <vector>
#include <optional>

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

// Base expression class
struct Expression : ASTNode {
    virtual ~Expression() = default;
};

// Expression pointer type
using ExpressionPtr = std::unique_ptr<Expression>;

// Literal expressions
struct NullLiteral : Expression {};
struct BooleanLiteral : Expression { bool value; };
struct NumericLiteral : Expression { f64 value; };
struct StringLiteral : Expression { String value; };
struct TemplateLiteral : Expression {
    std::vector<String> quasis;
    std::vector<ExpressionPtr> expressions;
};

// Identifier
struct Identifier : Expression {
    String name;
};

// This expression
struct ThisExpression : Expression {};

// Array/Object literals
struct ArrayExpression : Expression {
    std::vector<std::optional<ExpressionPtr>> elements;  // null for holes
};

struct Property : ASTNode {
    enum class Kind { Init, Get, Set };
    Kind kind{Kind::Init};
    ExpressionPtr key;
    ExpressionPtr value;
    bool computed{false};
    bool shorthand{false};
    bool method{false};
};

struct ObjectExpression : Expression {
    std::vector<std::unique_ptr<Property>> properties;
};

// Function expression
struct FunctionExpression : Expression {
    std::optional<String> name;
    std::vector<ExpressionPtr> params;  // Changed from PatternPtr
    ExpressionPtr body;  // Changed from StatementPtr
    bool is_async{false};
    bool is_generator{false};
};

// Arrow function
struct ArrowFunctionExpression : Expression {
    std::vector<ExpressionPtr> params;
    ExpressionPtr body;
    bool is_async{false};
    bool expression{false};  // body is expression vs block
};

// Class expression
struct ClassExpression : Expression {
    std::optional<String> name;
    ExpressionPtr super_class;
    std::vector<std::unique_ptr<Property>> body;
};

// Member expression
struct MemberExpression : Expression {
    ExpressionPtr object;
    ExpressionPtr property;
    bool computed{false};
    bool optional{false};
};

// Call expression
struct CallExpression : Expression {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
    bool optional{false};
};

// New expression
struct NewExpression : Expression {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
};

// Unary expression
struct UnaryExpression : Expression {
    enum class Operator {
        Minus, Plus, Not, BitwiseNot, Typeof, Void, Delete
    };
    Operator op;
    ExpressionPtr argument;
    bool prefix{true};
};

// Update expression (++/--)
struct UpdateExpression : Expression {
    enum class Operator { Increment, Decrement };
    Operator op;
    ExpressionPtr argument;
    bool prefix;
};

// Binary expression
struct BinaryExpression : Expression {
    enum class Operator {
        Add, Subtract, Multiply, Divide, Modulo, Exponent,
        Equal, NotEqual, StrictEqual, StrictNotEqual,
        LessThan, LessEqual, GreaterThan, GreaterEqual,
        LeftShift, RightShift, UnsignedRightShift,
        BitwiseAnd, BitwiseOr, BitwiseXor,
        In, Instanceof
    };
    Operator op;
    ExpressionPtr left;
    ExpressionPtr right;
};

// Logical expression
struct LogicalExpression : Expression {
    enum class Operator { And, Or, NullishCoalescing };
    Operator op;
    ExpressionPtr left;
    ExpressionPtr right;
};

// Conditional expression (ternary)
struct ConditionalExpression : Expression {
    ExpressionPtr test;
    ExpressionPtr consequent;
    ExpressionPtr alternate;
};

// Assignment expression
struct AssignmentExpression : Expression {
    enum class Operator {
        Assign, AddAssign, SubtractAssign, MultiplyAssign, DivideAssign,
        ModuloAssign, ExponentAssign, LeftShiftAssign, RightShiftAssign,
        UnsignedRightShiftAssign, BitwiseAndAssign, BitwiseOrAssign,
        BitwiseXorAssign, LogicalAndAssign, LogicalOrAssign, NullishAssign
    };
    Operator op;
    ExpressionPtr left;
    ExpressionPtr right;
};

// Sequence expression
struct SequenceExpression : Expression {
    std::vector<ExpressionPtr> expressions;
};

// Spread element
struct SpreadElement : Expression {
    ExpressionPtr argument;
};

// ============================================================================
// Patterns (for destructuring) - simplified to use Expression
// ============================================================================

// ============================================================================
// Statements
// ============================================================================

// Base statement class
struct Statement : ASTNode {
    virtual ~Statement() = default;
};

// Statement pointer type
using StatementPtr = std::unique_ptr<Statement>;

struct EmptyStatement : Statement {};

struct ExpressionStatement : Statement {
    ExpressionPtr expression;
};

struct BlockStatement : Statement {
    std::vector<StatementPtr> body;
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
    std::variant<std::monostate, StatementPtr, ExpressionPtr> init;
    ExpressionPtr test;
    ExpressionPtr update;
    StatementPtr body;
};

struct ForInStatement : Statement {
    ExpressionPtr left;
    ExpressionPtr right;
    StatementPtr body;
};

struct ForOfStatement : Statement {
    ExpressionPtr left;
    ExpressionPtr right;
    StatementPtr body;
    bool is_await{false};
};

struct BreakStatement : Statement {
    std::optional<String> label;
};

struct ContinueStatement : Statement {
    std::optional<String> label;
};

struct ReturnStatement : Statement {
    ExpressionPtr argument;
};

struct ThrowStatement : Statement {
    ExpressionPtr argument;
};

struct CatchClause : ASTNode {
    ExpressionPtr param;
    std::unique_ptr<BlockStatement> body;
};

struct TryStatement : Statement {
    std::unique_ptr<BlockStatement> block;
    std::unique_ptr<CatchClause> handler;
    std::unique_ptr<BlockStatement> finalizer;
};

struct SwitchCase : ASTNode {
    ExpressionPtr test;  // null for default
    std::vector<StatementPtr> consequent;
};

struct SwitchStatement : Statement {
    ExpressionPtr discriminant;
    std::vector<std::unique_ptr<SwitchCase>> cases;
};

struct LabeledStatement : Statement {
    String label;
    StatementPtr body;
};

struct DebuggerStatement : Statement {};

// Declarations
struct VariableDeclarator : ASTNode {
    ExpressionPtr id;
    ExpressionPtr init;
};

struct VariableDeclaration : Statement {
    enum class Kind { Var, Let, Const };
    Kind kind;
    std::vector<std::unique_ptr<VariableDeclarator>> declarations;
};

struct FunctionDeclaration : Statement {
    String name;
    std::vector<ExpressionPtr> params;
    std::unique_ptr<BlockStatement> body;
    bool is_async{false};
    bool is_generator{false};
};

struct ClassDeclaration : Statement {
    String name;
    ExpressionPtr super_class;
    std::vector<std::unique_ptr<Property>> body;
};

// ============================================================================
// Program (root node)
// ============================================================================

struct Program : ASTNode {
    enum class SourceType { Script, Module };
    SourceType source_type{SourceType::Script};
    std::vector<StatementPtr> body;
};

} // namespace lithium::js
