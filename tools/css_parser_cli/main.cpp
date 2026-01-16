/**
 * CSS Parser CLI Tool
 * Usage: lithium-css [file.css] or pipe CSS to stdin
 */

#include "lithium/css/parser.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace lithium;

int main(int argc, char* argv[]) {
    logging::init();
    logging::set_level(LogLevel::Warn);

    String css;

    if (argc > 1) {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Error: Cannot open file: " << argv[1] << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        css = String(buffer.str());
    } else {
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        css = String(buffer.str());
    }

    css::Parser parser;
    auto stylesheet = parser.parse_stylesheet(css);

    std::cout << "=== Stylesheet ===\n";
    std::cout << "Rules: " << stylesheet.rules.size() << "\n";

    auto style_rules = stylesheet.style_rules();
    std::cout << "Style Rules: " << style_rules.size() << "\n";

    for (const auto* rule : style_rules) {
        std::cout << "\n  Selector: (selector list)\n";
        std::cout << "  Declarations: " << rule->declarations.declarations.size() << "\n";

        for (const auto& decl : rule->declarations.declarations) {
            std::cout << "    " << decl.property.c_str() << ": ...";
            if (decl.important) std::cout << " !important";
            std::cout << "\n";
        }
    }

    if (!parser.errors().empty()) {
        std::cout << "\n=== Parse Errors ===\n";
        for (const auto& error : parser.errors()) {
            std::cout << "  - " << error.c_str() << "\n";
        }
    }

    logging::shutdown();
    return 0;
}
