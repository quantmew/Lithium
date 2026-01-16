#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <variant>
#include <vector>
#include <optional>

namespace lithium::js {

// ============================================================================
// Token Types
// ============================================================================

enum class TokenType {
    // Literals
    Null,
    True,
    False,
    Number,
    String,
    TemplateHead,
    TemplateMiddle,
    TemplateTail,
    NoSubstitutionTemplate,
    RegExp,

    // Identifiers and keywords
    Identifier,

    // Keywords
    Await,
    Break,
    Case,
    Catch,
    Class,
    Const,
    Continue,
    Debugger,
    Default,
    Delete,
    Do,
    Else,
    Export,
    Extends,
    Finally,
    For,
    Function,
    If,
    Import,
    In,
    Instanceof,
    Let,
    New,
    Return,
    Super,
    Switch,
    This,
    Throw,
    Try,
    Typeof,
    Var,
    Void,
    While,
    With,
    Yield,

    // Future reserved words
    Enum,

    // Punctuators
    OpenBrace,      // {
    CloseBrace,     // }
    OpenParen,      // (
    CloseParen,     // )
    OpenBracket,    // [
    CloseBracket,   // ]
    Dot,            // .
    Ellipsis,       // ...
    Semicolon,      // ;
    Comma,          // ,
    LessThan,       // <
    GreaterThan,    // >
    LessEqual,      // <=
    GreaterEqual,   // >=
    Equal,          // ==
    NotEqual,       // !=
    StrictEqual,    // ===
    StrictNotEqual, // !==
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    StarStar,       // **
    PlusPlus,       // ++
    MinusMinus,     // --
    LeftShift,      // <<
    RightShift,     // >>
    UnsignedRightShift, // >>>
    Ampersand,      // &
    Pipe,           // |
    Caret,          // ^
    Exclamation,    // !
    Tilde,          // ~
    AmpersandAmpersand, // &&
    PipePipe,       // ||
    QuestionQuestion, // ??
    Question,       // ?
    Colon,          // :
    Assign,         // =
    PlusAssign,     // +=
    MinusAssign,    // -=
    StarAssign,     // *=
    SlashAssign,    // /=
    PercentAssign,  // %=
    StarStarAssign, // **=
    LeftShiftAssign, // <<=
    RightShiftAssign, // >>=
    UnsignedRightShiftAssign, // >>>=
    AmpersandAssign, // &=
    PipeAssign,     // |=
    CaretAssign,    // ^=
    AmpersandAmpersandAssign, // &&=
    PipePipeAssign, // ||=
    QuestionQuestionAssign, // ??=
    Arrow,          // =>
    OptionalChain,  // ?.

    // Special
    EndOfFile,
    Invalid
};

// ============================================================================
// Token
// ============================================================================

struct Token {
    TokenType type{TokenType::Invalid};
    String value;

    // For numbers
    f64 number_value{0};
    bool is_integer{false};

    // Source location
    usize line{1};
    usize column{1};
    usize start{0};
    usize end{0};

    // Flags
    bool preceded_by_line_terminator{false};

    [[nodiscard]] bool is_keyword() const;
    [[nodiscard]] bool is_literal() const;
    [[nodiscard]] bool is_assignment_operator() const;
    [[nodiscard]] bool is_binary_operator() const;
    [[nodiscard]] bool is_unary_operator() const;
};

// ============================================================================
// Lexer
// ============================================================================

class Lexer {
public:
    using ErrorCallback = std::function<void(const String& message, usize line, usize column)>;

    Lexer();

    void set_input(const String& source);
    void set_input(std::string_view source);

    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }

    // Get next token
    [[nodiscard]] Token next_token();

    // Peek at next token
    [[nodiscard]] Token peek_token();

    // Check if at end
    [[nodiscard]] bool at_end() const { return m_position >= m_source.size(); }

    // Current position info
    [[nodiscard]] usize line() const { return m_line; }
    [[nodiscard]] usize column() const { return m_column; }

    // For template literal parsing
    void set_template_mode(bool enabled) { m_template_mode = enabled; }

private:
    // Character consumption
    [[nodiscard]] unicode::CodePoint peek(usize offset = 0) const;
    unicode::CodePoint consume();
    void reconsume();
    bool consume_if(unicode::CodePoint expected);
    bool consume_if(std::string_view str);

    // Token scanning
    Token scan_token();
    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_string(unicode::CodePoint quote);
    Token scan_template();
    Token scan_regexp();
    Token scan_punctuator();

    // Helpers
    void skip_whitespace_and_comments();
    void skip_line_comment();
    void skip_block_comment();

    [[nodiscard]] bool is_identifier_start(unicode::CodePoint cp) const;
    [[nodiscard]] bool is_identifier_part(unicode::CodePoint cp) const;

    unicode::CodePoint scan_unicode_escape();
    unicode::CodePoint scan_hex_escape(usize digits);

    void error(const String& message);

    // Make token helper
    Token make_token(TokenType type);
    Token make_token(TokenType type, const String& value);

    // Source
    String m_source;
    usize m_position{0};
    usize m_line{1};
    usize m_column{1};
    usize m_token_start{0};
    usize m_token_start_line{1};
    usize m_token_start_column{1};

    // State
    bool m_template_mode{false};
    bool m_line_terminator_before{false};

    // Peeked token
    std::optional<Token> m_peeked;

    // Error callback
    ErrorCallback m_error_callback;
};

} // namespace lithium::js
