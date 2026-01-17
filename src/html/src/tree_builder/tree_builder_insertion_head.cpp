/**
 * HTML Tree Builder - insertion modes for document head
 */

#include "lithium/html/tree_builder.hpp"
#include "tree_builder/constants.hpp"

namespace lithium::html {

void TreeBuilder::process_initial(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment, m_document.get());
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        auto dt = m_document->create_document_type(
            doctype->name,
            doctype->public_identifier.value_or(String()),
            doctype->system_identifier.value_or(String())
        );
        m_document->append_child(dt);

        if (doctype->force_quirks) {
            m_document->set_quirks_mode(dom::Document::QuirksMode::Quirks);
        }

        m_insertion_mode = InsertionMode::BeforeHtml;
        return;
    }

    m_document->set_quirks_mode(dom::Document::QuirksMode::Quirks);
    m_insertion_mode = InsertionMode::BeforeHtml;
    process_token(token);
}

void TreeBuilder::process_before_html(const Token& token) {
    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment, m_document.get());
        return;
    }

    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            return;
        }
    }

    if (is_start_tag_named(token, "html"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        m_document->append_child(element);
        push_open_element(element);
        m_insertion_mode = InsertionMode::BeforeHead;
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name != "head"_s && tag.name != "body"_s &&
            tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    auto html = m_document->create_element("html"_s);
    m_document->append_child(html);
    push_open_element(html);
    m_insertion_mode = InsertionMode::BeforeHead;
    process_token(token);
}

void TreeBuilder::process_before_head(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag_named(token, "head"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto head = create_element_for_token(tag);
        insert_element(head);
        m_head_element = head.get();
        m_insertion_mode = InsertionMode::InHead;
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name != "head"_s && tag.name != "body"_s &&
            tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    auto head = m_document->create_element("head"_s);
    insert_element(head);
    m_head_element = head.get();
    m_insertion_mode = InsertionMode::InHead;
    process_token(token);
}

void TreeBuilder::process_in_head(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            insert_character(character->code_point);
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            return;
        }

        if (tag.name == "meta"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            return;
        }

        if (tag.name == "title"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RCDATA);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "style"_s || tag.name == "noscript"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RAWTEXT);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "script"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::ScriptData);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "head"_s) {
            parse_error("Unexpected head tag"_s);
            return;
        }
    }

    if (is_end_tag_named(token, "head"_s)) {
        pop_current_element();
        m_insertion_mode = InsertionMode::AfterHead;
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name != "body"_s && tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    pop_current_element();
    m_insertion_mode = InsertionMode::AfterHead;
    process_token(token);
}

void TreeBuilder::process_in_head_noscript(const Token& token) {
    process_in_head(token);
}

void TreeBuilder::process_after_head(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            insert_character(character->code_point);
            return;
        }
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag_named(token, "body"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        m_frameset_ok = false;
        m_insertion_mode = InsertionMode::InBody;
        return;
    }

    if (is_start_tag_named(token, "frameset"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        m_insertion_mode = InsertionMode::InFrameset;
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s ||
            tag.name == "meta"_s || tag.name == "noframes"_s ||
            tag.name == "script"_s || tag.name == "style"_s ||
            tag.name == "template"_s || tag.name == "title"_s) {
            parse_error("Unexpected tag in after head"_s);
            push_open_element(RefPtr<dom::Element>(m_head_element));
            process_using_rules_for(InsertionMode::InHead, token);
            remove_from_stack(m_head_element);
            return;
        }

        if (tag.name == "head"_s) {
            parse_error("Unexpected head tag"_s);
            return;
        }
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "template"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }
        if (tag.name != "body"_s && tag.name != "html"_s && tag.name != "br"_s) {
            parse_error("Unexpected end tag"_s);
            return;
        }
    }

    auto body = m_document->create_element("body"_s);
    insert_element(body);
    m_insertion_mode = InsertionMode::InBody;
    process_token(token);
}

} // namespace lithium::html
