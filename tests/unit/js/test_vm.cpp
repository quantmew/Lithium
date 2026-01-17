#include <gtest/gtest.h>
#include "lithium/js/vm.hpp"
#include "lithium/core/string.hpp"

using namespace lithium;
using namespace lithium::js;

class JSVmTest : public ::testing::Test {
protected:
    VM vm;

    Value run(const String& src) {
        auto result = vm.interpret(src);
        EXPECT_EQ(result, VM::InterpretResult::Ok) << vm.error_message().c_str();
        return vm.last_value();
    }
};

TEST_F(JSVmTest, EvaluatesArithmetic) {
    Value result = run("1 + 2 * 3;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 7.0);
}

TEST_F(JSVmTest, HandlesVariablesAndAssignment) {
    Value result = run("let a = 1; let b = 2; a = a + b;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 3.0);
}

TEST_F(JSVmTest, CallsFunctionAndReturnsValue) {
    Value result = run("function add(a, b) { return a + b; } add(2, 3);"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 5.0);
}

TEST_F(JSVmTest, SupportsClosures) {
    Value result = run(
        "function make(x) { return function(y) { return x + y; }; }"
        "let add1 = make(1);"
        "add1(5);"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 6.0);
}

TEST_F(JSVmTest, SupportsControlFlow) {
    Value result = run(
        "let i = 0; let sum = 0;"
        "while (i < 3) { sum = sum + i; i = i + 1; }"
        "sum;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 3.0);
}

TEST_F(JSVmTest, LogicalOperatorsFollowJsSemantics) {
    Value result1 = run("let v = null ?? 5; v;"_s);
    EXPECT_DOUBLE_EQ(result1.to_number(), 5.0);

    Value result2 = run("let a = 0 || 3; a;"_s);
    EXPECT_DOUBLE_EQ(result2.to_number(), 3.0);

    Value result3 = run("let b = 0 && 4; b;"_s);
    EXPECT_DOUBLE_EQ(result3.to_number(), 0.0);
}

TEST_F(JSVmTest, ObjectsAndMemberAssignment) {
    Value result = run(
        "let o = { a: 1 };"
        "o.a = o.a + 1;"
        "o.a;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 2.0);
}

TEST_F(JSVmTest, ArraysAndComputedMembers) {
    Value result = run(
        "let arr = [1, 2, 3];"
        "arr[1] = arr[1] + 5;"
        "arr[0] + arr[1] + arr.length;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 1 + 7 + 3);
}

TEST_F(JSVmTest, ArrowFunctionsExpressionBody) {
    Value result = run("(x => x * 3)(4);"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 12.0);
}

TEST_F(JSVmTest, RuntimeErrorOnConstAssignment) {
    auto status = vm.interpret("const x = 1; x = 2;"_s);
    EXPECT_EQ(status, VM::InterpretResult::RuntimeError);
}

TEST_F(JSVmTest, ParseErrorSurfaced) {
    auto status = vm.interpret("const y;"_s);
    EXPECT_EQ(status, VM::InterpretResult::ParseError);
}

TEST_F(JSVmTest, ForAndContinueBreak) {
    Value result = run(
        "let sum = 0;"
        "for (let i = 0; i < 5; i = i + 1) {"
        "  if (i == 2) continue;"
        "  if (i == 4) break;"
        "  sum = sum + i;"
        "}"
        "sum;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 4.0);
}

TEST_F(JSVmTest, DoWhileExecutesBodyBeforeCheck) {
    Value result = run(
        "let n = 0;"
        "do { n = n + 1; } while (false);"
        "n;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 1.0);
}

TEST_F(JSVmTest, SwitchCasesWithDefault) {
    Value result = run(
        "let x = 2; let r = 0;"
        "switch (x) {"
        "  case 1: r = 1; break;"
        "  case 2: r = 2; "
        "  default: r = r + 1;"
        "}"
        "r;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 3.0);
}

TEST_F(JSVmTest, TryCatchFinally) {
    Value result = run(
        "let flag = 0;"
        "try { throw 5; } catch (e) { flag = e; } finally { flag = flag + 1; }"
        "flag;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 6.0);
}

TEST_F(JSVmTest, ThrowUncaughtProducesRuntimeError) {
    auto status = vm.interpret("throw 3;"_s);
    EXPECT_EQ(status, VM::InterpretResult::RuntimeError);
}

TEST_F(JSVmTest, WithStatementAssignsToObject) {
    Value result = run(
        "let o = { a: 1 };"
        "with (o) { a = 5; }"
        "o.a;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 5.0);
}

TEST_F(JSVmTest, ExponentAndBitwiseOperators) {
    Value result = run("2 ** 3 + (5 | 1) << 1;"_s);
    EXPECT_DOUBLE_EQ(result.to_number(), 26.0);
}

TEST_F(JSVmTest, OptionalChainingReturnsUndefinedForNullishBase) {
    Value result = run("let o = null; o?.a;"_s);
    EXPECT_TRUE(result.is_undefined());

    Value nested = run("let o = { a: { b: 7 } }; o?.a?.b;"_s);
    EXPECT_DOUBLE_EQ(nested.to_number(), 7.0);
}

TEST_F(JSVmTest, TemplateLiteralProducesString) {
    Value result = run("let name = 'JS'; `Hello ${name}!`;"_s);
    EXPECT_EQ(result.to_string(), String("Hello JS!"));
}
