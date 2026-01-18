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

    // Determine filename
    String filename = argc > 1 ? String(argv[1]) : "<stdin>"_s;

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

    auto result = vm.interpret(source, filename);

    auto error_type_name = [](js::ErrorType type) -> const char* {
        switch (type) {
            case js::ErrorType::ReferenceError: return "ReferenceError";
            case js::ErrorType::TypeError: return "TypeError";
            case js::ErrorType::SyntaxError: return "SyntaxError";
            case js::ErrorType::RangeError: return "RangeError";
            case js::ErrorType::URIError: return "URIError";
            case js::ErrorType::Error: return "Error";
            default: return nullptr;
        }
    };

    // V8-style error formatting
    auto print_diags = [&]() {
        for (const auto& d : vm.diagnostics()) {
            // For runtime errors with ErrorType, use V8 format
            if (d.stage == js::DiagnosticStage::Runtime && d.error_type != js::ErrorType::None) {
                // Print file:line if available
                if (!d.file.empty() && d.line > 0) {
                    std::cerr << d.file.c_str() << ":" << d.line << "\n";
                }

                // Print source line if available
                if (!d.source_line.empty()) {
                    std::cerr << d.source_line.c_str() << "\n";

                    // Print position indicator (^)
                    if (d.column > 0) {
                        for (usize i = 1; i < d.column; ++i) {
                            std::cerr << " ";
                        }
                        std::cerr << "^\n";
                    }
                }

                std::cerr << "\n";

                // Print error type and message
                const char* err_type = error_type_name(d.error_type);
                if (err_type) {
                    std::cerr << err_type << ": " << d.message.c_str() << "\n";
                } else {
                    std::cerr << d.message.c_str() << "\n";
                }

                // Print stack trace
                if (!d.stack_trace.empty()) {
                    for (const auto& frame : d.stack_trace) {
                        std::cerr << "    at ";
                        if (!frame.function_name.empty()) {
                            std::cerr << frame.function_name.c_str();
                        } else {
                            std::cerr << "<anonymous>";
                        }
                        std::cerr << " (";
                        if (!frame.file.empty()) {
                            std::cerr << frame.file.c_str();
                            if (frame.line > 0) {
                                std::cerr << ":" << frame.line;
                                if (frame.column > 0) {
                                    std::cerr << ":" << frame.column;
                                }
                            }
                        }
                        std::cerr << ")\n";
                    }
                }
            } else {
                // Old format for non-runtime errors
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
