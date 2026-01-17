#pragma once

#include "tokenizer.hpp"
#include "tree_builder.hpp"
#include "lithium/dom/document.hpp"
#include <memory>

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

    // Incremental parsing (document.write style)
    void begin();
    void write(const String& html);
    [[nodiscard]] RefPtr<dom::Document> finish();
    [[nodiscard]] RefPtr<dom::Document> document() const { return m_streaming_document; }

    // Scripting support
    void set_scripting_enabled(bool enabled) { m_scripting_enabled = enabled; }
    [[nodiscard]] bool scripting_enabled() const { return m_scripting_enabled; }
    using ScriptCallback = std::function<void(const String& script_text)>;
    void set_script_callback(ScriptCallback callback) { m_script_callback = std::move(callback); }

    // Mode flags
    void set_parser_cannot_change_mode(bool value) { m_parser_cannot_change_mode = value; }
    void set_iframe_srcdoc(bool value) { m_is_iframe_srcdoc = value; }
    void set_transport_charset(const String& value) { m_transport_charset = value.to_lowercase(); }

    // Error handling
    using ErrorCallback = std::function<void(const String& message, usize line, usize column)>;
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }

    // Get parse errors
    [[nodiscard]] const std::vector<String>& errors() const { return m_errors; }
    [[nodiscard]] usize reparse_count() const { return m_reparse_count; }

private:
    void on_parse_error(const String& message);
    void process_streaming_tokens(bool mark_end_of_stream);
    void reinitialize_streaming_with_charset(const String& charset);

    bool m_scripting_enabled{false};
    bool m_parser_cannot_change_mode{false};
    bool m_is_iframe_srcdoc{false};
    ErrorCallback m_error_callback;
    std::vector<String> m_errors;

    // Streaming/document.write helpers
    RefPtr<dom::Document> m_streaming_document;
    std::unique_ptr<Tokenizer> m_streaming_tokenizer;
    std::unique_ptr<TreeBuilder> m_streaming_builder;
    bool m_streaming_open{false};
    bool m_seen_first_chunk{false};
    bool m_in_script_callback{false};
    bool m_collecting_script{false};
    StringBuilder m_script_buffer;
    ScriptCallback m_script_callback;
    String m_transport_charset;
    usize m_reparse_count{0};
    String m_current_charset{"utf-8"};
    String m_streaming_charset{"utf-8"};
    String m_streaming_raw_html;
    bool m_streaming_reparsed{false};
    bool m_streaming_from_bom{false};
    bool m_streaming_from_transport{false};
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
