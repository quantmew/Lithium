/**
 * HTML Tokenizer - core driver and shared helpers
 */

#include "lithium/html/tokenizer.hpp"
#include <cctype>

namespace lithium::html {

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
    auto cp = peek();
    if (cp && *cp == expected) {
        consume();
        return true;
    }
    return false;
}

bool Tokenizer::consume_if_match(std::string_view str, bool case_insensitive) {
    if (m_position + str.size() > m_input.length()) {
        return false;
    }

    for (usize i = 0; i < str.size(); ++i) {
        char c = static_cast<char>(m_input[m_position + i]);
        char s = static_cast<char>(str[i]);
        if (case_insensitive) {
            if (std::tolower(c) != std::tolower(s)) {
                return false;
            }
        } else if (c != s) {
            return false;
        }
    }

    m_position += str.size();
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
        case TokenizerState::CommentStartDash:
            handle_comment_start_dash_state();
            break;
        case TokenizerState::Comment:
            handle_comment_state();
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
        case TokenizerState::CommentEndDash:
            handle_comment_end_dash_state();
            break;
        case TokenizerState::CommentEnd:
            handle_comment_end_state();
            break;
        case TokenizerState::CommentEndBang:
            handle_comment_end_bang_state();
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
        case TokenizerState::CDATASection:
            handle_cdata_section_state();
            break;
        case TokenizerState::CDATASectionBracket:
            handle_cdata_section_bracket_state();
            break;
        case TokenizerState::CDATASectionEnd:
            handle_cdata_section_end_state();
            break;
        case TokenizerState::CharacterReference:
            handle_character_reference_state();
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
            handle_data_state();
            break;
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
}

void Tokenizer::finish_attribute_value() {
    if (m_current_token && std::holds_alternative<TagToken>(*m_current_token)) {
        auto& tag = std::get<TagToken>(*m_current_token);
        tag.set_attribute(m_current_attribute_name, m_current_attribute_value);
    }
    m_current_attribute_name.clear();
    m_current_attribute_value.clear();
}

} // namespace lithium::html
