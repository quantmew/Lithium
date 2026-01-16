#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <variant>
#include <vector>
#include <optional>

namespace lithium::css {

// ============================================================================
// CSS Token Types (CSS Syntax Module Level 3)
// ============================================================================

struct IdentToken { String value; };
struct FunctionToken { String value; };
struct AtKeywordToken { String value; };
struct HashToken { String value; bool is_id{false}; };
struct StringToken { String value; };
struct BadStringToken {};
struct UrlToken { String value; };
struct BadUrlToken {};
struct DelimToken { unicode::CodePoint value; };
struct NumberToken { f64 value; bool is_integer{false}; };
struct PercentageToken { f64 value; };
struct DimensionToken { f64 value; String unit; bool is_integer{false}; };
struct WhitespaceToken {};
struct CDOToken {};  // <!--
struct CDCToken {};  // -->
struct ColonToken {};
struct SemicolonToken {};
struct CommaToken {};
struct OpenSquareToken {};
struct CloseSquareToken {};
struct OpenParenToken {};
struct CloseParenToken {};
struct OpenCurlyToken {};
struct CloseCurlyToken {};
struct EOFToken {};

using Token = std::variant<
    IdentToken,
    FunctionToken,
    AtKeywordToken,
    HashToken,
    StringToken,
    BadStringToken,
    UrlToken,
    BadUrlToken,
    DelimToken,
    NumberToken,
    PercentageToken,
    DimensionToken,
    WhitespaceToken,
    CDOToken,
    CDCToken,
    ColonToken,
    SemicolonToken,
    CommaToken,
    OpenSquareToken,
    CloseSquareToken,
    OpenParenToken,
    CloseParenToken,
    OpenCurlyToken,
    CloseCurlyToken,
    EOFToken
>;

// Token type checking
template<typename T>
[[nodiscard]] bool is_token_type(const Token& token) {
    return std::holds_alternative<T>(token);
}

[[nodiscard]] bool is_ident(const Token& token);
[[nodiscard]] bool is_ident_with_value(const Token& token, const String& value);

// ============================================================================
// CSS Tokenizer
// ============================================================================

class Tokenizer {
public:
    Tokenizer();

    void set_input(const String& input);
    void set_input(std::string_view input);

    // Tokenize all input
    [[nodiscard]] std::vector<Token> tokenize();

    // Get next token
    [[nodiscard]] Token next_token();

    // Peek at next token without consuming
    [[nodiscard]] Token peek_token();

    // Check if at end
    [[nodiscard]] bool at_end() const;

private:
    // Character consumption
    [[nodiscard]] unicode::CodePoint peek(usize offset = 0) const;
    unicode::CodePoint consume();
    void reconsume();

    // Token production
    Token consume_token();
    Token consume_ident_like_token();
    Token consume_string_token(unicode::CodePoint ending);
    Token consume_url_token();
    Token consume_numeric_token();
    Token consume_hash_token();

    // Helpers
    [[nodiscard]] bool is_valid_escape(usize offset = 0) const;
    [[nodiscard]] bool would_start_identifier(usize offset = 0) const;
    [[nodiscard]] bool would_start_number(usize offset = 0) const;

    unicode::CodePoint consume_escaped_code_point();
    String consume_name();
    std::pair<f64, bool> consume_number();
    void consume_whitespace();
    void consume_remnants_of_bad_url();

    // Input
    String m_input;
    usize m_position{0};

    // Peeked token cache
    std::optional<Token> m_peeked;
};

} // namespace lithium::css
