/**
 * HTML Parser implementation
 */

#include "lithium/html/parser.hpp"
#include <array>
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

enum class EncodingSource { Default, Transport, BOM, Meta };

String normalize_charset(const String& charset) {
    auto lower = charset.to_lowercase();
    if (lower == "utf8"_s) {
        return "utf-8"_s;
    }
    return lower;
}

bool is_supported_charset(const String& charset) {
    auto normalized = normalize_charset(charset);
    static const std::array<const char*, 4> SUPPORTED = {
        "utf-8", "windows-1252", "iso-8859-1", "shift_jis"
    };
    for (auto* candidate : SUPPORTED) {
        if (normalized == String(candidate)) {
            return true;
        }
    }
    return false;
}

struct EncodingDecision {
    String charset;
    String input;
    EncodingSource source{EncodingSource::Default};
    bool unsupported{false};
};

EncodingDecision determine_initial_encoding(const String& raw, const String& transport_charset) {
    EncodingDecision decision;
    decision.input = raw;

    bool has_utf8_bom = raw.view().size() >= 3 &&
        static_cast<unsigned char>(raw.view()[0]) == 0xEF &&
        static_cast<unsigned char>(raw.view()[1]) == 0xBB &&
        static_cast<unsigned char>(raw.view()[2]) == 0xBF;

    if (has_utf8_bom) {
        decision.input = strip_utf8_bom(raw);
        decision.charset = "utf-8"_s;
        decision.source = EncodingSource::BOM;
    } else if (!transport_charset.empty()) {
        decision.charset = normalize_charset(transport_charset);
        decision.source = EncodingSource::Transport;
    } else {
        auto sniffed = sniff_meta_charset(raw);
        if (sniffed.has_value()) {
            decision.charset = normalize_charset(*sniffed);
            decision.source = EncodingSource::Meta;
        } else {
            decision.charset = "utf-8"_s;
            decision.source = EncodingSource::Default;
        }
    }

    decision.input = strip_utf8_bom(decision.input);
    decision.unsupported = !is_supported_charset(decision.charset);
    return decision;
}

std::optional<String> charset_from_token(const Token& token) {
    if (!is_start_tag_named(token, "meta"_s)) {
        return std::nullopt;
    }

    auto& tag = std::get<TagToken>(token);
    if (auto charset = tag.get_attribute("charset"_s)) {
        return normalize_charset(*charset);
    }

    auto http_equiv = tag.get_attribute("http-equiv"_s);
    auto content = tag.get_attribute("content"_s);
    if (!http_equiv || !content) return std::nullopt;
    if (http_equiv->to_lowercase() != "content-type"_s) return std::nullopt;

    auto lower = content->to_lowercase();
    auto pos = lower.find("charset="_s);
    if (!pos.has_value()) return std::nullopt;
    usize start = *pos + 8;
    usize end = start;
    while (end < lower.length()) {
        char c = lower[end];
        if (unicode::is_ascii_whitespace(c) || c == ';' || c == '"' || c == '\'') {
            break;
        }
        ++end;
    }
    if (end <= start) return std::nullopt;
    return normalize_charset(lower.substring(start, end - start));
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
    m_reparse_count = 0;

    auto raw_input = html;
    auto initial_decision = determine_initial_encoding(raw_input, m_transport_charset);

    auto parse_round = [&](auto&& self, EncodingDecision decision, bool allow_reparse) -> RefPtr<dom::Document> {
        if (decision.unsupported) {
            on_parse_error("unsupported-encoding"_s);
        }

        m_current_charset = decision.charset;
        auto document = make_ref<dom::Document>();
        document->set_character_set(decision.charset);

        Tokenizer tokenizer;
        tokenizer.set_input(decision.input);
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

        m_collecting_script = false;
        m_script_buffer.clear();

        while (true) {
            tokenizer.set_in_foreign_content(builder.in_foreign_content());
            auto token = tokenizer.next_token();
            if (!token.has_value()) {
                break;
            }

            if (auto new_charset = charset_from_token(*token)) {
                auto normalized = normalize_charset(*new_charset);
                bool supported = is_supported_charset(normalized);
                if (!supported) {
                    on_parse_error("unsupported-encoding"_s);
                }
                if (normalized != decision.charset &&
                    supported &&
                    decision.source != EncodingSource::BOM &&
                    decision.source != EncodingSource::Transport &&
                    allow_reparse) {
                    ++m_reparse_count;
                    EncodingDecision next;
                    next.charset = normalized;
                    next.input = strip_utf8_bom(raw_input);
                    next.source = EncodingSource::Meta;
                    next.unsupported = !is_supported_charset(normalized);
                    m_errors.clear();
                    return self(self, next, false);
                } else if (normalized != decision.charset &&
                           (decision.source == EncodingSource::BOM || decision.source == EncodingSource::Transport)) {
                    on_parse_error("encoding-change-blocked"_s);
                }
            }

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
                    tokenizer.reset_after_script_execution();
                    m_in_script_callback = false;
                }
            }
            if (is_eof(*token)) {
                break;
            }
        }

        ensure_body_exists(document.get());
        return document;
    };

    return parse_round(parse_round, initial_decision, true);
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

    auto decision = determine_initial_encoding(html, m_transport_charset);
    if (decision.unsupported) {
        on_parse_error("unsupported-encoding"_s);
    }
    document->set_character_set(decision.charset);
    Tokenizer tokenizer;
    tokenizer.set_input(decision.input);

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
        tokenizer.set_in_foreign_content(builder.in_foreign_content());
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
    m_reparse_count = 0;
    m_streaming_raw_html.clear();
    m_streaming_reparsed = false;
    m_streaming_from_bom = false;
    m_streaming_from_transport = false;
    m_streaming_charset = "utf-8"_s;
    m_current_charset = m_streaming_charset;
    m_in_script_callback = false;
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
    m_collecting_script = false;
}

void Parser::reinitialize_streaming_with_charset(const String& charset) {
    m_streaming_charset = charset;
    m_current_charset = charset;
    m_streaming_from_bom = false;
    m_streaming_from_transport = false;
    m_seen_first_chunk = true;
    m_collecting_script = false;
    m_in_script_callback = false;

    m_streaming_document = make_ref<dom::Document>();
    m_streaming_document->set_character_set(charset);
    m_streaming_tokenizer = std::make_unique<Tokenizer>();
    m_streaming_tokenizer->enable_streaming(true);
    m_streaming_tokenizer->set_error_callback([this](const String& msg) {
        on_parse_error(msg);
    });
    m_streaming_tokenizer->set_input(strip_utf8_bom(m_streaming_raw_html));

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
}

void Parser::process_streaming_tokens(bool mark_end_of_stream) {
    if (!m_streaming_tokenizer || !m_streaming_builder) {
        return;
    }

    if (mark_end_of_stream) {
        m_streaming_tokenizer->mark_end_of_stream();
    }

    while (true) {
        m_streaming_tokenizer->set_in_foreign_content(m_streaming_builder->in_foreign_content());
        auto token = m_streaming_tokenizer->next_token();
        if (!token.has_value()) {
            break;
        }

        if (auto new_charset = charset_from_token(*token)) {
            auto normalized = normalize_charset(*new_charset);
            bool supported = is_supported_charset(normalized);
            if (!supported) {
                on_parse_error("unsupported-encoding"_s);
            }
            bool blocked = m_streaming_from_bom || m_streaming_from_transport;
            if (normalized != m_streaming_charset && supported && !blocked && !m_streaming_reparsed) {
                ++m_reparse_count;
                m_streaming_reparsed = true;
                reinitialize_streaming_with_charset(normalized);
                process_streaming_tokens(mark_end_of_stream);
                return;
            } else if (normalized != m_streaming_charset && blocked) {
                on_parse_error("encoding-change-blocked"_s);
            }
        }

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
                if (m_streaming_tokenizer) {
                    m_streaming_tokenizer->reset_after_script_execution();
                }
                m_in_script_callback = false;
            }
        }

        if (is_eof(*token)) {
            m_streaming_open = false;
            break;
        }
    }
}

void Parser::write(const String& html) {
    if (!m_streaming_open) {
        begin();
    }

    m_streaming_raw_html.append(html);

    if (!m_seen_first_chunk) {
        auto decision = determine_initial_encoding(m_streaming_raw_html, m_transport_charset);
        m_streaming_charset = decision.charset;
        m_current_charset = decision.charset;
        m_streaming_from_bom = decision.source == EncodingSource::BOM;
        m_streaming_from_transport = decision.source == EncodingSource::Transport;
        if (decision.unsupported) {
            on_parse_error("unsupported-encoding"_s);
        }
        m_streaming_document->set_character_set(decision.charset);
        m_streaming_tokenizer->set_input(decision.input);
        m_seen_first_chunk = true;
    } else if (m_in_script_callback && m_streaming_tokenizer) {
        m_streaming_tokenizer->insert_input_at_current_position(html);
    } else if (m_streaming_tokenizer) {
        m_streaming_tokenizer->append_input(html);
    }

    if (m_in_script_callback) {
        return;
    }

    process_streaming_tokens(false);
}

RefPtr<dom::Document> Parser::finish() {
    if (!m_streaming_open && m_streaming_document) {
        return m_streaming_document;
    }
    if (!m_streaming_document || !m_streaming_tokenizer || !m_streaming_builder) {
        return nullptr;
    }

    process_streaming_tokens(true);
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
