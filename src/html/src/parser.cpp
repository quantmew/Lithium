/**
 * HTML Parser implementation
 */

#include "lithium/html/parser.hpp"
#include <algorithm>

namespace lithium::html {

namespace {

String strip_utf8_bom(const String& input) {
    auto view = input.view();
    if (view.size() >= 3 && static_cast<unsigned char>(view[0]) == 0xEF &&
        static_cast<unsigned char>(view[1]) == 0xBB &&
        static_cast<unsigned char>(view[2]) == 0xBF) {
        return input.substring(3);
    }
    return input;
}

std::optional<String> sniff_meta_charset(const String& input) {
    // Best-effort sniff within the first 1024 bytes of the original input.
    auto limit = std::min<usize>(input.size(), 1024);
    auto lower = input.substring(0, limit).to_lowercase();

    auto find_charset = [&](const String& needle) -> std::optional<usize> {
        auto pos = lower.find(needle);
        if (pos.has_value()) return pos;
        return std::nullopt;
    };

    auto pos = find_charset("charset="_s);
    if (!pos.has_value()) {
        // Look for http-equiv=content-type and charset inside content
        auto http_equiv = find_charset("http-equiv=\"content-type\""_s);
        if (!http_equiv.has_value()) {
            http_equiv = find_charset("http-equiv='content-type'"_s);
        }
        if (http_equiv.has_value()) {
            pos = find_charset("charset="_s);
        }
    }
    if (!pos.has_value()) return std::nullopt;

    usize start = *pos + 8; // skip "charset="
    char quote = 0;
    if (start < lower.length() && (lower[start] == '"' || lower[start] == '\'')) {
        quote = lower[start];
        ++start;
    }
    usize end = start;
    while (end < lower.length()) {
        char c = lower[end];
        if ((quote && c == quote) || (!quote && (c == '"' || c == '\'')) ||
            unicode::is_ascii_whitespace(c) || c == ';') {
            break;
        }
        ++end;
    }
    if (end <= start) return std::nullopt;
    return lower.substring(start, end - start);
}

String preprocess_input(const String& raw, bool& unsupported_encoding) {
    String input = strip_utf8_bom(raw);
    auto charset = sniff_meta_charset(input);
    if (charset.has_value()) {
        if (*charset != "utf-8"_s && *charset != "utf8"_s) {
            unsupported_encoding = true;
        }
    }
    return input;
}

void ensure_body_exists(dom::Document* document) {
    if (!document) return;
    auto* html_element = document->document_element();
    if (html_element && !document->body()) {
        bool has_frameset = false;
        for (const auto& child : html_element->child_nodes()) {
            if (child->is_element() && child->as_element()->local_name() == "frameset"_s) {
                has_frameset = true;
                break;
            }
        }
        if (!has_frameset) {
            auto body = document->create_element("body"_s);
            html_element->append_child(body);
        }
    }
}

} // namespace

Parser::Parser() = default;

RefPtr<dom::Document> Parser::parse(const String& html) {
    m_errors.clear();
    bool unsupported = false;
    auto input = preprocess_input(html, unsupported);
    if (unsupported) {
        on_parse_error("unsupported-encoding"_s);
    }
    auto document = make_ref<dom::Document>();

    Tokenizer tokenizer;
    tokenizer.set_input(input);
    tokenizer.set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });

    TreeBuilder builder;
    builder.set_document(document);
    builder.set_tokenizer(&tokenizer);
    builder.set_scripting_enabled(m_scripting_enabled);
    builder.set_parser_cannot_change_mode(m_parser_cannot_change_mode);
    builder.set_iframe_srcdoc(m_is_iframe_srcdoc);
    builder.set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });

    // Process tokens
    m_collecting_script = false;
    while (auto token = tokenizer.next_token()) {
        builder.process_token(*token);
        if (m_script_callback) {
            if (is_start_tag_named(*token, "script"_s)) {
                m_collecting_script = true;
                m_script_buffer.clear();
            }
            if (m_collecting_script) {
                if (auto* ch = std::get_if<CharacterToken>(&*token)) {
                    m_script_buffer.append(ch->code_point);
                }
            }
            if (m_collecting_script && is_end_tag_named(*token, "script"_s)) {
                auto text = m_script_buffer.build();
                m_collecting_script = false;
                m_in_script_callback = true;
                m_script_callback(text);
                m_in_script_callback = false;
            }
        }
        if (is_eof(*token)) {
            break;
        }
    }

    ensure_body_exists(document.get());

    return document;
}

RefPtr<dom::Document> Parser::parse(std::string_view html) {
    return parse(String(html));
}

RefPtr<dom::DocumentFragment> Parser::parse_fragment(
    const String& html, dom::Element* context_element)
{
    m_errors.clear();
    auto document = make_ref<dom::Document>();
    auto fragment = document->create_document_fragment();

    // Clone a lightweight context element for parsing so we don't mutate the caller's tree.
    RefPtr<dom::Element> context_clone;
    if (context_element) {
        context_clone = document->create_element(context_element->local_name());
    } else {
        context_clone = document->create_element("div"_s);
    }

    bool unsupported = false;
    auto input = preprocess_input(html, unsupported);
    if (unsupported) {
        on_parse_error("unsupported-encoding"_s);
    }
    Tokenizer tokenizer;
    tokenizer.set_input(input);

    // Adjust tokenizer state based on context element per HTML fragment parsing algorithm.
    auto context_tag = context_clone->local_name();
    if (context_tag == "title"_s || context_tag == "textarea"_s) {
        tokenizer.set_state(TokenizerState::RCDATA);
    } else if (context_tag == "style"_s || context_tag == "xmp"_s ||
               context_tag == "iframe"_s || context_tag == "noembed"_s ||
               context_tag == "noframes"_s ||
               (context_tag == "noscript"_s && !m_scripting_enabled)) {
        tokenizer.set_state(TokenizerState::RAWTEXT);
    } else if (context_tag == "plaintext"_s) {
        tokenizer.set_state(TokenizerState::PLAINTEXT);
    } else if (context_tag == "script"_s) {
        tokenizer.set_state(TokenizerState::ScriptData);
    }

    tokenizer.set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });

    TreeBuilder builder;
    builder.set_document(document);
    builder.set_tokenizer(&tokenizer);
    builder.set_scripting_enabled(m_scripting_enabled);
    builder.set_parser_cannot_change_mode(true);
    builder.set_iframe_srcdoc(false);
    builder.set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });
    builder.prepare_for_fragment(context_clone);

    while (auto token = tokenizer.next_token()) {
        builder.process_token(*token);
        if (is_eof(*token)) {
            break;
        }
    }

    // Move children from the temporary context element into the returned fragment.
    auto* context_node = context_clone.get();
    while (context_node && context_node->first_child()) {
        auto child = RefPtr<dom::Node>(context_node->first_child());
        context_node->remove_child(child);
        fragment->append_child(child);
    }

    return fragment;
}

void Parser::begin() {
    m_errors.clear();
    m_streaming_document = make_ref<dom::Document>();
    m_streaming_tokenizer = std::make_unique<Tokenizer>();
    m_streaming_tokenizer->enable_streaming(true);
    m_streaming_tokenizer->set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });

    m_streaming_builder = std::make_unique<TreeBuilder>();
    m_streaming_builder->set_document(m_streaming_document);
    m_streaming_builder->set_tokenizer(m_streaming_tokenizer.get());
    m_streaming_builder->set_scripting_enabled(m_scripting_enabled);
    m_streaming_builder->set_parser_cannot_change_mode(m_parser_cannot_change_mode);
    m_streaming_builder->set_iframe_srcdoc(m_is_iframe_srcdoc);
    m_streaming_builder->set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });
    m_streaming_open = true;
    m_seen_first_chunk = false;
    m_streaming_detected_charset = false;
    m_streaming_unsupported = false;
    m_collecting_script = false;
}

void Parser::write(const String& html) {
    if (!m_streaming_open) {
        begin();
    }

    if (m_in_script_callback && m_streaming_tokenizer) {
        m_streaming_tokenizer->append_input(html);
        return;
    }
    String chunk = html;
    if (!m_seen_first_chunk) {
        bool unsupported = false;
        chunk = preprocess_input(html, unsupported);
        if (unsupported) {
            on_parse_error("unsupported-encoding"_s);
            m_streaming_unsupported = true;
        }
        m_seen_first_chunk = true;
        m_streaming_tokenizer->set_input(chunk);
    } else {
        m_streaming_tokenizer->append_input(chunk);
    }

    if (!m_streaming_detected_charset) {
        auto charset = sniff_meta_charset(chunk);
        if (charset.has_value()) {
            m_streaming_detected_charset = true;
            if (*charset != "utf-8"_s && *charset != "utf8"_s && !m_streaming_unsupported) {
                on_parse_error("unsupported-encoding"_s);
                m_streaming_unsupported = true;
            }
        }
    }

    if (m_in_script_callback) {
        return;
    }

    while (auto token = m_streaming_tokenizer->next_token()) {
        m_streaming_builder->process_token(*token);
        if (m_script_callback) {
            if (is_start_tag_named(*token, "script"_s)) {
                m_collecting_script = true;
                m_script_buffer.clear();
            }
            if (m_collecting_script) {
                if (auto* ch = std::get_if<CharacterToken>(&*token)) {
                    m_script_buffer.append(ch->code_point);
                }
            }
            if (m_collecting_script && is_end_tag_named(*token, "script"_s)) {
                auto text = m_script_buffer.build();
                m_collecting_script = false;
                m_in_script_callback = true;
                m_script_callback(text);
                m_in_script_callback = false;
            }
        }
        if (is_eof(*token)) {
            m_streaming_open = false;
            break;
        }
    }
}

RefPtr<dom::Document> Parser::finish() {
    if (!m_streaming_open && m_streaming_document) {
        return m_streaming_document;
    }
    if (!m_streaming_document || !m_streaming_tokenizer || !m_streaming_builder) {
        return nullptr;
    }

    m_streaming_tokenizer->mark_end_of_stream();
    while (auto token = m_streaming_tokenizer->next_token()) {
        m_streaming_builder->process_token(*token);
        if (m_script_callback) {
            if (is_start_tag_named(*token, "script"_s)) {
                m_collecting_script = true;
                m_script_buffer.clear();
            }
            if (m_collecting_script) {
                if (auto* ch = std::get_if<CharacterToken>(&*token)) {
                    m_script_buffer.append(ch->code_point);
                }
            }
            if (m_collecting_script && is_end_tag_named(*token, "script"_s)) {
                auto text = m_script_buffer.build();
                m_collecting_script = false;
                m_in_script_callback = true;
                m_script_callback(text);
                m_in_script_callback = false;
            }
        }
        if (is_eof(*token)) {
            break;
        }
    }
    ensure_body_exists(m_streaming_document.get());
    m_streaming_open = false;
    return m_streaming_document;
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
