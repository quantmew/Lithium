#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <variant>
#include <vector>
#include <optional>
#include <functional>

namespace lithium::html {

// ============================================================================
// HTML Token Types
// ============================================================================

struct DoctypeToken {
    String name;
    std::optional<String> public_identifier;
    std::optional<String> system_identifier;
    bool force_quirks{false};
};

struct TagToken {
    String name;
    std::vector<std::pair<String, String>> attributes;
    bool self_closing{false};
    bool is_end_tag{false};

    [[nodiscard]] std::optional<String> get_attribute(const String& name) const;
    void set_attribute(const String& name, const String& value);
};

struct CommentToken {
    String data;
};

struct CharacterToken {
    unicode::CodePoint code_point;
};

struct EndOfFileToken {};

using Token = std::variant<
    DoctypeToken,
    TagToken,
    CommentToken,
    CharacterToken,
    EndOfFileToken
>;

// Token type checking helpers
[[nodiscard]] bool is_doctype(const Token& token);
[[nodiscard]] bool is_start_tag(const Token& token);
[[nodiscard]] bool is_end_tag(const Token& token);
[[nodiscard]] bool is_character(const Token& token);
[[nodiscard]] bool is_comment(const Token& token);
[[nodiscard]] bool is_eof(const Token& token);

[[nodiscard]] bool is_start_tag_named(const Token& token, const String& name);
[[nodiscard]] bool is_end_tag_named(const Token& token, const String& name);

// ============================================================================
// Tokenizer States (WHATWG HTML5 spec)
// ============================================================================

enum class TokenizerState {
    Data,
    RCDATA,
    RAWTEXT,
    ScriptData,
    PLAINTEXT,
    TagOpen,
    EndTagOpen,
    TagName,
    RCDATALessThanSign,
    RCDATAEndTagOpen,
    RCDATAEndTagName,
    RAWTEXTLessThanSign,
    RAWTEXTEndTagOpen,
    RAWTEXTEndTagName,
    ScriptDataLessThanSign,
    ScriptDataEndTagOpen,
    ScriptDataEndTagName,
    ScriptDataEscapeStart,
    ScriptDataEscapeStartDash,
    ScriptDataEscaped,
    ScriptDataEscapedDash,
    ScriptDataEscapedDashDash,
    ScriptDataEscapedLessThanSign,
    ScriptDataEscapedEndTagOpen,
    ScriptDataEscapedEndTagName,
    ScriptDataDoubleEscapeStart,
    ScriptDataDoubleEscaped,
    ScriptDataDoubleEscapedDash,
    ScriptDataDoubleEscapedDashDash,
    ScriptDataDoubleEscapedLessThanSign,
    ScriptDataDoubleEscapeEnd,
    BeforeAttributeName,
    AttributeName,
    AfterAttributeName,
    BeforeAttributeValue,
    AttributeValueDoubleQuoted,
    AttributeValueSingleQuoted,
    AttributeValueUnquoted,
    AfterAttributeValueQuoted,
    SelfClosingStartTag,
    BogusComment,
    MarkupDeclarationOpen,
    CommentStart,
    CommentStartDash,
    Comment,
    CommentLessThanSign,
    CommentLessThanSignBang,
    CommentLessThanSignBangDash,
    CommentLessThanSignBangDashDash,
    CommentEndDash,
    CommentEnd,
    CommentEndBang,
    DOCTYPE,
    BeforeDOCTYPEName,
    DOCTYPEName,
    AfterDOCTYPEName,
    AfterDOCTYPEPublicKeyword,
    BeforeDOCTYPEPublicIdentifier,
    DOCTYPEPublicIdentifierDoubleQuoted,
    DOCTYPEPublicIdentifierSingleQuoted,
    AfterDOCTYPEPublicIdentifier,
    BetweenDOCTYPEPublicAndSystemIdentifiers,
    AfterDOCTYPESystemKeyword,
    BeforeDOCTYPESystemIdentifier,
    DOCTYPESystemIdentifierDoubleQuoted,
    DOCTYPESystemIdentifierSingleQuoted,
    AfterDOCTYPESystemIdentifier,
    BogusDOCTYPE,
    CDATASection,
    CDATASectionBracket,
    CDATASectionEnd,
    CharacterReference,
    NamedCharacterReference,
    AmbiguousAmpersand,
    NumericCharacterReference,
    HexadecimalCharacterReferenceStart,
    DecimalCharacterReferenceStart,
    HexadecimalCharacterReference,
    DecimalCharacterReference,
    NumericCharacterReferenceEnd,
};

// ============================================================================
// Tokenizer - HTML5 compliant tokenizer
// ============================================================================

class Tokenizer {
public:
    using TokenCallback = std::function<void(Token)>;
    using ErrorCallback = std::function<void(const String& message)>;

    Tokenizer();

    // Set input
    void set_input(const String& input);
    void set_input(std::string_view input);

    // Set callbacks
    void set_token_callback(TokenCallback callback) { m_token_callback = std::move(callback); }
    void set_error_callback(ErrorCallback callback) { m_error_callback = std::move(callback); }
    void set_in_foreign_content(bool in_foreign) { m_in_foreign_content = in_foreign; }

    // Streaming support
    void enable_streaming(bool streaming) { m_streaming = streaming; }
    void append_input(const String& more);
    void insert_input_at_current_position(const String& more);
    void mark_end_of_stream();
    void clear_token_queue();
    void reset_after_script_execution();
    [[nodiscard]] bool streaming() const { return m_streaming; }

    // Run tokenizer
    void run();

    // Get next token (alternative to callback)
    [[nodiscard]] std::optional<Token> next_token();

    // State manipulation (for tree builder integration)
    void set_state(TokenizerState state) { m_state = state; }
    [[nodiscard]] TokenizerState state() const { return m_state; }

    // Set last start tag (for end tag matching)
    void set_last_start_tag(const String& name) { m_last_start_tag_name = name; }

private:
    // Character consumption
    [[nodiscard]] std::optional<unicode::CodePoint> peek() const;
    [[nodiscard]] std::optional<unicode::CodePoint> peek_next() const;
    unicode::CodePoint consume();
    void reconsume();
    bool consume_if(unicode::CodePoint expected);
    bool consume_if_match(std::string_view str, bool case_insensitive = false);

    // Token emission
    void emit(Token token);
    void emit_character(unicode::CodePoint cp);
    void emit_current_token();
    void emit_eof();

    // Error reporting
    void parse_error(const String& message);

    // State machine
    void process_state();

    // State handlers
    void handle_data_state();
    void handle_rcdata_state();
    void handle_rawtext_state();
    void handle_script_data_state();
    void handle_plaintext_state();
    void handle_tag_open_state();
    void handle_end_tag_open_state();
    void handle_tag_name_state();
    void handle_before_attribute_name_state();
    void handle_attribute_name_state();
    void handle_after_attribute_name_state();
    void handle_before_attribute_value_state();
    void handle_attribute_value_double_quoted_state();
    void handle_attribute_value_single_quoted_state();
    void handle_attribute_value_unquoted_state();
    void handle_after_attribute_value_quoted_state();
    void handle_self_closing_start_tag_state();
    void handle_bogus_comment_state();
    void handle_markup_declaration_open_state();
    void handle_comment_start_state();
    void handle_comment_state();
    void handle_comment_end_dash_state();
    void handle_comment_end_state();
    void handle_doctype_state();
    void handle_before_doctype_name_state();
    void handle_doctype_name_state();
    void handle_after_doctype_name_state();
    void handle_character_reference_state();

    // Additional state handlers (HTML5 spec 13.2.5)
    void handle_rcdata_less_than_sign_state();
    void handle_rcdata_end_tag_open_state();
    void handle_rcdata_end_tag_name_state();
    void handle_rawtext_less_than_sign_state();
    void handle_rawtext_end_tag_open_state();
    void handle_rawtext_end_tag_name_state();
    void handle_script_data_less_than_sign_state();
    void handle_script_data_end_tag_open_state();
    void handle_script_data_end_tag_name_state();
    void handle_script_data_escape_start_state();
    void handle_script_data_escape_start_dash_state();
    void handle_script_data_escaped_state();
    void handle_script_data_escaped_dash_state();
    void handle_script_data_escaped_dash_dash_state();
    void handle_script_data_escaped_less_than_sign_state();
    void handle_script_data_escaped_end_tag_open_state();
    void handle_script_data_escaped_end_tag_name_state();
    void handle_script_data_double_escape_start_state();
    void handle_script_data_double_escaped_state();
    void handle_script_data_double_escaped_dash_state();
    void handle_script_data_double_escaped_dash_dash_state();
    void handle_script_data_double_escaped_less_than_sign_state();
    void handle_script_data_double_escape_end_state();
    void handle_comment_start_dash_state();
    void handle_comment_less_than_sign_state();
    void handle_comment_less_than_sign_bang_state();
    void handle_comment_less_than_sign_bang_dash_state();
    void handle_comment_less_than_sign_bang_dash_dash_state();
    void handle_comment_end_bang_state();
    void handle_after_doctype_public_keyword_state();
    void handle_before_doctype_public_identifier_state();
    void handle_doctype_public_identifier_double_quoted_state();
    void handle_doctype_public_identifier_single_quoted_state();
    void handle_after_doctype_public_identifier_state();
    void handle_between_doctype_public_and_system_identifiers_state();
    void handle_after_doctype_system_keyword_state();
    void handle_before_doctype_system_identifier_state();
    void handle_doctype_system_identifier_double_quoted_state();
    void handle_doctype_system_identifier_single_quoted_state();
    void handle_after_doctype_system_identifier_state();
    void handle_bogus_doctype_state();
    void handle_cdata_section_state();
    void handle_cdata_section_bracket_state();
    void handle_cdata_section_end_state();
    void handle_named_character_reference_state();
    void handle_ambiguous_ampersand_state();
    void handle_numeric_character_reference_state();
    void handle_hexadecimal_character_reference_start_state();
    void handle_decimal_character_reference_start_state();
    void handle_hexadecimal_character_reference_state();
    void handle_decimal_character_reference_state();
    void handle_numeric_character_reference_end_state();

    // Character reference handling
    unicode::CodePoint consume_character_reference();
    unicode::CodePoint consume_named_character_reference();
    unicode::CodePoint consume_numeric_character_reference();

    // Helper methods
    [[nodiscard]] bool is_appropriate_end_tag_token() const;
    void start_new_attribute();
    void finish_attribute_name();
    void finish_attribute_value();

    // Input
    String m_input;
    usize m_position{0};
    usize m_mark{0};

    // Current state
    TokenizerState m_state{TokenizerState::Data};
    TokenizerState m_return_state{TokenizerState::Data};

    // Current token being built
    std::optional<Token> m_current_token;

    // Buffers
    StringBuilder m_temp_buffer;
    String m_current_attribute_name;
    String m_current_attribute_value;
    String m_last_start_tag_name;
    u32 m_character_reference_code{0};

    // Callbacks
    TokenCallback m_token_callback;
    ErrorCallback m_error_callback;

    // Token queue for next_token() interface
    std::vector<Token> m_token_queue;

    // Streaming
    bool m_streaming{false};
    bool m_end_of_stream{true};
    bool m_eof_emitted{false};
    bool m_in_foreign_content{false};
};

} // namespace lithium::html
