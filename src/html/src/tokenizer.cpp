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

    if (*cp == '\t' || *cp == '\n' || *cp == '\f' || *cp == ' ') {
        // Ignore
    } else if (*cp == '/') {
        m_state = TokenizerState::SelfClosingStartTag;
    } else if (*cp == '=') {
        m_state = TokenizerState::BeforeAttributeValue;
    } else if (*cp == '>') {
        m_state = TokenizerState::Data;
        emit_current_token();
    } else {
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

} // namespace lithium::html
