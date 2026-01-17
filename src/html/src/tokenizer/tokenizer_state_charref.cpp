/**
 * HTML Tokenizer - character reference handling
 */

#include "lithium/html/tokenizer.hpp"
#include "entities.inc"
#include "lithium/core/string.hpp"
#include <cctype>

namespace lithium::html {

namespace {

inline bool is_attribute_value_state(TokenizerState state) {
    return state == TokenizerState::AttributeValueDoubleQuoted
        || state == TokenizerState::AttributeValueSingleQuoted
        || state == TokenizerState::AttributeValueUnquoted;
}

inline bool is_ascii_alnum_or_equal(unicode::CodePoint cp) {
    return unicode::is_ascii_alphanumeric(cp) || cp == '=';
}

inline unicode::CodePoint sanitize_numeric_code(u32 code) {
    // HTML numeric character reference replacement rules
    if (code == 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
        return unicode::REPLACEMENT_CHARACTER;
    }

    // Control character replacement map (spec table)
    switch (code) {
        case 0x80: return 0x20AC;
        case 0x82: return 0x201A;
        case 0x83: return 0x0192;
        case 0x84: return 0x201E;
        case 0x85: return 0x2026;
        case 0x86: return 0x2020;
        case 0x87: return 0x2021;
        case 0x88: return 0x02C6;
        case 0x89: return 0x2030;
        case 0x8A: return 0x0160;
        case 0x8B: return 0x2039;
        case 0x8C: return 0x0152;
        case 0x8E: return 0x017D;
        case 0x91: return 0x2018;
        case 0x92: return 0x2019;
        case 0x93: return 0x201C;
        case 0x94: return 0x201D;
        case 0x95: return 0x2022;
        case 0x96: return 0x2013;
        case 0x97: return 0x2014;
        case 0x98: return 0x02DC;
        case 0x99: return 0x2122;
        case 0x9A: return 0x0161;
        case 0x9B: return 0x203A;
        case 0x9C: return 0x0153;
        case 0x9E: return 0x017E;
        case 0x9F: return 0x0178;
        default:
            break;
    }

    // Other control characters (except whitespace) become replacement character
    if ((code >= 0x01 && code <= 0x1F) || (code >= 0x7F && code <= 0x9F)) {
        if (code != '\t' && code != '\n' && code != '\r') {
            return unicode::REPLACEMENT_CHARACTER;
        }
    }

    return static_cast<unicode::CodePoint>(code);
}

inline const detail::NamedEntity* match_named_entity(const String& input, usize position) {
    const detail::NamedEntity* best = nullptr;
    auto remaining = input.length() - position;

    for (const auto& entity : detail::NAMED_ENTITIES) {
        if (entity.name_length > remaining) continue;
        bool matches = true;
        for (usize i = 0; i < entity.name_length; ++i) {
            if (input[position + i] != entity.name[i]) {
                matches = false;
                break;
            }
        }
        if (!matches) continue;
        if (!best || entity.name_length > best->name_length) {
            best = &entity;
        }
    }
    return best;
}

} // namespace

void Tokenizer::handle_character_reference_state() {
    auto emit_literal_amp = [&] {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value.append("&"_s);
        } else {
            emit_character('&');
        }
    };

    auto cp = peek();
    if (!cp) {
        emit_literal_amp();
        m_state = m_return_state;
        return;
    }

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ' || *cp == '<' || *cp == '&') {
        emit_literal_amp();
        m_state = m_return_state;
        return;
    }

    if (*cp == '#') {
        consume();
        m_character_reference_code = 0;
        m_state = TokenizerState::NumericCharacterReference;
        return;
    }

    m_state = TokenizerState::NamedCharacterReference;
}

void Tokenizer::handle_named_character_reference_state() {
    auto emit_literal_amp = [&] {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value.append("&"_s);
        } else {
            emit_character('&');
        }
    };

    auto append_cp = [&](unicode::CodePoint cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value.append(String::from_code_point(cp));
        } else {
            emit_character(cp);
        }
    };

    const auto* entity = match_named_entity(m_input, m_position);
    if (!entity) {
        emit_literal_amp();
        m_state = m_return_state;
        return;
    }

    // Semicolon handling and legacy allowances
    usize consumed = entity->name_length;
    bool missing_semicolon = !entity->has_semicolon;
    unicode::CodePoint next = (m_position + consumed < m_input.length())
        ? static_cast<unsigned char>(m_input[m_position + consumed])
        : 0;

    bool should_reject = missing_semicolon &&
        (!entity->is_legacy || is_ascii_alnum_or_equal(next));

    if (should_reject) {
        emit_literal_amp();
        m_state = m_return_state;
        return;
    }

    if (missing_semicolon) {
        parse_error("missing-semicolon-after-character-reference"_s);
    }

    m_position += consumed;
    for (usize i = 0; i < entity->codepoint_length; ++i) {
        append_cp(static_cast<unicode::CodePoint>(entity->codepoints[i]));
    }
    m_state = m_return_state;
}

void Tokenizer::handle_ambiguous_ampersand_state() {
    // This state is retained for completeness but should be unreachable with the
    // current named character reference handling. Fall back to literal emission.
    auto cp = peek();
    if (!cp) {
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value.append("&"_s);
        } else {
            emit_character('&');
        }
        m_state = m_return_state;
        return;
    }

    consume();
    if (is_attribute_value_state(m_return_state)) {
        m_current_attribute_value.append(String(1, static_cast<char>(*cp)));
    } else {
        emit_character(*cp);
    }
    m_state = m_return_state;
}

void Tokenizer::handle_numeric_character_reference_state() {
    m_temp_buffer.clear();
    auto cp = peek();
    if (cp && (*cp == 'x' || *cp == 'X')) {
        consume();
        m_state = TokenizerState::HexadecimalCharacterReferenceStart;
    } else {
        m_state = TokenizerState::DecimalCharacterReferenceStart;
    }
}

void Tokenizer::handle_hexadecimal_character_reference_start_state() {
    auto cp = peek();
    if (!cp || !unicode::is_ascii_hex_digit(*cp)) {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value.append("&#x"_s);
        } else {
            emit_character('&');
            emit_character('#');
            emit_character('x');
        }
        m_state = m_return_state;
        return;
    }

    m_character_reference_code = 0;
    m_state = TokenizerState::HexadecimalCharacterReference;
}

void Tokenizer::handle_decimal_character_reference_start_state() {
    auto cp = peek();
    if (!cp || !unicode::is_ascii_digit(*cp)) {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        if (is_attribute_value_state(m_return_state)) {
            m_current_attribute_value.append("&#"_s);
        } else {
            emit_character('&');
            emit_character('#');
        }
        m_state = m_return_state;
        return;
    }

    m_character_reference_code = 0;
    m_state = TokenizerState::DecimalCharacterReference;
}

void Tokenizer::handle_hexadecimal_character_reference_state() {
    auto cp = peek();
    if (cp && unicode::is_ascii_hex_digit(*cp)) {
        consume();
        m_character_reference_code = (m_character_reference_code * 16)
            + static_cast<u32>(unicode::to_ascii_lower(*cp) >= 'a'
                ? 10 + (unicode::to_ascii_lower(*cp) - 'a')
                : (*cp - '0'));
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
    if (cp && unicode::is_ascii_digit(*cp)) {
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
    auto cp = sanitize_numeric_code(m_character_reference_code);
    if (is_attribute_value_state(m_return_state)) {
        m_current_attribute_value.append(String::from_code_point(cp));
    } else {
        emit_character(cp);
    }
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
