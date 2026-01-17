/**
 * HTML5 Tokenizer implementation
 * Following WHATWG HTML Living Standard
 */

#include "lithium/html/tokenizer.hpp"
#include <cctype>
#include <algorithm>

namespace lithium::html {

// ============================================================================
// Token helpers
// ============================================================================

std::optional<String> TagToken::get_attribute(const String& name) const {
    auto lower_name = name.to_lowercase();
    for (const auto& [attr_name, attr_value] : attributes) {
        if (attr_name.to_lowercase() == lower_name) {
            return attr_value;
        }
    }
    return std::nullopt;
}

void TagToken::set_attribute(const String& name, const String& value) {
    for (auto& [attr_name, attr_value] : attributes) {
        if (attr_name == name) {
            attr_value = value;
            return;
        }
    }
    attributes.emplace_back(name, value);
}

bool is_doctype(const Token& token) {
    return std::holds_alternative<DoctypeToken>(token);
}

bool is_start_tag(const Token& token) {
    if (auto* tag = std::get_if<TagToken>(&token)) {
        return !tag->is_end_tag;
    }
    return false;
}

bool is_end_tag(const Token& token) {
    if (auto* tag = std::get_if<TagToken>(&token)) {
        return tag->is_end_tag;
    }
    return false;
}

bool is_character(const Token& token) {
    return std::holds_alternative<CharacterToken>(token);
}

bool is_comment(const Token& token) {
    return std::holds_alternative<CommentToken>(token);
}

bool is_eof(const Token& token) {
    return std::holds_alternative<EndOfFileToken>(token);
}

bool is_start_tag_named(const Token& token, const String& name) {
    if (auto* tag = std::get_if<TagToken>(&token)) {
        return !tag->is_end_tag && tag->name.to_lowercase() == name.to_lowercase();
    }
    return false;
}

bool is_end_tag_named(const Token& token, const String& name) {
    if (auto* tag = std::get_if<TagToken>(&token)) {
        return tag->is_end_tag && tag->name.to_lowercase() == name.to_lowercase();
    }
    return false;
}

// ============================================================================
// Tokenizer implementation
// ============================================================================

Tokenizer::Tokenizer() = default;

void Tokenizer::set_input(const String& input) {
    m_input = input;
    m_position = 0;
}

void Tokenizer::set_input(std::string_view input) {
    m_input = String(input);
    m_position = 0;
}

void Tokenizer::run() {
    while (m_position <= m_input.length()) {
        process_state();
    }
}

std::optional<Token> Tokenizer::next_token() {
    while (m_token_queue.empty() && m_position <= m_input.length()) {
        process_state();
    }

    if (!m_token_queue.empty()) {
        Token token = std::move(m_token_queue.front());
        m_token_queue.erase(m_token_queue.begin());
        return token;
    }

    return std::nullopt;
}

std::optional<unicode::CodePoint> Tokenizer::peek() const {
    if (m_position >= m_input.length()) {
        return std::nullopt;
    }
    return m_input[m_position];
}

std::optional<unicode::CodePoint> Tokenizer::peek_next() const {
    if (m_position + 1 >= m_input.length()) {
        return std::nullopt;
    }
    return m_input[m_position + 1];
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

bool Tokenizer::consume_if(unicode::CodePoint expected) {
    if (peek() == expected) {
        consume();
        return true;
    }
    return false;
}

bool Tokenizer::consume_if_match(std::string_view str, bool case_insensitive) {
    if (m_position + str.length() > m_input.length()) {
        return false;
    }

    for (usize i = 0; i < str.length(); ++i) {
        char input_char = static_cast<char>(m_input[m_position + i]);
        char expected_char = str[i];

        if (case_insensitive) {
            if (std::tolower(input_char) != std::tolower(expected_char)) {
                return false;
            }
        } else {
            if (input_char != expected_char) {
                return false;
            }
        }
    }

    m_position += str.length();
    return true;
}

void Tokenizer::emit(Token token) {
    if (m_token_callback) {
        m_token_callback(std::move(token));
    } else {
        m_token_queue.push_back(std::move(token));
    }
}

void Tokenizer::emit_character(unicode::CodePoint cp) {
    emit(CharacterToken{cp});
}

void Tokenizer::emit_current_token() {
    if (m_current_token) {
        if (auto* tag = std::get_if<TagToken>(&*m_current_token)) {
            if (!tag->is_end_tag) {
                m_last_start_tag_name = tag->name;
            }
        }
        emit(std::move(*m_current_token));
        m_current_token.reset();
    }
}

void Tokenizer::emit_eof() {
    emit(EndOfFileToken{});
    // Advance past end so the main loops terminate after emitting EOF
    m_position = m_input.length() + 1;
}

void Tokenizer::parse_error(const String& message) {
    if (m_error_callback) {
        m_error_callback(message);
    }
}

void Tokenizer::process_state() {
    switch (m_state) {
        case TokenizerState::Data:
            handle_data_state();
            break;
        case TokenizerState::RCDATA:
            handle_rcdata_state();
            break;
        case TokenizerState::RAWTEXT:
            handle_rawtext_state();
            break;
        case TokenizerState::ScriptData:
            handle_script_data_state();
            break;
        case TokenizerState::PLAINTEXT:
            handle_plaintext_state();
            break;
        case TokenizerState::TagOpen:
            handle_tag_open_state();
            break;
        case TokenizerState::EndTagOpen:
            handle_end_tag_open_state();
            break;
        case TokenizerState::TagName:
            handle_tag_name_state();
            break;
        case TokenizerState::BeforeAttributeName:
            handle_before_attribute_name_state();
            break;
        case TokenizerState::AttributeName:
            handle_attribute_name_state();
            break;
        case TokenizerState::AfterAttributeName:
            handle_after_attribute_name_state();
            break;
        case TokenizerState::BeforeAttributeValue:
            handle_before_attribute_value_state();
            break;
        case TokenizerState::AttributeValueDoubleQuoted:
            handle_attribute_value_double_quoted_state();
            break;
        case TokenizerState::AttributeValueSingleQuoted:
            handle_attribute_value_single_quoted_state();
            break;
        case TokenizerState::AttributeValueUnquoted:
            handle_attribute_value_unquoted_state();
            break;
        case TokenizerState::AfterAttributeValueQuoted:
            handle_after_attribute_value_quoted_state();
            break;
        case TokenizerState::SelfClosingStartTag:
            handle_self_closing_start_tag_state();
            break;
        case TokenizerState::BogusComment:
            handle_bogus_comment_state();
            break;
        case TokenizerState::MarkupDeclarationOpen:
            handle_markup_declaration_open_state();
            break;
        case TokenizerState::CommentStart:
            handle_comment_start_state();
            break;
        case TokenizerState::Comment:
            handle_comment_state();
            break;
        case TokenizerState::CommentEndDash:
            handle_comment_end_dash_state();
            break;
        case TokenizerState::CommentEnd:
            handle_comment_end_state();
            break;
        case TokenizerState::DOCTYPE:
            handle_doctype_state();
            break;
        case TokenizerState::BeforeDOCTYPEName:
            handle_before_doctype_name_state();
            break;
        case TokenizerState::DOCTYPEName:
            handle_doctype_name_state();
            break;
        case TokenizerState::AfterDOCTYPEName:
            handle_after_doctype_name_state();
            break;
        case TokenizerState::CharacterReference:
            handle_character_reference_state();
            break;
        case TokenizerState::RCDATALessThanSign:
            handle_rcdata_less_than_sign_state();
            break;
        case TokenizerState::RCDATAEndTagOpen:
            handle_rcdata_end_tag_open_state();
            break;
        case TokenizerState::RCDATAEndTagName:
            handle_rcdata_end_tag_name_state();
            break;
        case TokenizerState::RAWTEXTLessThanSign:
            handle_rawtext_less_than_sign_state();
            break;
        case TokenizerState::RAWTEXTEndTagOpen:
            handle_rawtext_end_tag_open_state();
            break;
        case TokenizerState::RAWTEXTEndTagName:
            handle_rawtext_end_tag_name_state();
            break;
        case TokenizerState::ScriptDataLessThanSign:
            handle_script_data_less_than_sign_state();
            break;
        case TokenizerState::ScriptDataEndTagOpen:
            handle_script_data_end_tag_open_state();
            break;
        case TokenizerState::ScriptDataEndTagName:
            handle_script_data_end_tag_name_state();
            break;
        case TokenizerState::ScriptDataEscapeStart:
            handle_script_data_escape_start_state();
            break;
        case TokenizerState::ScriptDataEscapeStartDash:
            handle_script_data_escape_start_dash_state();
            break;
        case TokenizerState::ScriptDataEscaped:
            handle_script_data_escaped_state();
            break;
        case TokenizerState::ScriptDataEscapedDash:
            handle_script_data_escaped_dash_state();
            break;
        case TokenizerState::ScriptDataEscapedDashDash:
            handle_script_data_escaped_dash_dash_state();
            break;
        case TokenizerState::ScriptDataEscapedLessThanSign:
            handle_script_data_escaped_less_than_sign_state();
            break;
        case TokenizerState::ScriptDataEscapedEndTagOpen:
            handle_script_data_escaped_end_tag_open_state();
            break;
        case TokenizerState::ScriptDataEscapedEndTagName:
            handle_script_data_escaped_end_tag_name_state();
            break;
        case TokenizerState::ScriptDataDoubleEscapeStart:
            handle_script_data_double_escape_start_state();
            break;
        case TokenizerState::ScriptDataDoubleEscaped:
            handle_script_data_double_escaped_state();
            break;
        case TokenizerState::ScriptDataDoubleEscapedDash:
            handle_script_data_double_escaped_dash_state();
            break;
        case TokenizerState::ScriptDataDoubleEscapedDashDash:
            handle_script_data_double_escaped_dash_dash_state();
            break;
        case TokenizerState::ScriptDataDoubleEscapedLessThanSign:
            handle_script_data_double_escaped_less_than_sign_state();
            break;
        case TokenizerState::ScriptDataDoubleEscapeEnd:
            handle_script_data_double_escape_end_state();
            break;
        case TokenizerState::CommentStartDash:
            handle_comment_start_dash_state();
            break;
        case TokenizerState::CommentLessThanSign:
            handle_comment_less_than_sign_state();
            break;
        case TokenizerState::CommentLessThanSignBang:
            handle_comment_less_than_sign_bang_state();
            break;
        case TokenizerState::CommentLessThanSignBangDash:
            handle_comment_less_than_sign_bang_dash_state();
            break;
        case TokenizerState::CommentLessThanSignBangDashDash:
            handle_comment_less_than_sign_bang_dash_dash_state();
            break;
        case TokenizerState::CommentEndBang:
            handle_comment_end_bang_state();
            break;
        case TokenizerState::AfterDOCTYPEPublicKeyword:
            handle_after_doctype_public_keyword_state();
            break;
        case TokenizerState::BeforeDOCTYPEPublicIdentifier:
            handle_before_doctype_public_identifier_state();
            break;
        case TokenizerState::DOCTYPEPublicIdentifierDoubleQuoted:
            handle_doctype_public_identifier_double_quoted_state();
            break;
        case TokenizerState::DOCTYPEPublicIdentifierSingleQuoted:
            handle_doctype_public_identifier_single_quoted_state();
            break;
        case TokenizerState::AfterDOCTYPEPublicIdentifier:
            handle_after_doctype_public_identifier_state();
            break;
        case TokenizerState::BetweenDOCTYPEPublicAndSystemIdentifiers:
            handle_between_doctype_public_and_system_identifiers_state();
            break;
        case TokenizerState::AfterDOCTYPESystemKeyword:
            handle_after_doctype_system_keyword_state();
            break;
        case TokenizerState::BeforeDOCTYPESystemIdentifier:
            handle_before_doctype_system_identifier_state();
            break;
        case TokenizerState::DOCTYPESystemIdentifierDoubleQuoted:
            handle_doctype_system_identifier_double_quoted_state();
            break;
        case TokenizerState::DOCTYPESystemIdentifierSingleQuoted:
            handle_doctype_system_identifier_single_quoted_state();
            break;
        case TokenizerState::AfterDOCTYPESystemIdentifier:
            handle_after_doctype_system_identifier_state();
            break;
        case TokenizerState::BogusDOCTYPE:
            handle_bogus_doctype_state();
            break;
        case TokenizerState::NamedCharacterReference:
            handle_named_character_reference_state();
            break;
        case TokenizerState::AmbiguousAmpersand:
            handle_ambiguous_ampersand_state();
            break;
        case TokenizerState::NumericCharacterReference:
            handle_numeric_character_reference_state();
            break;
        case TokenizerState::HexadecimalCharacterReferenceStart:
            handle_hexadecimal_character_reference_start_state();
            break;
        case TokenizerState::DecimalCharacterReferenceStart:
            handle_decimal_character_reference_start_state();
            break;
        case TokenizerState::HexadecimalCharacterReference:
            handle_hexadecimal_character_reference_state();
            break;
        case TokenizerState::DecimalCharacterReference:
            handle_decimal_character_reference_state();
            break;
        case TokenizerState::NumericCharacterReferenceEnd:
            handle_numeric_character_reference_end_state();
            break;
        default:
            // Handle remaining states with data state as fallback
            handle_data_state();
            break;
    }
}

// ============================================================================
// State handlers
// ============================================================================

void Tokenizer::handle_data_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '&') {
        m_return_state = TokenizerState::Data;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == '<') {
        m_state = TokenizerState::TagOpen;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(*cp);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_rcdata_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '&') {
        m_return_state = TokenizerState::RCDATA;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == '<') {
        m_state = TokenizerState::RCDATALessThanSign;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_rawtext_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '<') {
        m_state = TokenizerState::RAWTEXTLessThanSign;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_script_data_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == '<') {
        m_state = TokenizerState::ScriptDataLessThanSign;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_plaintext_state() {
    auto cp = peek();
    if (!cp) {
        emit_eof();
        return;
    }

    consume();

    if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        emit_character(0xFFFD);
    } else {
        emit_character(*cp);
    }
}

void Tokenizer::handle_tag_open_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-before-tag-name"_s);
        emit_character('<');
        emit_eof();
        return;
    }

    consume();

    if (*cp == '!') {
        m_state = TokenizerState::MarkupDeclarationOpen;
    } else if (*cp == '/') {
        m_state = TokenizerState::EndTagOpen;
    } else if (std::isalpha(*cp)) {
        m_current_token = TagToken{};
        std::get<TagToken>(*m_current_token).name.clear();
        reconsume();
        m_state = TokenizerState::TagName;
    } else if (*cp == '?') {
        parse_error("unexpected-question-mark-instead-of-tag-name"_s);
        m_current_token = CommentToken{};
        reconsume();
        m_state = TokenizerState::BogusComment;
    } else {
        parse_error("invalid-first-character-of-tag-name"_s);
        emit_character('<');
        reconsume();
        m_state = TokenizerState::Data;
    }
}

void Tokenizer::handle_end_tag_open_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-before-tag-name"_s);
        emit_character('<');
        emit_character('/');
        emit_eof();
        return;
    }

    consume();

    if (std::isalpha(*cp)) {
        m_current_token = TagToken{};
        std::get<TagToken>(*m_current_token).is_end_tag = true;
        reconsume();
        m_state = TokenizerState::TagName;
    } else if (*cp == '>') {
        parse_error("missing-end-tag-name"_s);
        m_state = TokenizerState::Data;
    } else {
        parse_error("invalid-first-character-of-tag-name"_s);
        m_current_token = CommentToken{};
        reconsume();
        m_state = TokenizerState::BogusComment;
    }
}

void Tokenizer::handle_tag_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    auto& tag = std::get<TagToken>(*m_current_token);

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeAttributeName;
    } else if (*cp == '/') {
        m_state = TokenizerState::SelfClosingStartTag;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (std::isupper(*cp)) {
        tag.name = tag.name + String(1, static_cast<char>(std::tolower(*cp)));
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        tag.name = tag.name + String(1, static_cast<char>(0xFFFD));
    } else {
        tag.name = tag.name + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_before_attribute_name_state() {
    auto cp = peek();
    if (!cp) {
        reconsume();
        m_state = TokenizerState::AfterAttributeName;
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '/' || *cp == '>') {
        reconsume();
        m_state = TokenizerState::AfterAttributeName;
    } else if (*cp == '=') {
        parse_error("unexpected-equals-sign-before-attribute-name"_s);
        start_new_attribute();
        m_current_attribute_name = String(1, static_cast<char>(*cp));
        m_state = TokenizerState::AttributeName;
    } else {
        start_new_attribute();
        reconsume();
        m_state = TokenizerState::AttributeName;
    }
}

void Tokenizer::handle_attribute_name_state() {
    auto cp = peek();
    if (!cp) {
        reconsume();
        m_state = TokenizerState::AfterAttributeName;
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ' ||
        *cp == '/' || *cp == '>') {
        reconsume();
        m_state = TokenizerState::AfterAttributeName;
        finish_attribute_name();
    } else if (*cp == '=') {
        finish_attribute_name();
        m_state = TokenizerState::BeforeAttributeValue;
    } else if (std::isupper(*cp)) {
        m_current_attribute_name = m_current_attribute_name + String(1, static_cast<char>(std::tolower(*cp)));
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        m_current_attribute_name = m_current_attribute_name + String(1, static_cast<char>(0xFFFD));
    } else if (*cp == '"' || *cp == '\'' || *cp == '<') {
        parse_error("unexpected-character-in-attribute-name"_s);
        m_current_attribute_name = m_current_attribute_name + String(1, static_cast<char>(*cp));
    } else {
        m_current_attribute_name = m_current_attribute_name + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_after_attribute_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    auto flush_attribute_if_needed = [&]() {
        if (!m_current_attribute_name.empty()) {
            finish_attribute_value();
        }
    };

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '/') {
        flush_attribute_if_needed();
        m_state = TokenizerState::SelfClosingStartTag;
    } else if (*cp == '=') {
        m_state = TokenizerState::BeforeAttributeValue;
    } else if (*cp == '>') {
        flush_attribute_if_needed();
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        flush_attribute_if_needed();
        start_new_attribute();
        reconsume();
        m_state = TokenizerState::AttributeName;
    }
}

void Tokenizer::handle_before_attribute_value_state() {
    auto cp = peek();
    if (!cp) {
        reconsume();
        m_state = TokenizerState::AttributeValueUnquoted;
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '"') {
        m_state = TokenizerState::AttributeValueDoubleQuoted;
    } else if (*cp == '\'') {
        m_state = TokenizerState::AttributeValueSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-attribute-value"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        reconsume();
        m_state = TokenizerState::AttributeValueUnquoted;
    }
}

void Tokenizer::handle_attribute_value_double_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    if (*cp == '"') {
        finish_attribute_value();
        m_state = TokenizerState::AfterAttributeValueQuoted;
    } else if (*cp == '&') {
        m_return_state = TokenizerState::AttributeValueDoubleQuoted;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(0xFFFD));
    } else {
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_attribute_value_single_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\'') {
        finish_attribute_value();
        m_state = TokenizerState::AfterAttributeValueQuoted;
    } else if (*cp == '&') {
        m_return_state = TokenizerState::AttributeValueSingleQuoted;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(0xFFFD));
    } else {
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_attribute_value_unquoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        finish_attribute_value();
        m_state = TokenizerState::BeforeAttributeName;
    } else if (*cp == '&') {
        m_return_state = TokenizerState::AttributeValueUnquoted;
        m_state = TokenizerState::CharacterReference;
    } else if (*cp == '>') {
        finish_attribute_value();
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(0xFFFD));
    } else if (*cp == '"' || *cp == '\'' || *cp == '<' || *cp == '=' || *cp == '`') {
        parse_error("unexpected-character-in-unquoted-attribute-value"_s);
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(*cp));
    } else {
        m_current_attribute_value = m_current_attribute_value + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_after_attribute_value_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeAttributeName;
    } else if (*cp == '/') {
        m_state = TokenizerState::SelfClosingStartTag;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-whitespace-between-attributes"_s);
        reconsume();
        m_state = TokenizerState::BeforeAttributeName;
    }
}

void Tokenizer::handle_self_closing_start_tag_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-tag"_s);
        emit_eof();
        return;
    }

    consume();

    if (*cp == '>') {
        std::get<TagToken>(*m_current_token).self_closing = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("unexpected-solidus-in-tag"_s);
        reconsume();
        m_state = TokenizerState::BeforeAttributeName;
    }
}

void Tokenizer::handle_bogus_comment_state() {
    auto cp = peek();
    if (!cp) {
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& comment = std::get<CommentToken>(*m_current_token);

    if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        comment.data = comment.data + String(1, static_cast<char>(0xFFFD));
    } else {
        comment.data = comment.data + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_markup_declaration_open_state() {
    if (consume_if_match("--", false)) {
        m_current_token = CommentToken{};
        m_state = TokenizerState::CommentStart;
    } else if (consume_if_match("DOCTYPE", true)) {
        m_state = TokenizerState::DOCTYPE;
    } else if (consume_if_match("[CDATA[", false)) {
        // TODO: Check if in foreign content
        parse_error("cdata-in-html-content"_s);
        m_current_token = CommentToken{};
        std::get<CommentToken>(*m_current_token).data = "[CDATA["_s;
        m_state = TokenizerState::BogusComment;
    } else {
        parse_error("incorrectly-opened-comment"_s);
        m_current_token = CommentToken{};
        m_state = TokenizerState::BogusComment;
    }
}

void Tokenizer::handle_comment_start_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentStartDash;
    } else if (*cp == '>') {
        parse_error("abrupt-closing-of-empty-comment"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& comment = std::get<CommentToken>(*m_current_token);

    if (*cp == '<') {
        comment.data = comment.data + String(1, static_cast<char>(*cp));
        m_state = TokenizerState::CommentLessThanSign;
    } else if (*cp == '-') {
        m_state = TokenizerState::CommentEndDash;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        comment.data = comment.data + String(1, static_cast<char>(0xFFFD));
    } else {
        comment.data = comment.data + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_comment_end_dash_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentEnd;
    } else {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "-"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_comment_end_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& comment = std::get<CommentToken>(*m_current_token);

    if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == '!') {
        m_state = TokenizerState::CommentEndBang;
    } else if (*cp == '-') {
        comment.data = comment.data + "-"_s;
    } else {
        comment.data = comment.data + "--"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

void Tokenizer::handle_doctype_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeDOCTYPEName;
    } else if (*cp == '>') {
        reconsume();
        m_state = TokenizerState::BeforeDOCTYPEName;
    } else {
        parse_error("missing-whitespace-before-doctype-name"_s);
        reconsume();
        m_state = TokenizerState::BeforeDOCTYPEName;
    }
}

void Tokenizer::handle_before_doctype_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (std::isupper(*cp)) {
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).name = String(1, static_cast<char>(std::tolower(*cp)));
        m_state = TokenizerState::DOCTYPEName;
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).name = String(1, static_cast<char>(0xFFFD));
        m_state = TokenizerState::DOCTYPEName;
    } else if (*cp == '>') {
        parse_error("missing-doctype-name"_s);
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        m_current_token = DoctypeToken{};
        std::get<DoctypeToken>(*m_current_token).name = String(1, static_cast<char>(*cp));
        m_state = TokenizerState::DOCTYPEName;
    }
}

void Tokenizer::handle_doctype_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    auto& doctype = std::get<DoctypeToken>(*m_current_token);

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::AfterDOCTYPEName;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (std::isupper(*cp)) {
        doctype.name = doctype.name + String(1, static_cast<char>(std::tolower(*cp)));
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        doctype.name = doctype.name + String(1, static_cast<char>(0xFFFD));
    } else {
        doctype.name = doctype.name + String(1, static_cast<char>(*cp));
    }
}

void Tokenizer::handle_after_doctype_name_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        // For simplicity, treat as bogus DOCTYPE
        parse_error("invalid-character-sequence-after-doctype-name"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_character_reference_state() {
    m_temp_buffer.clear();
    m_temp_buffer.append('&');

    auto cp = peek();
    if (!cp) {
        // Flush as character tokens
        for (usize i = 0; i < m_temp_buffer.size(); ++i) {
            emit_character(m_temp_buffer.build()[i]);
        }
        m_state = m_return_state;
        return;
    }

    if (std::isalnum(*cp)) {
        reconsume();
        m_state = TokenizerState::NamedCharacterReference;
    } else if (*cp == '#') {
        consume();
        m_temp_buffer.append('#');
        m_state = TokenizerState::NumericCharacterReference;
    } else {
        // Flush as character tokens
        for (usize i = 0; i < m_temp_buffer.size(); ++i) {
            emit_character(m_temp_buffer.build()[i]);
        }
        m_state = m_return_state;
    }
}

bool Tokenizer::is_appropriate_end_tag_token() const {
    if (!m_current_token || !std::holds_alternative<TagToken>(*m_current_token)) {
        return false;
    }
    const auto& tag = std::get<TagToken>(*m_current_token);
    return tag.is_end_tag && tag.name == m_last_start_tag_name;
}

void Tokenizer::start_new_attribute() {
    m_current_attribute_name.clear();
    m_current_attribute_value.clear();
}

void Tokenizer::finish_attribute_name() {
    // Attribute name is already stored in m_current_attribute_name
}

void Tokenizer::finish_attribute_value() {
    if (m_current_token && std::holds_alternative<TagToken>(*m_current_token)) {
        auto& tag = std::get<TagToken>(*m_current_token);
        tag.set_attribute(m_current_attribute_name, m_current_attribute_value);
    }
    m_current_attribute_name.clear();
    m_current_attribute_value.clear();
}

unicode::CodePoint Tokenizer::consume_character_reference() {
    // Simplified character reference handling
    return '&';
}

unicode::CodePoint Tokenizer::consume_named_character_reference() {
    // TODO: Implement full named character reference table
    return '&';
}

unicode::CodePoint Tokenizer::consume_numeric_character_reference() {
    // TODO: Implement numeric character reference
    return '&';
}

// ============================================================================
// Additional state handlers following HTML5 spec
// ============================================================================

// RCDATA less-than sign state (13.2.5.9)
void Tokenizer::handle_rcdata_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('<');
        emit_eof();
        return;
    }

    if (*cp == '/') {
        consume();
        m_state = TokenizerState::RCDATAEndTagOpen;
        return;
    }

    emit_character('<');
    m_state = TokenizerState::RCDATA;
}

// RCDATA end tag open state (13.2.5.10)
void Tokenizer::handle_rcdata_end_tag_open_state() {
    if (!m_last_start_tag_name.empty() && consume_if_match(m_last_start_tag_name.c_str(), true)) {
        auto cp = peek();
        if (cp && *cp == '>') {
            consume();
            m_current_token = TagToken{};
            auto& tag = std::get<TagToken>(*m_current_token);
            tag.name = m_last_start_tag_name;
            tag.is_end_tag = true;
            emit_current_token();
            m_state = TokenizerState::Data;
            return;
        }
    }

    emit_character('<');
    emit_character('/');
    m_state = TokenizerState::RCDATA;
}

// RCDATA end tag name state (13.2.5.11)
void Tokenizer::handle_rcdata_end_tag_name_state() {
    emit_character('<');
    emit_character('/');
    // Emit buffer contents
    for (usize i = 0; i < m_temp_buffer.size(); ++i) {
        emit_character(m_temp_buffer.build()[i]);
    }
    m_state = TokenizerState::RCDATA;
}

// RAWTEXT less-than sign state (13.2.5.12)
void Tokenizer::handle_rawtext_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('<');
        emit_eof();
        return;
    }

    if (*cp == '/') {
        consume();
        m_state = TokenizerState::RAWTEXTEndTagOpen;
        return;
    }

    emit_character('<');
    m_state = TokenizerState::RAWTEXT;
}

// RAWTEXT end tag open state (13.2.5.13)
void Tokenizer::handle_rawtext_end_tag_open_state() {
    if (!m_last_start_tag_name.empty() && consume_if_match(m_last_start_tag_name.c_str(), true)) {
        auto cp = peek();
        if (cp && *cp == '>') {
            consume();
            m_current_token = TagToken{};
            auto& tag = std::get<TagToken>(*m_current_token);
            tag.name = m_last_start_tag_name;
            tag.is_end_tag = true;
            emit_current_token();
            m_state = TokenizerState::Data;
            return;
        }
    }

    emit_character('<');
    emit_character('/');
    m_state = TokenizerState::RAWTEXT;
}

// RAWTEXT end tag name state (13.2.5.14)
void Tokenizer::handle_rawtext_end_tag_name_state() {
    emit_character('<');
    emit_character('/');
    // Emit buffer contents
    for (usize i = 0; i < m_temp_buffer.size(); ++i) {
        emit_character(m_temp_buffer.build()[i]);
    }
    m_state = TokenizerState::RAWTEXT;
}

// Script data less-than sign state (13.2.5.15)
void Tokenizer::handle_script_data_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('<');
        emit_eof();
        return;
    }

    if (*cp == '/') {
        consume();
        m_state = TokenizerState::ScriptDataEndTagOpen;
        return;
    }

    emit_character('<');
    m_state = TokenizerState::ScriptData;
}

// Script data end tag open state (13.2.5.16)
void Tokenizer::handle_script_data_end_tag_open_state() {
    // Check for </script>
    if (consume_if_match("script", true)) {
        auto cp = peek();
        if (cp && *cp == '>') {
            consume();
            m_current_token = TagToken{};
            auto& tag = std::get<TagToken>(*m_current_token);
            tag.name = "script"_s;
            tag.is_end_tag = true;
            emit_current_token();
            m_state = TokenizerState::Data;
            return;
        }
        // Not a real end tag, fall through to emit text
    }

    emit_character('<');
    emit_character('/');
    m_state = TokenizerState::ScriptData;
}

// Script data end tag name state (13.2.5.17)
void Tokenizer::handle_script_data_end_tag_name_state() {
    emit_character('<');
    emit_character('/');
    // Emit buffer contents
    for (usize i = 0; i < m_temp_buffer.size(); ++i) {
        emit_character(m_temp_buffer.build()[i]);
    }
    m_state = TokenizerState::ScriptData;
}

// Script data escape start state (13.2.5.18)
void Tokenizer::handle_script_data_escape_start_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escape start dash state (13.2.5.19)
void Tokenizer::handle_script_data_escape_start_dash_state() {
    emit_character('-');
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escaped state (13.2.5.20)
void Tokenizer::handle_script_data_escaped_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escaped dash state (13.2.5.21)
void Tokenizer::handle_script_data_escaped_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escaped dash dash state (13.2.5.22)
void Tokenizer::handle_script_data_escaped_dash_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escaped less-than sign state (13.2.5.23)
void Tokenizer::handle_script_data_escaped_less_than_sign_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escaped end tag open state (13.2.5.24)
void Tokenizer::handle_script_data_escaped_end_tag_open_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data escaped end tag name state (13.2.5.25)
void Tokenizer::handle_script_data_escaped_end_tag_name_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data double escape start state (13.2.5.26)
void Tokenizer::handle_script_data_double_escape_start_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data double escaped state (13.2.5.27)
void Tokenizer::handle_script_data_double_escaped_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data double escaped dash state (13.2.5.28)
void Tokenizer::handle_script_data_double_escaped_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data double escaped dash dash state (13.2.5.29)
void Tokenizer::handle_script_data_double_escaped_dash_dash_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data double escaped less-than sign state (13.2.5.30)
void Tokenizer::handle_script_data_double_escaped_less_than_sign_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Script data double escape end state (13.2.5.31)
void Tokenizer::handle_script_data_double_escape_end_state() {
    reconsume();
    m_state = TokenizerState::ScriptData;
}

// Comment start dash state (13.2.5.44)
void Tokenizer::handle_comment_start_dash_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentEnd;
    } else if (*cp == '>') {
        parse_error("abrupt-closing-of-empty-comment"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "-"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

// Comment less-than sign state (13.2.5.46)
void Tokenizer::handle_comment_less_than_sign_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '!') {
        m_state = TokenizerState::CommentLessThanSignBang;
    } else {
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

// Comment less-than sign bang state (13.2.5.47)
void Tokenizer::handle_comment_less_than_sign_bang_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentLessThanSignBangDash;
    } else {
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

// Comment less-than sign bang dash state (13.2.5.48)
void Tokenizer::handle_comment_less_than_sign_bang_dash_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        m_state = TokenizerState::CommentLessThanSignBangDashDash;
    } else {
        reconsume();
        m_state = TokenizerState::CommentEndDash;
    }
}

// Comment less-than sign bang dash dash state (13.2.5.49)
void Tokenizer::handle_comment_less_than_sign_bang_dash_dash_state() {
    auto cp = peek();
    if (!cp || *cp == '>') {
        reconsume();
        m_state = TokenizerState::CommentEnd;
    } else {
        parse_error("nested-comment"_s);
        reconsume();
        m_state = TokenizerState::CommentEnd;
    }
}

// Comment end bang state (13.2.5.52)
void Tokenizer::handle_comment_end_bang_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-comment"_s);
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '-') {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "--!"_s;
        m_state = TokenizerState::CommentEndDash;
    } else if (*cp == '>') {
        parse_error("incorrectly-closed-comment"_s);
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        auto& comment = std::get<CommentToken>(*m_current_token);
        comment.data = comment.data + "--!"_s;
        reconsume();
        m_state = TokenizerState::Comment;
    }
}

// DOCTYPE public keyword states (13.2.5.57-61)
void Tokenizer::handle_after_doctype_public_keyword_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeDOCTYPEPublicIdentifier;
    } else if (*cp == '"') {
        parse_error("missing-whitespace-after-doctype-public-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        parse_error("missing-whitespace-after-doctype-public-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_before_doctype_public_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '"') {
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        std::get<DoctypeToken>(*m_current_token).public_identifier = ""_s;
        m_state = TokenizerState::DOCTYPEPublicIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_doctype_public_identifier_double_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '"') {
        m_state = TokenizerState::AfterDOCTYPEPublicIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_doctype_public_identifier_single_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\'') {
        m_state = TokenizerState::AfterDOCTYPEPublicIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-public-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.public_identifier) {
            *doctype.public_identifier = *doctype.public_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_after_doctype_public_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BetweenDOCTYPEPublicAndSystemIdentifiers;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == '"') {
        parse_error("missing-whitespace-between-doctype-public-and-system-identifiers"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        parse_error("missing-whitespace-between-doctype-public-and-system-identifiers"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_between_doctype_public_and_system_identifiers_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == '"') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

// DOCTYPE system keyword states (13.2.5.63-67)
void Tokenizer::handle_after_doctype_system_keyword_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        m_state = TokenizerState::BeforeDOCTYPESystemIdentifier;
    } else if (*cp == '"') {
        parse_error("missing-whitespace-after-doctype-system-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        parse_error("missing-whitespace-after-doctype-system-keyword"_s);
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_before_doctype_system_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '"') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierDoubleQuoted;
    } else if (*cp == '\'') {
        std::get<DoctypeToken>(*m_current_token).system_identifier = ""_s;
        m_state = TokenizerState::DOCTYPESystemIdentifierSingleQuoted;
    } else if (*cp == '>') {
        parse_error("missing-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("missing-quote-before-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_doctype_system_identifier_double_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '"') {
        m_state = TokenizerState::AfterDOCTYPESystemIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_doctype_system_identifier_single_quoted_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\'') {
        m_state = TokenizerState::AfterDOCTYPESystemIdentifier;
    } else if (*cp == '>') {
        parse_error("abrupt-doctype-system-identifier"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(0xFFFD));
        }
    } else {
        auto& doctype = std::get<DoctypeToken>(*m_current_token);
        if (doctype.system_identifier) {
            *doctype.system_identifier = *doctype.system_identifier + String(1, static_cast<char>(*cp));
        }
    }
}

void Tokenizer::handle_after_doctype_system_identifier_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("eof-in-doctype"_s);
        std::get<DoctypeToken>(*m_current_token).force_quirks = true;
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
        parse_error("unexpected-character-after-doctype-system-identifier"_s);
        m_state = TokenizerState::BogusDOCTYPE;
    }
}

void Tokenizer::handle_bogus_doctype_state() {
    auto cp = peek();
    if (!cp) {
        emit_current_token();
        emit_eof();
        return;
    }

    consume();

    if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else if (*cp == 0) {
        parse_error("unexpected-null-character"_s);
        // Ignore
    } else {
        // Ignore
    }
}

// Named character reference state (13.2.5.73)
void Tokenizer::handle_named_character_reference_state() {
    // Simplified: just treat as ambiguous ampersand for now
    m_state = TokenizerState::AmbiguousAmpersand;
}

// Ambiguous ampersand state (13.2.5.74)
void Tokenizer::handle_ambiguous_ampersand_state() {
    auto cp = peek();
    if (!cp) {
        emit_character('&');
        m_state = m_return_state;
        return;
    }

    consume();

    if (*cp == ';') {
        parse_error("unknown-named-character-reference"_s);
        m_state = m_return_state;
    } else {
        emit_character('&');
        reconsume();
        m_state = m_return_state;
    }
}

// Numeric character reference states (13.2.5.75-80)
void Tokenizer::handle_numeric_character_reference_state() {
    // No digits found
    parse_error("absence-of-digits-in-numeric-character-reference"_s);

    // Flush buffer as character tokens
    for (usize i = 0; i < m_temp_buffer.size(); ++i) {
        emit_character(m_temp_buffer.build()[i]);
    }
    m_state = m_return_state;
}

void Tokenizer::handle_hexadecimal_character_reference_start_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        for (usize i = 0; i < m_temp_buffer.size(); ++i) {
            emit_character(m_temp_buffer.build()[i]);
        }
        m_state = m_return_state;
        return;
    }

    if (std::isxdigit(*cp)) {
        m_state = TokenizerState::HexadecimalCharacterReference;
    } else {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        for (usize i = 0; i < m_temp_buffer.size(); ++i) {
            emit_character(m_temp_buffer.build()[i]);
        }
        m_state = m_return_state;
    }
}

void Tokenizer::handle_decimal_character_reference_start_state() {
    auto cp = peek();
    if (!cp) {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        for (usize i = 0; i < m_temp_buffer.size(); ++i) {
            emit_character(m_temp_buffer.build()[i]);
        }
        m_state = m_return_state;
        return;
    }

    if (std::isdigit(*cp)) {
        m_state = TokenizerState::DecimalCharacterReference;
    } else {
        parse_error("absence-of-digits-in-numeric-character-reference"_s);
        for (usize i = 0; i < m_temp_buffer.size(); ++i) {
            emit_character(m_temp_buffer.build()[i]);
        }
        m_state = m_return_state;
    }
}

void Tokenizer::handle_hexadecimal_character_reference_state() {
    // Simplified implementation - need to accumulate character reference code
    auto cp = peek();
    if (!cp) {
        parse_error("missing-semicolon-after-character-reference"_s);
        m_state = TokenizerState::NumericCharacterReferenceEnd;
        return;
    }

    consume();

    if (std::isdigit(*cp)) {
        // Add digit to character reference code (not implemented)
    } else if (*cp == ';') {
        m_state = TokenizerState::NumericCharacterReferenceEnd;
    } else {
        parse_error("missing-semicolon-after-character-reference"_s);
        m_state = TokenizerState::NumericCharacterReferenceEnd;
    }
}

void Tokenizer::handle_decimal_character_reference_state() {
    // Simplified implementation
    auto cp = peek();
    if (!cp) {
        parse_error("missing-semicolon-after-character-reference"_s);
        m_state = TokenizerState::NumericCharacterReferenceEnd;
        return;
    }

    consume();

    if (std::isdigit(*cp)) {
        // Add digit to character reference code (not implemented)
    } else if (*cp == ';') {
        m_state = TokenizerState::NumericCharacterReferenceEnd;
    } else {
        parse_error("missing-semicolon-after-character-reference"_s);
        m_state = TokenizerState::NumericCharacterReferenceEnd;
    }
}

void Tokenizer::handle_numeric_character_reference_end_state() {
    // Flush buffer with resolved character reference
    // For now, just emit '&' as fallback
    emit_character('&');
    m_state = m_return_state;
}

} // namespace lithium::html
