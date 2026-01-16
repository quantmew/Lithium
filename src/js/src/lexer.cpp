/**
 * JavaScript Lexer implementation
 */

#include "lithium/js/lexer.hpp"
#include <cctype>
#include <unordered_map>

namespace lithium::js {

// ============================================================================
// Token
// ============================================================================

bool Token::is_keyword() const {
    return type >= TokenType::Await && type <= TokenType::Enum;
}

bool Token::is_literal() const {
    return type >= TokenType::Null && type <= TokenType::RegExp;
}

bool Token::is_assignment_operator() const {
    return type >= TokenType::Assign && type <= TokenType::QuestionQuestionAssign;
}

bool Token::is_binary_operator() const {
    return (type >= TokenType::Plus && type <= TokenType::Caret) ||
           (type >= TokenType::AmpersandAmpersand && type <= TokenType::QuestionQuestion) ||
           type == TokenType::In || type == TokenType::Instanceof;
}

bool Token::is_unary_operator() const {
    return type == TokenType::Exclamation || type == TokenType::Tilde ||
           type == TokenType::Plus || type == TokenType::Minus ||
           type == TokenType::PlusPlus || type == TokenType::MinusMinus ||
           type == TokenType::Typeof || type == TokenType::Void ||
           type == TokenType::Delete || type == TokenType::Await;
}

// ============================================================================
// Lexer implementation
// ============================================================================

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"null", TokenType::Null},
    {"true", TokenType::True},
    {"false", TokenType::False},
    {"await", TokenType::Await},
    {"break", TokenType::Break},
    {"case", TokenType::Case},
    {"catch", TokenType::Catch},
    {"class", TokenType::Class},
    {"const", TokenType::Const},
    {"continue", TokenType::Continue},
    {"debugger", TokenType::Debugger},
    {"default", TokenType::Default},
    {"delete", TokenType::Delete},
    {"do", TokenType::Do},
    {"else", TokenType::Else},
    {"export", TokenType::Export},
    {"extends", TokenType::Extends},
    {"finally", TokenType::Finally},
    {"for", TokenType::For},
    {"function", TokenType::Function},
    {"if", TokenType::If},
    {"import", TokenType::Import},
    {"in", TokenType::In},
    {"instanceof", TokenType::Instanceof},
    {"let", TokenType::Let},
    {"new", TokenType::New},
    {"return", TokenType::Return},
    {"super", TokenType::Super},
    {"switch", TokenType::Switch},
    {"this", TokenType::This},
    {"throw", TokenType::Throw},
    {"try", TokenType::Try},
    {"typeof", TokenType::Typeof},
    {"var", TokenType::Var},
    {"void", TokenType::Void},
    {"while", TokenType::While},
    {"with", TokenType::With},
    {"yield", TokenType::Yield},
    {"enum", TokenType::Enum},
};

Lexer::Lexer() = default;

void Lexer::set_input(const String& source) {
    m_source = source;
    m_position = 0;
    m_line = 1;
    m_column = 1;
    m_peeked.reset();
}

void Lexer::set_input(std::string_view source) {
    m_source = String(source);
    m_position = 0;
    m_line = 1;
    m_column = 1;
    m_peeked.reset();
}

Token Lexer::next_token() {
    if (m_peeked) {
        Token token = std::move(*m_peeked);
        m_peeked.reset();
        return token;
    }
    return scan_token();
}

Token Lexer::peek_token() {
    if (!m_peeked) {
        m_peeked = scan_token();
    }
    return *m_peeked;
}

unicode::CodePoint Lexer::peek(usize offset) const {
    if (m_position + offset >= m_source.length()) {
        return 0;
    }
    return m_source[m_position + offset];
}

unicode::CodePoint Lexer::consume() {
    if (m_position >= m_source.length()) {
        return 0;
    }

    unicode::CodePoint cp = m_source[m_position++];

    if (cp == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }

    return cp;
}

void Lexer::reconsume() {
    if (m_position > 0) {
        m_position--;
        // Simplified: don't track line/column backwards
    }
}

bool Lexer::consume_if(unicode::CodePoint expected) {
    if (peek() == expected) {
        consume();
        return true;
    }
    return false;
}

bool Lexer::consume_if(std::string_view str) {
    if (m_position + str.length() > m_source.length()) {
        return false;
    }

    for (usize i = 0; i < str.length(); ++i) {
        if (m_source[m_position + i] != static_cast<unicode::CodePoint>(str[i])) {
            return false;
        }
    }

    for (usize i = 0; i < str.length(); ++i) {
        consume();
    }
    return true;
}

Token Lexer::scan_token() {
    skip_whitespace_and_comments();

    m_token_start = m_position;
    m_token_start_line = m_line;
    m_token_start_column = m_column;

    if (at_end()) {
        return make_token(TokenType::EndOfFile);
    }

    unicode::CodePoint cp = peek();

    // String
    if (cp == '"' || cp == '\'') {
        return scan_string(consume());
    }

    // Template literal
    if (cp == '`') {
        return scan_template();
    }

    // Number
    if (std::isdigit(cp) || (cp == '.' && std::isdigit(peek(1)))) {
        return scan_number();
    }

    // Identifier or keyword
    if (is_identifier_start(cp)) {
        return scan_identifier_or_keyword();
    }

    // Punctuator
    return scan_punctuator();
}

Token Lexer::scan_identifier_or_keyword() {
    StringBuilder ident;

    while (!at_end() && is_identifier_part(peek())) {
        ident.append(consume());
    }

    String name = ident.build();
    auto it = KEYWORDS.find(std::string(name.c_str()));
    if (it != KEYWORDS.end()) {
        return make_token(it->second, name);
    }

    return make_token(TokenType::Identifier, name);
}

Token Lexer::scan_number() {
    StringBuilder num;
    bool is_float = false;

    // Handle hex, octal, binary
    if (peek() == '0') {
        num.append(consume());
        if (peek() == 'x' || peek() == 'X') {
            num.append(consume());
            while (!at_end() && std::isxdigit(peek())) {
                num.append(consume());
            }
            Token token = make_token(TokenType::Number, num.build());
            token.number_value = static_cast<f64>(std::strtoll(num.build().c_str() + 2, nullptr, 16));
            token.is_integer = true;
            return token;
        }
        if (peek() == 'o' || peek() == 'O') {
            num.append(consume());
            while (!at_end() && peek() >= '0' && peek() <= '7') {
                num.append(consume());
            }
            Token token = make_token(TokenType::Number, num.build());
            token.number_value = static_cast<f64>(std::strtoll(num.build().c_str() + 2, nullptr, 8));
            token.is_integer = true;
            return token;
        }
        if (peek() == 'b' || peek() == 'B') {
            num.append(consume());
            while (!at_end() && (peek() == '0' || peek() == '1')) {
                num.append(consume());
            }
            Token token = make_token(TokenType::Number, num.build());
            token.number_value = static_cast<f64>(std::strtoll(num.build().c_str() + 2, nullptr, 2));
            token.is_integer = true;
            return token;
        }
    }

    // Integer part
    while (!at_end() && std::isdigit(peek())) {
        num.append(consume());
    }

    // Fractional part
    if (peek() == '.' && std::isdigit(peek(1))) {
        is_float = true;
        num.append(consume()); // .
        while (!at_end() && std::isdigit(peek())) {
            num.append(consume());
        }
    }

    // Exponent
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        num.append(consume());
        if (peek() == '+' || peek() == '-') {
            num.append(consume());
        }
        while (!at_end() && std::isdigit(peek())) {
            num.append(consume());
        }
    }

    Token token = make_token(TokenType::Number, num.build());
    token.number_value = std::strtod(num.build().c_str(), nullptr);
    token.is_integer = !is_float;
    return token;
}

Token Lexer::scan_string(unicode::CodePoint quote) {
    StringBuilder str;

    while (!at_end() && peek() != quote) {
        unicode::CodePoint cp = consume();

        if (cp == '\n' || cp == '\r') {
            error("Unterminated string literal"_s);
            break;
        }

        if (cp == '\\') {
            if (at_end()) break;

            unicode::CodePoint escape = consume();
            switch (escape) {
                case 'n': str.append('\n'); break;
                case 'r': str.append('\r'); break;
                case 't': str.append('\t'); break;
                case 'b': str.append('\b'); break;
                case 'f': str.append('\f'); break;
                case 'v': str.append('\v'); break;
                case '0': str.append('\0'); break;
                case '\\': str.append('\\'); break;
                case '\'': str.append('\''); break;
                case '"': str.append('"'); break;
                case 'u': str.append(scan_unicode_escape()); break;
                case 'x': str.append(scan_hex_escape(2)); break;
                default: str.append(escape); break;
            }
        } else {
            str.append(cp);
        }
    }

    if (!at_end() && peek() == quote) {
        consume();
    }

    return make_token(TokenType::String, str.build());
}

Token Lexer::scan_template() {
    consume(); // `
    StringBuilder str;
    bool is_head = true;

    while (!at_end()) {
        unicode::CodePoint cp = peek();

        if (cp == '`') {
            consume();
            return make_token(is_head ? TokenType::NoSubstitutionTemplate : TokenType::TemplateTail, str.build());
        }

        if (cp == '$' && peek(1) == '{') {
            consume(); consume();
            return make_token(is_head ? TokenType::TemplateHead : TokenType::TemplateMiddle, str.build());
        }

        if (cp == '\\') {
            consume();
            if (!at_end()) {
                str.append(consume());
            }
        } else {
            str.append(consume());
        }
    }

    error("Unterminated template literal"_s);
    return make_token(TokenType::Invalid);
}

Token Lexer::scan_regexp() {
    // Simplified: just scan until /
    StringBuilder pattern;
    StringBuilder flags;

    while (!at_end() && peek() != '/') {
        unicode::CodePoint cp = consume();
        if (cp == '\\' && !at_end()) {
            pattern.append(cp);
            pattern.append(consume());
        } else {
            pattern.append(cp);
        }
    }

    if (!at_end()) consume(); // closing /

    // Flags
    while (!at_end() && is_identifier_part(peek())) {
        flags.append(consume());
    }

    return make_token(TokenType::RegExp, pattern.build());
}

Token Lexer::scan_punctuator() {
    unicode::CodePoint cp = consume();

    switch (cp) {
        case '{': return make_token(TokenType::OpenBrace);
        case '}': return make_token(TokenType::CloseBrace);
        case '(': return make_token(TokenType::OpenParen);
        case ')': return make_token(TokenType::CloseParen);
        case '[': return make_token(TokenType::OpenBracket);
        case ']': return make_token(TokenType::CloseBracket);
        case ';': return make_token(TokenType::Semicolon);
        case ',': return make_token(TokenType::Comma);
        case '~': return make_token(TokenType::Tilde);
        case ':': return make_token(TokenType::Colon);

        case '.':
            if (consume_if("..")) return make_token(TokenType::Ellipsis);
            return make_token(TokenType::Dot);

        case '?':
            if (consume_if("?.")) return make_token(TokenType::OptionalChain);
            if (consume_if("??=")) return make_token(TokenType::QuestionQuestionAssign);
            if (consume_if("?")) return make_token(TokenType::QuestionQuestion);
            return make_token(TokenType::Question);

        case '<':
            if (consume_if("<=")){
                return make_token(TokenType::LeftShiftAssign);
            }
            if (consume_if("<")) return make_token(TokenType::LeftShift);
            if (consume_if("=")) return make_token(TokenType::LessEqual);
            return make_token(TokenType::LessThan);

        case '>':
            if (consume_if(">>>=")) return make_token(TokenType::UnsignedRightShiftAssign);
            if (consume_if(">>>")) return make_token(TokenType::UnsignedRightShift);
            if (consume_if(">>=")) return make_token(TokenType::RightShiftAssign);
            if (consume_if(">>")) return make_token(TokenType::RightShift);
            if (consume_if("=")) return make_token(TokenType::GreaterEqual);
            return make_token(TokenType::GreaterThan);

        case '=':
            if (consume_if("==")) return make_token(TokenType::StrictEqual);
            if (consume_if("=")) return make_token(TokenType::Equal);
            if (consume_if(">")) return make_token(TokenType::Arrow);
            return make_token(TokenType::Assign);

        case '!':
            if (consume_if("==")) return make_token(TokenType::StrictNotEqual);
            if (consume_if("=")) return make_token(TokenType::NotEqual);
            return make_token(TokenType::Exclamation);

        case '+':
            if (consume_if("+")) return make_token(TokenType::PlusPlus);
            if (consume_if("=")) return make_token(TokenType::PlusAssign);
            return make_token(TokenType::Plus);

        case '-':
            if (consume_if("-")) return make_token(TokenType::MinusMinus);
            if (consume_if("=")) return make_token(TokenType::MinusAssign);
            return make_token(TokenType::Minus);

        case '*':
            if (consume_if("*=")) return make_token(TokenType::StarStarAssign);
            if (consume_if("*")) return make_token(TokenType::StarStar);
            if (consume_if("=")) return make_token(TokenType::StarAssign);
            return make_token(TokenType::Star);

        case '/':
            if (consume_if("=")) return make_token(TokenType::SlashAssign);
            return make_token(TokenType::Slash);

        case '%':
            if (consume_if("=")) return make_token(TokenType::PercentAssign);
            return make_token(TokenType::Percent);

        case '&':
            if (consume_if("&=")) return make_token(TokenType::AmpersandAmpersandAssign);
            if (consume_if("&")) return make_token(TokenType::AmpersandAmpersand);
            if (consume_if("=")) return make_token(TokenType::AmpersandAssign);
            return make_token(TokenType::Ampersand);

        case '|':
            if (consume_if("|=")) return make_token(TokenType::PipePipeAssign);
            if (consume_if("|")) return make_token(TokenType::PipePipe);
            if (consume_if("=")) return make_token(TokenType::PipeAssign);
            return make_token(TokenType::Pipe);

        case '^':
            if (consume_if("=")) return make_token(TokenType::CaretAssign);
            return make_token(TokenType::Caret);

        default:
            return make_token(TokenType::Invalid);
    }
}

void Lexer::skip_whitespace_and_comments() {
    m_line_terminator_before = false;

    while (!at_end()) {
        unicode::CodePoint cp = peek();

        if (cp == ' ' || cp == '\t' || cp == '\r') {
            consume();
        } else if (cp == '\n') {
            consume();
            m_line_terminator_before = true;
        } else if (cp == '/' && peek(1) == '/') {
            skip_line_comment();
        } else if (cp == '/' && peek(1) == '*') {
            skip_block_comment();
        } else {
            break;
        }
    }
}

void Lexer::skip_line_comment() {
    consume(); consume(); // //
    while (!at_end() && peek() != '\n') {
        consume();
    }
}

void Lexer::skip_block_comment() {
    consume(); consume(); // /*
    while (!at_end()) {
        if (peek() == '*' && peek(1) == '/') {
            consume(); consume();
            return;
        }
        if (consume() == '\n') {
            m_line_terminator_before = true;
        }
    }
    error("Unterminated block comment"_s);
}

bool Lexer::is_identifier_start(unicode::CodePoint cp) const {
    return std::isalpha(cp) || cp == '_' || cp == '$' || cp > 127;
}

bool Lexer::is_identifier_part(unicode::CodePoint cp) const {
    return is_identifier_start(cp) || std::isdigit(cp);
}

unicode::CodePoint Lexer::scan_unicode_escape() {
    if (consume_if('{')) {
        // \u{XXXXXX}
        StringBuilder hex;
        while (!at_end() && peek() != '}') {
            hex.append(consume());
        }
        consume_if('}');
        return static_cast<unicode::CodePoint>(std::strtoul(hex.build().c_str(), nullptr, 16));
    }
    return scan_hex_escape(4);
}

unicode::CodePoint Lexer::scan_hex_escape(usize digits) {
    StringBuilder hex;
    for (usize i = 0; i < digits && !at_end(); ++i) {
        hex.append(consume());
    }
    return static_cast<unicode::CodePoint>(std::strtoul(hex.build().c_str(), nullptr, 16));
}

void Lexer::error(const String& message) {
    if (m_error_callback) {
        m_error_callback(message, m_line, m_column);
    }
}

Token Lexer::make_token(TokenType type) {
    Token token;
    token.type = type;
    token.line = m_token_start_line;
    token.column = m_token_start_column;
    token.start = m_token_start;
    token.end = m_position;
    token.preceded_by_line_terminator = m_line_terminator_before;
    return token;
}

Token Lexer::make_token(TokenType type, const String& value) {
    Token token = make_token(type);
    token.value = value;
    return token;
}

} // namespace lithium::js
