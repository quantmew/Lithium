#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <memory>
#include <vector>
#include <variant>
#include <optional>

namespace lithium::js {

// Forward declarations
struct Expression;
struct Statement;
struct Pattern;

using ExpressionPtr = std::unique_ptr<Expression>;
using StatementPtr = std::unique_ptr<Statement>;
using PatternPtr = std::unique_ptr<Pattern>;

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

// Literal expressions
struct NullLiteral : ASTNode {};
struct BooleanLiteral : ASTNode { bool value; };
struct NumericLiteral : ASTNode { f64 value; };
struct StringLiteral : ASTNode { String value; };
struct TemplateLiteral : ASTNode {
    std::vector<String> quasis;
    std::vector<ExpressionPtr> expressions;
};

// Identifier
struct Identifier : ASTNode {
    String name;
};

// This expression
struct ThisExpression : ASTNode {};

// Array/Object literals
struct ArrayExpression : ASTNode {
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

struct ObjectExpression : ASTNode {
    std::vector<std::unique_ptr<Property>> properties;
};

// Function expression
struct FunctionExpression : ASTNode {
    std::optional<String> name;
    std::vector<PatternPtr> params;
    StatementPtr body;
    bool is_async{false};
    bool is_generator{false};
};

// Arrow function
struct ArrowFunctionExpression : ASTNode {
    std::vector<PatternPtr> params;
    std::variant<ExpressionPtr, StatementPtr> body;
    bool is_async{false};
    bool expression{false};  // body is expression vs block
};

// Class expression
struct ClassExpression : ASTNode {
    std::optional<String> name;
    ExpressionPtr super_class;
    std::vector<std::unique_ptr<Property>> body;
};

// Member expression
struct MemberExpression : ASTNode {
    ExpressionPtr object;
    ExpressionPtr property;
    bool computed{false};
    bool optional{false};
};

// Call expression
struct CallExpression : ASTNode {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
    bool optional{false};
};

// New expression
struct NewExpression : ASTNode {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
};

// Unary expression
struct UnaryExpression : ASTNode {
    enum class Operator {
        Minus, Plus, Not, BitwiseNot, Typeof, Void, Delete
    };
    Operator op;
    ExpressionPtr argument;
    bool prefix{true};
};

// Update expression (++/--)
struct UpdateExpression : ASTNode {
    enum class Operator { Increment, Decrement };
    Operator op;
    ExpressionPtr argument;
    bool prefix;
};

// Binary expression
struct BinaryExpression : ASTNode {
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
struct LogicalExpression : ASTNode {
    enum class Operator { And, Or, NullishCoalescing };
    Operator op;
    ExpressionPtr left;
    ExpressionPtr right;
};

// Conditional expression (ternary)
struct ConditionalExpression : ASTNode {
    ExpressionPtr test;
    ExpressionPtr consequent;
    ExpressionPtr alternate;
};

// Assignment expression
struct AssignmentExpression : ASTNode {
    enum class Operator {
        Assign, AddAssign, SubtractAssign, MultiplyAssign, DivideAssign,
        ModuloAssign, ExponentAssign, LeftShiftAssign, RightShiftAssign,
        UnsignedRightShiftAssign, BitwiseAndAssign, BitwiseOrAssign,
        BitwiseXorAssign, LogicalAndAssign, LogicalOrAssign, NullishAssign
    };
    Operator op;
    std::variant<ExpressionPtr, PatternPtr> left;
    ExpressionPtr right;
};

// Sequence expression
struct SequenceExpression : ASTNode {
    std::vector<ExpressionPtr> expressions;
};

// Spread element
struct SpreadElement : ASTNode {
    ExpressionPtr argument;
};

// Expression variant
using Expression = std::variant<
    NullLiteral, BooleanLiteral, NumericLiteral, StringLiteral, TemplateLiteral,
    Identifier, ThisExpression, ArrayExpression, ObjectExpression,
    FunctionExpression, ArrowFunctionExpression, ClassExpression,
    MemberExpression, CallExpression, NewExpression,
    UnaryExpression, UpdateExpression, BinaryExpression, LogicalExpression,
    ConditionalExpression, AssignmentExpression, SequenceExpression, SpreadElement
>;

// ============================================================================
// Patterns (for destructuring)
// ============================================================================

struct ArrayPattern : ASTNode {
    std::vector<std::optional<PatternPtr>> elements;
};

struct ObjectPatternProperty : ASTNode {
    ExpressionPtr key;
    PatternPtr value;
    bool computed{false};
    bool shorthand{false};
};

struct ObjectPattern : ASTNode {
    std::vector<std::unique_ptr<ObjectPatternProperty>> properties;
};

struct AssignmentPattern : ASTNode {
    PatternPtr left;
    ExpressionPtr right;
};

struct RestElement : ASTNode {
    PatternPtr argument;
};

using Pattern = std::variant<
    Identifier, ArrayPattern, ObjectPattern, AssignmentPattern, RestElement
>;

// ============================================================================
// Statements
// ============================================================================

struct EmptyStatement : ASTNode {};

struct ExpressionStatement : ASTNode {
    ExpressionPtr expression;
};

struct BlockStatement : ASTNode {
    std::vector<StatementPtr> body;
};

struct IfStatement : ASTNode {
    ExpressionPtr test;
    StatementPtr consequent;
    StatementPtr alternate;
};

struct WhileStatement : ASTNode {
    ExpressionPtr test;
    StatementPtr body;
};

struct DoWhileStatement : ASTNode {
    StatementPtr body;
    ExpressionPtr test;
};

struct ForStatement : ASTNode {
    std::variant<std::monostate, StatementPtr, ExpressionPtr> init;
    ExpressionPtr test;
    ExpressionPtr update;
    StatementPtr body;
};

struct ForInStatement : ASTNode {
    std::variant<PatternPtr, ExpressionPtr> left;
    ExpressionPtr right;
    StatementPtr body;
};

struct ForOfStatement : ASTNode {
    std::variant<PatternPtr, ExpressionPtr> left;
    ExpressionPtr right;
    StatementPtr body;
    bool is_await{false};
};

struct BreakStatement : ASTNode {
    std::optional<String> label;
};

struct ContinueStatement : ASTNode {
    std::optional<String> label;
};

struct ReturnStatement : ASTNode {
    ExpressionPtr argument;
};

struct ThrowStatement : ASTNode {
    ExpressionPtr argument;
};

struct CatchClause : ASTNode {
    PatternPtr param;
    std::unique_ptr<BlockStatement> body;
};

struct TryStatement : ASTNode {
    std::unique_ptr<BlockStatement> block;
    std::unique_ptr<CatchClause> handler;
    std::unique_ptr<BlockStatement> finalizer;
};

struct SwitchCase : ASTNode {
    ExpressionPtr test;  // null for default
    std::vector<StatementPtr> consequent;
};

struct SwitchStatement : ASTNode {
    ExpressionPtr discriminant;
    std::vector<std::unique_ptr<SwitchCase>> cases;
};

struct LabeledStatement : ASTNode {
    String label;
    StatementPtr body;
};

struct DebuggerStatement : ASTNode {};

// Declarations
struct VariableDeclarator : ASTNode {
    PatternPtr id;
    ExpressionPtr init;
};

struct VariableDeclaration : ASTNode {
    enum class Kind { Var, Let, Const };
    Kind kind;
    std::vector<std::unique_ptr<VariableDeclarator>> declarations;
};

struct FunctionDeclaration : ASTNode {
    String name;
    std::vector<PatternPtr> params;
    std::unique_ptr<BlockStatement> body;
    bool is_async{false};
    bool is_generator{false};
};

struct ClassDeclaration : ASTNode {
    String name;
    ExpressionPtr super_class;
    std::vector<std::unique_ptr<Property>> body;
};

using Statement = std::variant<
    EmptyStatement, ExpressionStatement, BlockStatement,
    IfStatement, WhileStatement, DoWhileStatement,
    ForStatement, ForInStatement, ForOfStatement,
    BreakStatement, ContinueStatement, ReturnStatement, ThrowStatement,
    TryStatement, SwitchStatement, LabeledStatement, DebuggerStatement,
    VariableDeclaration, FunctionDeclaration, ClassDeclaration
>;

// ============================================================================
// Program (root node)
// ============================================================================

struct Program : ASTNode {
    enum class SourceType { Script, Module };
    SourceType source_type{SourceType::Script};
    std::vector<StatementPtr> body;
};

} // namespace lithium::js
