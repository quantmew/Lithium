#include <gtest/gtest.h>
#include "lithium/core/string.hpp"
#include "lithium/js/parser.hpp"

using namespace lithium;
using namespace lithium::js;

namespace {

template<typename T>
T* assert_cast(const ExpressionPtr& ptr) {
    auto* result = dynamic_cast<T*>(ptr.get());
    EXPECT_NE(result, nullptr);
    return result;
}

} // namespace

TEST(JSParserTest, ParsesVariableDeclaration) {
    Parser parser;
    auto program = parser.parse("let x = 1 + 2;"_s);
    ASSERT_FALSE(parser.has_errors());
    ASSERT_EQ(program->body.size(), 1u);

    auto* decl = dynamic_cast<VariableDeclaration*>(program->body[0].get());
    ASSERT_NE(decl, nullptr);
    ASSERT_EQ(decl->declarations.size(), 1u);
    EXPECT_EQ(decl->declarations[0].id, "x"_s);

    auto init = std::move(decl->declarations[0].init);
    auto* binary = assert_cast<BinaryExpression>(init);
    EXPECT_EQ(binary->op, BinaryExpression::Operator::Add);
    auto* left = assert_cast<NumericLiteral>(binary->left);
    auto* right = assert_cast<NumericLiteral>(binary->right);
    EXPECT_DOUBLE_EQ(left->value, 1.0);
    EXPECT_DOUBLE_EQ(right->value, 2.0);
}

TEST(JSParserTest, ParsesFunctionDeclaration) {
    Parser parser;
    auto program = parser.parse("function add(a, b) { return a + b; }"_s);
    ASSERT_FALSE(parser.has_errors());
    ASSERT_EQ(program->body.size(), 1u);

    auto* fn = dynamic_cast<FunctionDeclaration*>(program->body[0].get());
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->name, "add"_s);
    ASSERT_EQ(fn->params.size(), 2u);
    EXPECT_EQ(fn->params[0], "a"_s);
    EXPECT_EQ(fn->params[1], "b"_s);
    ASSERT_EQ(fn->body.size(), 1u);

    auto* ret = dynamic_cast<ReturnStatement*>(fn->body[0].get());
    ASSERT_NE(ret, nullptr);
    auto* binary = assert_cast<BinaryExpression>(ret->argument);
    EXPECT_EQ(binary->op, BinaryExpression::Operator::Add);
}

TEST(JSParserTest, ParsesObjectLiteralWithComputedProperty) {
    Parser parser;
    auto expr = parser.parse_expression("({ a: 1, [b]: 2 })"_s);
    ASSERT_FALSE(parser.has_errors());

    auto* obj = dynamic_cast<ObjectExpression*>(expr.get());
    ASSERT_NE(obj, nullptr);
    ASSERT_EQ(obj->properties.size(), 2u);

    const auto& first = obj->properties[0];
    EXPECT_FALSE(first.computed);
    EXPECT_EQ(first.key, "a"_s);
    auto* one = assert_cast<NumericLiteral>(first.value);
    EXPECT_DOUBLE_EQ(one->value, 1.0);

    const auto& second = obj->properties[1];
    EXPECT_TRUE(second.computed);
    ASSERT_NE(second.computed_key, nullptr);
    auto* comp_id = assert_cast<Identifier>(second.computed_key);
    EXPECT_EQ(comp_id->name, "b"_s);
    auto* two = assert_cast<NumericLiteral>(second.value);
    EXPECT_DOUBLE_EQ(two->value, 2.0);
}

TEST(JSParserTest, ParsesArrowFunctionExpressionBody) {
    Parser parser;
    auto expr = parser.parse_expression("x => x * 2"_s);
    ASSERT_FALSE(parser.has_errors());

    auto* fn = dynamic_cast<FunctionExpression*>(expr.get());
    ASSERT_NE(fn, nullptr);
    EXPECT_TRUE(fn->is_arrow);
    EXPECT_TRUE(fn->expression_body);
    ASSERT_EQ(fn->params.size(), 1u);
    EXPECT_EQ(fn->params[0], "x"_s);
    ASSERT_NE(fn->concise_body, nullptr);
    auto* binary = assert_cast<BinaryExpression>(fn->concise_body);
    EXPECT_EQ(binary->op, BinaryExpression::Operator::Multiply);
}

TEST(JSParserTest, ConstRequiresInitializer) {
    Parser parser;
    auto program = parser.parse("const x;"_s);
    EXPECT_TRUE(parser.has_errors());
    EXPECT_EQ(program->body.size(), 1u); // parse still returns a program
}

TEST(JSParserTest, ParsesOptionalChainingAndNullish) {
    Parser parser;
    auto expr = parser.parse_expression("a?.b ?? c"_s);
    if (parser.has_errors()) {
        std::string msg;
        for (auto& e : parser.errors()) msg += e.std_string() + "; ";
        FAIL() << msg;
    }

    auto* logical = assert_cast<LogicalExpression>(expr);
    EXPECT_EQ(logical->op, LogicalExpression::Operator::NullishCoalescing);
    auto* member = assert_cast<MemberExpression>(logical->left);
    EXPECT_TRUE(member->optional);
    auto* id = assert_cast<Identifier>(member->object);
    EXPECT_EQ(id->name, "a"_s);
}

TEST(JSParserTest, DisallowsNullishMixingWithLogicalWithoutParens) {
    Parser parser;
    auto expr = parser.parse_expression("a ?? b || c"_s);
    (void)expr;
    EXPECT_TRUE(parser.has_errors());
}

TEST(JSParserTest, ParsesBitwiseAndShiftPrecedence) {
    Parser parser;
    auto expr = parser.parse_expression("1 + 2 << 1"_s);
    if (parser.has_errors()) {
        std::string msg;
        for (auto& e : parser.errors()) {
            msg += e.std_string();
            msg += "; ";
        }
        FAIL() << msg;
    }

    auto* shift = assert_cast<BinaryExpression>(expr);
    EXPECT_EQ(shift->op, BinaryExpression::Operator::LeftShift);
    auto* add = assert_cast<BinaryExpression>(shift->left);
    EXPECT_EQ(add->op, BinaryExpression::Operator::Add);
}

TEST(JSParserTest, ParsesExponentiationRightAssociative) {
    Parser parser;
    auto expr = parser.parse_expression("2 ** 3 ** 2"_s);
    ASSERT_FALSE(parser.has_errors());

    auto* outer = assert_cast<BinaryExpression>(expr);
    EXPECT_EQ(outer->op, BinaryExpression::Operator::Exponent);
    auto* inner = assert_cast<BinaryExpression>(outer->right);
    EXPECT_EQ(inner->op, BinaryExpression::Operator::Exponent);
}

TEST(JSParserTest, ParsesTemplateLiteralWithExpression) {
    Parser parser;
    auto expr = parser.parse_expression("`Hello ${name}!`"_s);
    if (parser.has_errors()) {
        std::string msg;
        for (auto& e : parser.errors()) msg += e.std_string() + "; ";
        FAIL() << msg;
    }

    auto* tmpl = assert_cast<TemplateLiteral>(expr);
    ASSERT_EQ(tmpl->quasis.size(), 2u);
    ASSERT_EQ(tmpl->expressions.size(), 1u);
    EXPECT_EQ(tmpl->quasis[0].value, "Hello "_s);
    EXPECT_EQ(tmpl->quasis[1].value, "!"_s);
}

TEST(JSParserTest, ParsesRegExpLiteral) {
    Parser parser;
    auto expr = parser.parse_expression("/ab+c/i"_s);
    ASSERT_FALSE(parser.has_errors());

    auto* re = assert_cast<RegExpLiteral>(expr);
    EXPECT_EQ(re->pattern, "ab+c"_s);
    EXPECT_EQ(re->flags, "i"_s);
}

TEST(JSParserTest, ParsesSpreadInArrayAndObject) {
    Parser parser;
    auto array_expr = parser.parse_expression("[1, ...rest]"_s);
    ASSERT_FALSE(parser.has_errors());
    auto* arr = assert_cast<ArrayExpression>(array_expr);
    ASSERT_EQ(arr->elements.size(), 2u);
    EXPECT_TRUE(dynamic_cast<SpreadElement*>(arr->elements[1].get()) != nullptr);

    auto obj_expr = parser.parse_expression("({ ...base, value: 2 })"_s);
    ASSERT_FALSE(parser.has_errors());
    auto* obj = assert_cast<ObjectExpression>(obj_expr);
    ASSERT_EQ(obj->properties.size(), 2u);
    EXPECT_TRUE(obj->properties[0].spread);
}

TEST(JSParserTest, ThisExpressionParsing) {
    Parser parser;
    auto expr = parser.parse_expression("this"_s);
    ASSERT_FALSE(parser.has_errors());

    // 'this' should be parsed as a ThisExpression, not a plain Identifier
    auto* this_expr = dynamic_cast<ThisExpression*>(expr.get());
    ASSERT_NE(this_expr, nullptr);
}
