/**
 * CSS Tokenizer implementation
 * Following CSS Syntax Module Level 3
 */

#include "lithium/css/tokenizer.hpp"
#include <cctype>
#include <cmath>

namespace lithium::css {

// ============================================================================
// Token type checking
// ============================================================================

bool is_ident(const Token& token) {
    return std::holds_alternative<IdentToken>(token);
}

bool is_ident_with_value(const Token& token, const String& value) {
    if (auto* ident = std::get_if<IdentToken>(&token)) {
        return ident->value.to_lowercase() == value.to_lowercase();
    }
    return false;
}

// ============================================================================
// Helper functions
// ============================================================================

static bool is_name_start_code_point(unicode::CodePoint cp) {
    return std::isalpha(cp) || cp > 0x7F || cp == '_';
}

static bool is_name_code_point(unicode::CodePoint cp) {
    return is_name_start_code_point(cp) || std::isdigit(cp) || cp == '-';
}

static bool is_non_printable(unicode::CodePoint cp) {
    return (cp >= 0x00 && cp <= 0x08) ||
           cp == 0x0B ||
           (cp >= 0x0E && cp <= 0x1F) ||
           cp == 0x7F;
}

static bool is_whitespace(unicode::CodePoint cp) {
    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f';
}

// ============================================================================
// Tokenizer implementation
// ============================================================================

Tokenizer::Tokenizer() = default;

void Tokenizer::set_input(const String& input) {
    m_input = input;
    m_position = 0;
    m_peeked.reset();
}

void Tokenizer::set_input(std::string_view input) {
    m_input = String(input);
    m_position = 0;
    m_peeked.reset();
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    while (!at_end()) {
        tokens.push_back(next_token());
        if (std::holds_alternative<EOFToken>(tokens.back())) {
            break;
        }
    }
    return tokens;
}

Token Tokenizer::next_token() {
    if (m_peeked) {
        Token token = std::move(*m_peeked);
        m_peeked.reset();
        return token;
    }
    return consume_token();
}

Token Tokenizer::peek_token() {
    if (!m_peeked) {
        m_peeked = consume_token();
    }
    return *m_peeked;
}

bool Tokenizer::at_end() const {
    return m_position >= m_input.length();
}

unicode::CodePoint Tokenizer::peek(usize offset) const {
    if (m_position + offset >= m_input.length()) {
        return 0;
    }
    return m_input[m_position + offset];
}

unicode::CodePoint Tokenizer::consume() {
    if (m_position >= m_input.length()) {
        return 0;
    }
    return m_input[m_position++];
}

void Tokenizer::reconsume() {
    if (m_position > 0) {
        --m_position;
    }
}

Token Tokenizer::consume_token() {
    consume_whitespace();

    if (at_end()) {
        return EOFToken{};
    }

    unicode::CodePoint cp = consume();

    // Comments
    if (cp == '/' && peek() == '*') {
        consume(); // consume *
        // Consume until */
        while (!at_end()) {
            if (consume() == '*' && peek() == '/') {
                consume();
                break;
            }
        }
        return consume_token(); // Consume next token after comment
    }

    // Whitespace
    if (is_whitespace(cp)) {
        while (!at_end() && is_whitespace(peek())) {
            consume();
        }
        return WhitespaceToken{};
    }

    // String
    if (cp == '"' || cp == '\'') {
        return consume_string_token(cp);
    }

    // Hash
    if (cp == '#') {
        if (is_name_code_point(peek()) || is_valid_escape()) {
            return consume_hash_token();
        }
        return DelimToken{cp};
    }

    // Plus
    if (cp == '+') {
        if (would_start_number(-1)) {
            reconsume();
            return consume_numeric_token();
        }
        return DelimToken{cp};
    }

    // Minus
    if (cp == '-') {
        if (would_start_number(-1)) {
            reconsume();
            return consume_numeric_token();
        }
        if (would_start_identifier(-1)) {
            reconsume();
            return consume_ident_like_token();
        }
        if (peek() == '-' && peek(1) == '>') {
            consume();
            consume();
            return CDCToken{};
        }
        return DelimToken{cp};
    }

    // Period
    if (cp == '.') {
        if (would_start_number(-1)) {
            reconsume();
            return consume_numeric_token();
        }
        return DelimToken{cp};
    }

    // Less-than
    if (cp == '<') {
        if (peek() == '!' && peek(1) == '-' && peek(2) == '-') {
            consume();
            consume();
            consume();
            return CDOToken{};
        }
        return DelimToken{cp};
    }

    // At
    if (cp == '@') {
        if (would_start_identifier()) {
            String name = consume_name();
            return AtKeywordToken{name};
        }
        return DelimToken{cp};
    }

    // Backslash
    if (cp == '\\') {
        if (is_valid_escape(-1)) {
            reconsume();
            return consume_ident_like_token();
        }
        // Parse error
        return DelimToken{cp};
    }

    // Colon
    if (cp == ':') {
        return ColonToken{};
    }

    // Semicolon
    if (cp == ';') {
        return SemicolonToken{};
    }

    // Comma
    if (cp == ',') {
        return CommaToken{};
    }

    // Brackets
    if (cp == '(') return OpenParenToken{};
    if (cp == ')') return CloseParenToken{};
    if (cp == '[') return OpenSquareToken{};
    if (cp == ']') return CloseSquareToken{};
    if (cp == '{') return OpenCurlyToken{};
    if (cp == '}') return CloseCurlyToken{};

    // Digit
    if (std::isdigit(cp)) {
        reconsume();
        return consume_numeric_token();
    }

    // Name start
    if (is_name_start_code_point(cp)) {
        reconsume();
        return consume_ident_like_token();
    }

    // Default: delimiter
    return DelimToken{cp};
}

Token Tokenizer::consume_ident_like_token() {
    String name = consume_name();

    // url(
    if (name.to_lowercase() == "url"_s && peek() == '(') {
        consume();
        // Skip whitespace
        while (is_whitespace(peek())) {
            consume();
        }
        if (peek() == '"' || peek() == '\'') {
            return FunctionToken{name};
        }
        return consume_url_token();
    }

    // function
    if (peek() == '(') {
        consume();
        return FunctionToken{name};
    }

    return IdentToken{name};
}

Token Tokenizer::consume_string_token(unicode::CodePoint ending) {
    StringBuilder result;

    while (!at_end()) {
        unicode::CodePoint cp = consume();

        if (cp == ending) {
            return StringToken{result.build()};
        }

        if (cp == '\n') {
            reconsume();
            return BadStringToken{};
        }

        if (cp == '\\') {
            if (at_end()) {
                continue;
            }
            unicode::CodePoint next = peek();
            if (next == '\n') {
                consume();
            } else {
                result.append(consume_escaped_code_point());
            }
        } else {
            result.append(cp);
        }
    }

    return StringToken{result.build()};
}

Token Tokenizer::consume_url_token() {
    StringBuilder result;

    // Skip whitespace
    while (is_whitespace(peek())) {
        consume();
    }

    while (!at_end()) {
        unicode::CodePoint cp = consume();

        if (cp == ')') {
            return UrlToken{result.build()};
        }

        if (is_whitespace(cp)) {
            while (is_whitespace(peek())) {
                consume();
            }
            if (peek() == ')' || at_end()) {
                if (!at_end()) consume();
                return UrlToken{result.build()};
            }
            consume_remnants_of_bad_url();
            return BadUrlToken{};
        }

        if (cp == '"' || cp == '\'' || cp == '(' || is_non_printable(cp)) {
            consume_remnants_of_bad_url();
            return BadUrlToken{};
        }

        if (cp == '\\') {
            if (is_valid_escape(-1)) {
                result.append(consume_escaped_code_point());
            } else {
                consume_remnants_of_bad_url();
                return BadUrlToken{};
            }
        } else {
            result.append(cp);
        }
    }

    return UrlToken{result.build()};
}

Token Tokenizer::consume_numeric_token() {
    auto [number, is_integer] = consume_number();

    if (would_start_identifier()) {
        String unit = consume_name();
        return DimensionToken{number, unit, is_integer};
    }

    if (peek() == '%') {
        consume();
        return PercentageToken{number};
    }

    return NumberToken{number, is_integer};
}

Token Tokenizer::consume_hash_token() {
    bool is_id = would_start_identifier();
    String name = consume_name();
    return HashToken{name, is_id};
}

bool Tokenizer::is_valid_escape(usize offset) const {
    usize pos = m_position + offset;
    if (pos >= m_input.length()) return false;
    if (m_input[pos] != '\\') return false;
    if (pos + 1 >= m_input.length()) return false;
    return m_input[pos + 1] != '\n';
}

bool Tokenizer::would_start_identifier(usize offset) const {
    usize pos = m_position + offset;
    if (pos >= m_input.length()) return false;

    unicode::CodePoint first = m_input[pos];

    if (is_name_start_code_point(first)) return true;

    if (first == '-') {
        if (pos + 1 >= m_input.length()) return false;
        unicode::CodePoint second = m_input[pos + 1];
        return is_name_start_code_point(second) || second == '-' || is_valid_escape(offset + 1);
    }

    if (first == '\\') {
        return is_valid_escape(offset);
    }

    return false;
}

bool Tokenizer::would_start_number(usize offset) const {
    usize pos = m_position + offset;
    if (pos >= m_input.length()) return false;

    unicode::CodePoint first = m_input[pos];

    if (first == '+' || first == '-') {
        if (pos + 1 >= m_input.length()) return false;
        unicode::CodePoint second = m_input[pos + 1];
        if (std::isdigit(second)) return true;
        if (second == '.' && pos + 2 < m_input.length() && std::isdigit(m_input[pos + 2])) {
            return true;
        }
        return false;
    }

    if (first == '.') {
        if (pos + 1 >= m_input.length()) return false;
        return std::isdigit(m_input[pos + 1]);
    }

    return std::isdigit(first);
}

unicode::CodePoint Tokenizer::consume_escaped_code_point() {
    if (at_end()) {
        return 0xFFFD;
    }

    unicode::CodePoint cp = consume();

    if (std::isxdigit(cp)) {
        StringBuilder hex;
        hex.append(cp);
        int count = 1;
        while (!at_end() && count < 6 && std::isxdigit(peek())) {
            hex.append(consume());
            count++;
        }
        // Skip optional whitespace
        if (!at_end() && is_whitespace(peek())) {
            consume();
        }

        // Convert hex to code point
        unsigned long value = std::strtoul(hex.build().c_str(), nullptr, 16);
        if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
            return 0xFFFD;
        }
        return static_cast<unicode::CodePoint>(value);
    }

    return cp;
}

String Tokenizer::consume_name() {
    StringBuilder result;

    while (!at_end()) {
        unicode::CodePoint cp = peek();

        if (is_name_code_point(cp)) {
            result.append(consume());
        } else if (is_valid_escape()) {
            consume(); // consume backslash
            result.append(consume_escaped_code_point());
        } else {
            break;
        }
    }

    return result.build();
}

std::pair<f64, bool> Tokenizer::consume_number() {
    StringBuilder repr;
    bool is_integer = true;

    // Sign
    if (peek() == '+' || peek() == '-') {
        repr.append(consume());
    }

    // Integer part
    while (!at_end() && std::isdigit(peek())) {
        repr.append(consume());
    }

    // Fractional part
    if (peek() == '.' && m_position + 1 < m_input.length() && std::isdigit(m_input[m_position + 1])) {
        repr.append(consume()); // .
        is_integer = false;
        while (!at_end() && std::isdigit(peek())) {
            repr.append(consume());
        }
    }

    // Exponent part
    if (peek() == 'e' || peek() == 'E') {
        usize saved_pos = m_position;
        repr.append(consume());

        if (peek() == '+' || peek() == '-') {
            repr.append(consume());
        }

        if (!at_end() && std::isdigit(peek())) {
            is_integer = false;
            while (!at_end() && std::isdigit(peek())) {
                repr.append(consume());
            }
        } else {
            // Not a valid exponent, restore
            m_position = saved_pos;
            repr = StringBuilder();
            // Re-parse without exponent
            if (m_input[saved_pos - 1] == '+' || m_input[saved_pos - 1] == '-') {
                // Hmm, simplify by just parsing the number as float
            }
        }
    }

    f64 value = std::strtod(repr.build().c_str(), nullptr);
    return {value, is_integer};
}

void Tokenizer::consume_whitespace() {
    while (!at_end() && is_whitespace(peek())) {
        consume();
    }
}

void Tokenizer::consume_remnants_of_bad_url() {
    while (!at_end()) {
        unicode::CodePoint cp = consume();
        if (cp == ')') return;
        if (is_valid_escape(-1)) {
            consume_escaped_code_point();
        }
    }
}

} // namespace lithium::css
