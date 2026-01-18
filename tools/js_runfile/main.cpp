/**
 * Simple JavaScript runner: interprets a file (or stdin) as a single script.
 */

#include "lithium/js/vm.hpp"
#include "lithium/js/diagnostic.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace lithium;

int main(int argc, char* argv[]) {
    logging::init();
    logging::set_level(LogLevel::Warn);

    String source;
    if (argc > 1) {
        std::ifstream in(argv[1], std::ios::in | std::ios::binary);
        if (!in) {
            std::cerr << "Failed to read file: " << argv[1] << "\n";
            return 1;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        source = String(ss.str());
    } else {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        source = String(ss.str());
    }

    js::VM vm;

    // Add print native function
    vm.define_native("print"_s, [](js::VM&, const std::vector<js::Value>& args) -> js::Value {
        for (usize i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].to_string().c_str();
        }
        std::cout << "\n";
        return js::Value::undefined();
    }, 1);

    // Add console.log
    auto console = std::make_shared<js::Object>();
    console->set_property("log"_s, js::Value(std::make_shared<js::NativeFunction>(
        "log"_s, [](js::VM&, const std::vector<js::Value>& args) -> js::Value {
            for (usize i = 0; i < args.size(); ++i) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].to_string().c_str();
            }
            std::cout << "\n";
            return js::Value::undefined();
        }, 1)));
    vm.define_native("console"_s, [console](js::VM&, const std::vector<js::Value>&) -> js::Value {
        return js::Value(console);
    }, 0);

    auto result = vm.interpret(source);
    auto print_diags = [&]() {
        for (const auto& d : vm.diagnostics()) {
            std::cout << "["
                      << (d.stage == js::DiagnosticStage::Lexer ? "lexer" :
                          d.stage == js::DiagnosticStage::Parser ? "parser" :
                          d.stage == js::DiagnosticStage::Compiler ? "compiler" :
                          d.stage == js::DiagnosticStage::Runtime ? "runtime" : "vm")
                      << "] "
                      << (d.level == js::DiagnosticLevel::Warning ? "warning" :
                          d.level == js::DiagnosticLevel::Info ? "info" : "error")
                      << " ";
            if (!d.file.empty()) {
                std::cout << d.file.c_str();
            }
            if (d.line) {
                std::cout << ":" << d.line;
                if (d.column) {
                    std::cout << ":" << d.column;
                }
                std::cout << " ";
            } else if (!d.file.empty()) {
                std::cout << " ";
            }
            std::cout << d.message.c_str() << "\n";
        }
    };

    if (result == js::VM::InterpretResult::Ok) {
        print_diags();
        std::cout << vm.last_value().debug_string().c_str() << "\n";
        return 0;
    }
    print_diags();
    std::cerr << (result == js::VM::InterpretResult::ParseError ? "Parse error: " : "Runtime error: ")
              << vm.error_message().c_str() << "\n";
    return 1;
}
