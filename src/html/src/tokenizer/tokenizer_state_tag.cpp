/**
 * HTML Tokenizer - tag and attribute states
 */

#include "lithium/html/tokenizer.hpp"
#include <cctype>

namespace lithium::html {

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

} // namespace lithium::html
