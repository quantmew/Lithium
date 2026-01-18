/**
 * Test program to verify source location tracking in AST nodes
 */
#include "lithium/js/parser.hpp"
#include <iostream>

using namespace lithium::js;

void print_location(const char* node_type, const ASTNode* node) {
    if (!node) return;
    std::cout << node_type << " location: line "
              << node->location.start_line << ":"
              << node->location.start_column << " to "
              << node->location.end_line << ":"
              << node->location.end_column << std::endl;
}

int main() {
    Parser parser;

    // Test 1: Simple variable declaration
    std::cout << "=== Test 1: Variable Declaration ===" << std::endl;
    auto program1 = parser.parse("let x = 42;");
    print_location("Program", program1.get());
    if (!program1->body.empty()) {
        print_location("Statement", program1->body[0].get());
    }

    // Test 2: Function declaration
    std::cout << "\n=== Test 2: Function Declaration ===" << std::endl;
    auto program2 = parser.parse("function foo() { return 1; }");
    print_location("Program", program2.get());
    if (!program2->body.empty()) {
        print_location("FunctionDeclaration", program2->body[0].get());
    }

    // Test 3: Expression
    std::cout << "\n=== Test 3: Expression ===" << std::endl;
    auto expr = parser.parse_expression("x + y");
    print_location("Expression", expr.get());

    // Test 4: Multi-line
    std::cout << "\n=== Test 4: Multi-line ===" << std::endl;
    auto program3 = parser.parse("let a = 1;\nlet b = 2;\nlet c = a + b;");
    print_location("Program", program3.get());
    for (size_t i = 0; i < program3->body.size(); i++) {
        std::cout << "Statement " << i << ": ";
        print_location("", program3->body[i].get());
    }

    std::cout << "\n=== All tests completed ===" << std::endl;
    return 0;
}
