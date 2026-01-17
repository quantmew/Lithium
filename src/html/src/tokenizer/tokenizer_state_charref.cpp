/**
 * HTML Tokenizer - character reference handling
 */

#include "lithium/html/tokenizer.hpp"
#include <array>
#include <cctype>
#include <string_view>

namespace lithium::html {

namespace {

inline bool is_attribute_value_state(TokenizerState state) {
    return state == TokenizerState::AttributeValueDoubleQuoted
        || state == TokenizerState::AttributeValueSingleQuoted
        || state == TokenizerState::AttributeValueUnquoted;
}

inline u32 hex_value(unicode::CodePoint cp) {
    if (cp >= '0' && cp <= '9') {
        return static_cast<u32>(cp - '0');
    }
    if (cp >= 'a' && cp <= 'f') {
        return static_cast<u32>(10 + cp - 'a');
    }
    if (cp >= 'A' && cp <= 'F') {
        return static_cast<u32>(10 + cp - 'A');
    }
    return 0;
}

inline unicode::CodePoint sanitize_numeric_code(u32 code) {
    if (code == 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
        return 0xFFFD;
    }

    // Replace control characters (except whitespace) with replacement char
    if ((code <= 0x1F || (code >= 0x7F && code <= 0x9F)) &&
        code != '\t' && code != '\n' && code != '\r') {
        return 0xFFFD;
    }

    return static_cast<unicode::CodePoint>(code);
}

} // namespace

void Tokenizer::handle_character_reference_state() {
    auto append = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(cp));
        } else {
            emit_character(cp);
        }
    };

    auto flush_temp_buffer = [&] {
        auto built = m_temp_buffer.build();
        for (char ch : built.std_string()) {
            append(static_cast<unsigned char>(ch));
        }
    };

    m_temp_buffer.clear();
    m_temp_buffer.append('&');

    auto cp = peek();
    if (!cp) {
        flush_temp_buffer();
        m_state = m_return_state;
        return;
    }

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ' || *cp == '<' || *cp == '&') {
        flush_temp_buffer();
        m_state = m_return_state;
        return;
    }

    if (*cp == '#') {
        consume();
        m_temp_buffer.append('#');
        m_character_reference_code = 0;
        m_state = TokenizerState::NumericCharacterReference;
    } else if (std::isalnum(*cp)) {
        m_state = TokenizerState::NamedCharacterReference;
    } else {
        flush_temp_buffer();
        m_state = m_return_state;
    }
}

void Tokenizer::handle_named_character_reference_state() {
    auto append = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(cp));
        } else {
            emit_character(cp);
        }
    };

    static const std::array<std::pair<std::string_view, unicode::CodePoint>, 5> named_entities{{
        {"amp", '&'},
        {"lt", '<'},
        {"gt", '>'},
        {"quot", '"'},
        {"apos", '\''},
    }};

    std::optional<std::pair<std::string_view, unicode::CodePoint>> match;
    usize matched_length = 0;

    for (const auto& entity : named_entities) {
        auto name_length = entity.first.size();
        if (m_position + name_length >= m_input.length()) {
            continue;
        }

        bool matches = true;
        for (usize i = 0; i < name_length; ++i) {
            if (m_input[m_position + i] != entity.first[i]) {
                matches = false;
                break;
            }
        }

        if (!matches) {
            continue;
        }

        if (m_position + name_length < m_input.length() && m_input[m_position + name_length] == ';') {
            match = entity;
            matched_length = name_length + 1; // include ';'
            break;
        }
    }

    if (match) {
        m_position += matched_length;
        append(match->second);
        m_state = m_return_state;
        return;
    }

    append('&');
    m_state = TokenizerState::AmbiguousAmpersand;
}

void Tokenizer::handle_ambiguous_ampersand_state() {
    auto append = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(cp));
        } else {
            emit_character(cp);
        }
    };

    auto cp = peek();
    if (!cp) {
        append('&');
        m_state = m_return_state;
        return;
    }

    consume();

    if (std::isalnum(*cp)) {
        append(*cp);
    } else if (*cp == ';') {
        parse_error("unknown-named-character-reference"_s);
        reconsume();
    } else {
        reconsume();
    }

    m_state = m_return_state;
}

void Tokenizer::handle_numeric_character_reference_state() {
    auto cp = peek();
    if (cp && (*cp == 'x' || *cp == 'X')) {
        consume();
        m_temp_buffer.append(*cp);
        m_state = TokenizerState::HexadecimalCharacterReferenceStart;
    } else {
        m_state = TokenizerState::DecimalCharacterReferenceStart;
    }
}

void Tokenizer::handle_hexadecimal_character_reference_start_state() {
    auto append = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(cp));
        } else {
            emit_character(cp);
        }
    };

    auto flush_temp_buffer = [&] {
        auto built = m_temp_buffer.build();
        for (char ch : built.std_string()) {
            append(static_cast<unsigned char>(ch));
        }
    };

    auto cp = peek();
    if (!cp || !std::isxdigit(*cp)) {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        flush_temp_buffer();
        m_state = m_return_state;
        return;
    }

    m_character_reference_code = 0;
    m_state = TokenizerState::HexadecimalCharacterReference;
}

void Tokenizer::handle_decimal_character_reference_start_state() {
    auto append = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(cp));
        } else {
            emit_character(cp);
        }
    };

    auto flush_temp_buffer = [&] {
        auto built = m_temp_buffer.build();
        for (char ch : built.std_string()) {
            append(static_cast<unsigned char>(ch));
        }
    };

    auto cp = peek();
    if (!cp || !std::isdigit(*cp)) {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        flush_temp_buffer();
        m_state = m_return_state;
        return;
    }

    m_character_reference_code = 0;
    m_state = TokenizerState::DecimalCharacterReference;
}

void Tokenizer::handle_hexadecimal_character_reference_state() {
    auto cp = peek();
    if (cp && std::isxdigit(*cp)) {
        consume();
        m_character_reference_code = (m_character_reference_code * 16) + hex_value(*cp);
        return;
    }

    if (cp && *cp == ';') {
        consume();
    } else {
        parse_error("missing-semicolon-after-character-reference"_s);
    }

    m_state = TokenizerState::NumericCharacterReferenceEnd;
}

void Tokenizer::handle_decimal_character_reference_state() {
    auto cp = peek();
    if (cp && std::isdigit(*cp)) {
        consume();
        m_character_reference_code = (m_character_reference_code * 10) + static_cast<u32>(*cp - '0');
        return;
    }

    if (cp && *cp == ';') {
        consume();
    } else {
        parse_error("missing-semicolon-after-character-reference"_s);
    }

    m_state = TokenizerState::NumericCharacterReferenceEnd;
}

void Tokenizer::handle_numeric_character_reference_end_state() {
    auto append = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(cp));
        } else {
            emit_character(cp);
        }
    };

    append(sanitize_numeric_code(m_character_reference_code));
    m_state = m_return_state;
}

unicode::CodePoint Tokenizer::consume_character_reference() {
    return '&';
}

unicode::CodePoint Tokenizer::consume_named_character_reference() {
    return '&';
}

unicode::CodePoint Tokenizer::consume_numeric_character_reference() {
    return '&';
}

} // namespace lithium::html
