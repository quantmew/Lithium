/**
 * JavaScript REPL Tool
 * Interactive JavaScript interpreter
 */

#include "lithium/js/vm.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>
#include <string>

using namespace lithium;

int main(int argc, char* argv[]) {
    logging::init();
    logging::set_level(LogLevel::Warn);

    js::VM vm;

    // Register built-in console.log
    vm.define_native("print"_s, [](js::VM& vm, const std::vector<js::Value>& args) -> js::Value {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string().c_str();
        }
        std::cout << "\n";
        return js::Value::undefined();
    });

    std::cout << "Lithium JavaScript REPL v0.1.0\n";
    std::cout << "Type 'exit' to quit, 'help' for help\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line == "exit" || line == "quit") {
            break;
        }

        if (line == "help") {
            std::cout << "Available commands:\n";
            std::cout << "  exit, quit - Exit the REPL\n";
            std::cout << "  help       - Show this help\n";
            std::cout << "\nBuilt-in functions:\n";
            std::cout << "  print(...) - Print values to console\n";
            continue;
        }

        if (line.empty()) {
            continue;
        }

        auto result = vm.interpret(String(line));

        if (result == js::VM::InterpretResult::Ok) {
            // Print result if not undefined
            // (Would need to implement proper result return from VM)
        } else if (result == js::VM::InterpretResult::ParseError) {
            std::cerr << "Parse error: " << vm.error_message().c_str() << "\n";
        } else if (result == js::VM::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.error_message().c_str() << "\n";
        }
    }

    std::cout << "\nGoodbye!\n";
    logging::shutdown();
    return 0;
}
