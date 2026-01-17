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

        auto public_id = doctype->public_identifier.value_or(String());
        auto system_id = doctype->system_identifier.value_or(String());

        auto starts_with = [](const String& s, const String& prefix) {
            return s.length() >= prefix.length() && s.substring(0, prefix.length()) == prefix;
        };

        bool quirks = doctype->force_quirks || doctype->name.to_lowercase() != "html"_s;
        bool limited_quirks = false;

        static const char* QUIRKS_PUBLIC_EXACT[] = {
            "-//w3o//dtd w3 html strict 3.0//en//",
            "-/w3c/dtd html 4.0 transitional/en",
            "html"
        };

        static const char* QUIRKS_SYSTEM_IDS[] = {
            "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"
        };

        static const char* QUIRKS_PREFIXES[] = {
            "+//silmaril//dtd html pro v0r11 19970101//",
            "-//as//dtd html 3.0 aswedit + extensions//",
            "-//advasoft ltd//dtd html 3.0 aswedit + extensions//",
            "-//ietf//dtd html 2.0 level 1//",
            "-//ietf//dtd html 2.0 level 2//",
            "-//ietf//dtd html 2.0 strict level 1//",
            "-//ietf//dtd html 2.0 strict level 2//",
            "-//ietf//dtd html 2.0 strict//",
            "-//ietf//dtd html 2.0//",
            "-//ietf//dtd html 2.1e//",
            "-//ietf//dtd html 3.0//",
            "-//ietf//dtd html 3.2 final//",
            "-//ietf//dtd html 3.2//",
            "-//ietf//dtd html 3//",
            "-//ietf//dtd html level 0//",
            "-//ietf//dtd html level 1//",
            "-//ietf//dtd html level 2//",
            "-//ietf//dtd html level 3//",
            "-//ietf//dtd html strict level 0//",
            "-//ietf//dtd html strict level 1//",
            "-//ietf//dtd html strict level 2//",
            "-//ietf//dtd html strict level 3//",
            "-//ietf//dtd html strict//",
            "-//ietf//dtd html//",
            "-//metrius//dtd metrius presentational//",
            "-//microsoft//dtd internet explorer 2.0 html strict//",
            "-//microsoft//dtd internet explorer 2.0 html//",
            "-//microsoft//dtd internet explorer 2.0 tables//",
            "-//microsoft//dtd internet explorer 3.0 html strict//",
            "-//microsoft//dtd internet explorer 3.0 html//",
            "-//microsoft//dtd internet explorer 3.0 tables//",
            "-//netscape comm. corp.//dtd html//",
            "-//netscape comm. corp.//dtd strict html//",
            "-//o'reilly and associates//dtd html 2.0//",
            "-//o'reilly and associates//dtd html extended 1.0//",
            "-//o'reilly and associates//dtd html extended relaxed 1.0//",
            "-//sq//dtd html 2.0 hotmetal + extensions//",
            "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//",
            "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//",
            "-//spyglass//dtd html 2.0 extended//",
            "-//sun microsystems corp.//dtd hotjava html//",
            "-//sun microsystems corp.//dtd hotjava strict html//",
            "-//w3c//dtd html 3 1995-03-24//",
            "-//w3c//dtd html 3.2 draft//",
            "-//w3c//dtd html 3.2 final//",
            "-//w3c//dtd html 3.2//",
            "-//w3c//dtd html 3.2s draft//",
            "-//w3c//dtd html 4.0 frameset//",
            "-//w3c//dtd html 4.0 transitional//",
            "-//w3c//dtd html experimental 19960712//",
            "-//w3c//dtd html experimental 970421//",
            "-//w3c//dtd w3 html//",
            "-//w3o//dtd w3 html 3.0//",
            "-//webtechs//dtd mozilla html 2.0//",
            "-//webtechs//dtd mozilla html//"
        };

        static const char* LIMITED_QUIRKS_PREFIXES[] = {
            "-//w3c//dtd xhtml 1.0 frameset//",
            "-//w3c//dtd xhtml 1.0 transitional//"
        };

        static const char* LIMITED_QUIRKS_NEED_SYSTEM[] = {
            "-//w3c//dtd html 4.01 frameset//",
            "-//w3c//dtd html 4.01 transitional//"
        };

        auto public_lower = public_id.to_lowercase();
        auto system_lower = system_id.to_lowercase();
        bool public_missing = !doctype->public_identifier.has_value();
        bool system_missing = !doctype->system_identifier.has_value();
        bool system_empty = system_id.empty();

        if (!quirks && !public_missing) {
            for (const auto* val : QUIRKS_PUBLIC_EXACT) {
                if (public_lower == String(val)) {
                    quirks = true;
                    break;
                }
            }
        }

        if (!quirks && !system_missing) {
            for (const auto* val : QUIRKS_SYSTEM_IDS) {
                if (system_lower == String(val)) {
                    quirks = true;
                    break;
                }
            }
        }

        if (!quirks && !public_missing) {
            for (const auto* prefix : QUIRKS_PREFIXES) {
                if (starts_with(public_lower, String(prefix))) {
                    quirks = true;
                    break;
                }
            }
        }

        if (!quirks && (system_missing || system_empty)) {
            if (starts_with(public_lower, "-//w3c//dtd html 4.01 frameset//"_s) ||
                starts_with(public_lower, "-//w3c//dtd html 4.01 transitional//"_s)) {
                quirks = true;
            }
        }

        bool allow_limited_quirks = !m_is_iframe_srcdoc && !m_parser_cannot_change_mode;

        if (!quirks && allow_limited_quirks) {
            for (const auto* prefix : LIMITED_QUIRKS_PREFIXES) {
                if (starts_with(public_lower, String(prefix))) {
                    limited_quirks = true;
                    break;
                }
            }
        }

        if (!quirks && allow_limited_quirks && !limited_quirks && !system_missing && !system_empty) {
            for (const auto* prefix : LIMITED_QUIRKS_NEED_SYSTEM) {
                if (starts_with(public_lower, String(prefix))) {
                    limited_quirks = true;
                    break;
                }
            }
        }

        if (limited_quirks && allow_limited_quirks) {
            m_document->set_quirks_mode(dom::Document::QuirksMode::LimitedQuirks);
        } else if (quirks) {
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
    if (std::holds_alternative<DoctypeToken>(token)) {
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

    if (std::holds_alternative<DoctypeToken>(token)) {
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

    if (std::holds_alternative<DoctypeToken>(token)) {
        parse_error("Unexpected DOCTYPE"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "template"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            push_marker();
            m_template_insertion_modes.push(InsertionMode::InTemplate);
            m_frameset_ok = false;
            m_insertion_mode = InsertionMode::InTemplate;
            return;
        }

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

        if (tag.name == "style"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RAWTEXT);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "noscript"_s) {
            if (m_scripting_enabled) {
                auto element = create_element_for_token(tag);
                insert_element(element);
                if (m_tokenizer) {
                    m_tokenizer->set_state(TokenizerState::RAWTEXT);
                }
                m_original_insertion_mode = m_insertion_mode;
                m_insertion_mode = InsertionMode::Text;
            } else {
                auto element = create_element_for_token(tag);
                insert_element(element);
                m_insertion_mode = InsertionMode::InHeadNoscript;
            }
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
    if (is_end_tag_named(token, "template"_s)) {
        if (!stack_contains_in_scope("template"_s)) {
            parse_error("No template to close"_s);
            return;
        }
        generate_implied_end_tags();
        while (current_node() && current_node()->local_name() != "template"_s) {
            pop_current_element();
        }
        if (current_node()) {
            pop_current_element();
        }
        clear_active_formatting_to_last_marker();
        if (!m_template_insertion_modes.empty()) {
            m_template_insertion_modes.pop();
        }
        reset_insertion_mode_appropriately();
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
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (detail::is_ascii_whitespace(character->code_point)) {
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

    if (is_end_tag_named(token, "noscript"_s)) {
        pop_current_element();
        m_insertion_mode = InsertionMode::InHead;
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "basefont"_s || tag.name == "bgsound"_s ||
            tag.name == "link"_s || tag.name == "meta"_s || tag.name == "noframes"_s ||
            tag.name == "style"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }
        if (tag.name == "head"_s || tag.name == "noscript"_s) {
            parse_error("Unexpected start tag in noscript head"_s);
            return;
        }
        parse_error("Unexpected start tag in noscript head"_s);
        pop_current_element();
        m_insertion_mode = InsertionMode::InHead;
        process_token(token);
        return;
    }

    if (is_end_tag(token)) {
        parse_error("Unexpected end tag in noscript head"_s);
        return;
    }

    pop_current_element();
    m_insertion_mode = InsertionMode::InHead;
    process_token(token);
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

    if (std::holds_alternative<DoctypeToken>(token)) {
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
