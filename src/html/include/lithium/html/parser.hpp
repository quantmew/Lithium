#pragma once

#include "tokenizer.hpp"
#include "tree_builder.hpp"
#include "lithium/dom/document.hpp"

namespace lithium::html {

// ============================================================================
// Parser - High-level HTML parsing interface
// ============================================================================

class Parser {
public:
    Parser();

    // Parse complete HTML document
    [[nodiscard]] RefPtr<dom::Document> parse(const String& html);
    [[nodiscard]] RefPtr<dom::Document> parse(std::string_view html);

    // Parse fragment (for innerHTML)
    [[nodiscard]] RefPtr<dom::DocumentFragment> parse_fragment(
        const String& html, dom::Element* context_element);

    // Scripting support
    void set_scripting_enabled(bool enabled) { m_scripting_enabled = enabled; }
    [[nodiscard]] bool scripting_enabled() const { return m_scripting_enabled; }

    // Error handling
    using ErrorCallback = std::function<void(const String& message, usize line, usize column)>;
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }

    // Get parse errors
    [[nodiscard]] const std::vector<String>& errors() const { return m_errors; }

private:
    void on_parse_error(const String& message);

    bool m_scripting_enabled{false};
    ErrorCallback m_error_callback;
    std::vector<String> m_errors;
};

// ============================================================================
// Convenience functions
// ============================================================================

// Parse a complete HTML document
[[nodiscard]] RefPtr<dom::Document> parse_html(const String& html);
[[nodiscard]] RefPtr<dom::Document> parse_html(std::string_view html);

// Parse an HTML fragment
[[nodiscard]] RefPtr<dom::DocumentFragment> parse_html_fragment(
    const String& html, dom::Element* context = nullptr);

} // namespace lithium::html
