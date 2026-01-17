/**
 * HTML Tokenizer - token helpers
 */

#include "lithium/html/tokenizer.hpp"

namespace lithium::html {

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

} // namespace lithium::html
