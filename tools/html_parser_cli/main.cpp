/**
 * HTML Parser CLI Tool
 * Usage: lithium-html [file.html] or pipe HTML to stdin
 */

#include "lithium/html/parser.hpp"
#include "lithium/core/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace lithium;

void print_node(const dom::Node* node, int indent = 0) {
    if (!node) return;

    std::string prefix(indent * 2, ' ');

    if (node->is_element()) {
        auto* element = node->as_element();
        std::cout << prefix << "<" << element->tag_name().c_str();

        for (const auto& attr : element->attributes()) {
            std::cout << " " << attr.name.c_str() << "=\"" << attr.value.c_str() << "\"";
        }

        std::cout << ">\n";

        for (const auto& child : node->child_nodes()) {
            print_node(child.get(), indent + 1);
        }

        std::cout << prefix << "</" << element->tag_name().c_str() << ">\n";
    } else if (node->is_text()) {
        auto text = node->as_text()->data().trim();
        if (!text.empty()) {
            std::cout << prefix << "#text: \"" << text.c_str() << "\"\n";
        }
    } else if (node->node_type() == dom::NodeType::Comment) {
        std::cout << prefix << "<!-- comment -->\n";
    }
}

int main(int argc, char* argv[]) {
    logging::init();
    logging::set_level(LogLevel::Warn);

    String html;

    if (argc > 1) {
        // Read from file
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Error: Cannot open file: " << argv[1] << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        html = String(buffer.str());
    } else {
        // Read from stdin
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        html = String(buffer.str());
    }

    html::Parser parser;
    auto doc = parser.parse(html);

    if (!doc) {
        std::cerr << "Error: Failed to parse HTML\n";
        return 1;
    }

    std::cout << "=== DOM Tree ===\n";
    print_node(doc->document_element());

    if (!parser.errors().empty()) {
        std::cout << "\n=== Parse Errors ===\n";
        for (const auto& error : parser.errors()) {
            std::cout << "  - " << error.c_str() << "\n";
        }
    }

    logging::shutdown();
    return 0;
}
