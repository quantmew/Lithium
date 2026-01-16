/**
 * HTML Parser implementation
 */

#include "lithium/html/parser.hpp"

namespace lithium::html {

Parser::Parser() = default;

RefPtr<dom::Document> Parser::parse(const String& html) {
    auto document = make_ref<dom::Document>();

    Tokenizer tokenizer;
    tokenizer.set_input(html);
    tokenizer.set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });

    TreeBuilder builder;
    builder.set_document(document);
    builder.set_tokenizer(&tokenizer);
    builder.set_scripting_enabled(m_scripting_enabled);
    builder.set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });

    // Process tokens
    while (auto token = tokenizer.next_token()) {
        builder.process_token(*token);
        if (is_eof(*token)) {
            break;
        }
    }

    return document;
}

RefPtr<dom::Document> Parser::parse(std::string_view html) {
    return parse(String(html));
}

RefPtr<dom::DocumentFragment> Parser::parse_fragment(
    const String& /*html*/, dom::Element* /*context_element*/)
{
    // TODO: Implement fragment parsing
    return nullptr;
}

void Parser::on_parse_error(const String& message) {
    m_errors.push_back(message);
    if (m_error_callback) {
        m_error_callback(message, 0, 0);
    }
}

// ============================================================================
// Convenience functions
// ============================================================================

RefPtr<dom::Document> parse_html(const String& html) {
    Parser parser;
    return parser.parse(html);
}

RefPtr<dom::Document> parse_html(std::string_view html) {
    Parser parser;
    return parser.parse(html);
}

RefPtr<dom::DocumentFragment> parse_html_fragment(
    const String& html, dom::Element* context)
{
    Parser parser;
    return parser.parse_fragment(html, context);
}

} // namespace lithium::html
