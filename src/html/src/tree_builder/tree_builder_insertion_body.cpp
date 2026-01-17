/**
 * HTML Tree Builder - body and generic insertion modes
 */

#include "lithium/html/tree_builder.hpp"
#include "tree_builder/constants.hpp"

namespace lithium::html {

void TreeBuilder::process_in_body(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == 0) {
            parse_error("Unexpected null character"_s);
            return;
        }

        reconstruct_active_formatting_elements();
        insert_character(character->code_point);

        if (character->code_point != '\t' && character->code_point != '\n' &&
            character->code_point != '\f' && character->code_point != '\r' &&
            character->code_point != ' ') {
            m_frameset_ok = false;
        }
        return;
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
        parse_error("Unexpected html tag"_s);
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);

        auto close_p = [&]() {
            if (!stack_contains_in_button_scope("p"_s)) {
                return;
            }
            generate_implied_end_tags("p"_s);
            while (current_node() && current_node()->local_name() != "p"_s) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
        };

        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s ||
            tag.name == "meta"_s || tag.name == "noframes"_s ||
            tag.name == "script"_s || tag.name == "style"_s ||
            tag.name == "template"_s || tag.name == "title"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }

        if (tag.name == "body"_s) {
            parse_error("Unexpected body tag"_s);
            return;
        }

        if (tag.name == "frameset"_s) {
            parse_error("Unexpected frameset tag"_s);
            if (!m_open_elements.empty() && m_open_elements[0]->local_name() == "html"_s) {
                while (current_node()) {
                    pop_current_element();
                }
                auto element = create_element_for_token(tag);
                insert_element(element);
                set_insertion_mode_if_allowed(InsertionMode::InFrameset, "parser-cannot-change-mode"_s);
            }
            return;
        }

        if (tag.name == "address"_s || tag.name == "article"_s ||
            tag.name == "aside"_s || tag.name == "blockquote"_s ||
            tag.name == "center"_s || tag.name == "details"_s ||
            tag.name == "dialog"_s || tag.name == "dir"_s ||
            tag.name == "div"_s || tag.name == "dl"_s ||
            tag.name == "fieldset"_s || tag.name == "figcaption"_s ||
            tag.name == "figure"_s || tag.name == "footer"_s ||
            tag.name == "header"_s || tag.name == "hgroup"_s ||
            tag.name == "main"_s || tag.name == "menu"_s ||
            tag.name == "nav"_s || tag.name == "ol"_s ||
            tag.name == "p"_s || tag.name == "section"_s ||
            tag.name == "summary"_s || tag.name == "ul"_s) {
            if (tag.name == "p"_s) {
                m_frameset_ok = false;
            }
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        if (tag.name == "h1"_s || tag.name == "h2"_s ||
            tag.name == "h3"_s || tag.name == "h4"_s ||
            tag.name == "h5"_s || tag.name == "h6"_s) {
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        if (tag.name == "pre"_s || tag.name == "listing"_s) {
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_frameset_ok = false;
            return;
        }

        if (tag.name == "form"_s) {
            if (m_form_element && !stack_contains("template"_s)) {
                parse_error("Form already open"_s);
                return;
            }
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (!stack_contains("template"_s)) {
                m_form_element = element.get();
            }
            resolve_pending_form_controls(element.get());
            return;
        }

        if (tag.name == "isindex"_s) {
            parse_error("isindex"_s);
            if (m_form_element && !stack_contains("template"_s)) {
                if (tag.self_closing) acknowledge_self_closing_flag();
                return;
            }
            close_p();

            TagToken form_token;
            form_token.name = "form"_s;
            auto form_element = create_element_for_token(form_token);
            if (auto action = tag.get_attribute("action"_s)) {
                form_element->set_attribute("action"_s, *action);
            }
            insert_element(form_element);
            if (!stack_contains("template"_s)) {
                m_form_element = form_element.get();
            }

            TagToken hr_token;
            hr_token.name = "hr"_s;
            process_token(hr_token);

            TagToken label_token;
            label_token.name = "label"_s;
            process_token(label_token);

            String prompt = tag.get_attribute("prompt"_s).value_or("This is a searchable index. Enter search keywords: "_s);
            for (char c : prompt.view()) {
                insert_character(static_cast<unicode::CodePoint>(c));
            }

            TagToken input_token;
            input_token.name = "input"_s;
            input_token.set_attribute("name"_s, "isindex"_s);
            input_token.set_attribute("type"_s, "text"_s);
            for (const auto& [name, value] : tag.attributes) {
                auto lower = name.to_lowercase();
                if (lower == "name"_s || lower == "prompt"_s || lower == "action"_s) continue;
                input_token.set_attribute(name, value);
            }
            process_token(input_token);

            if (current_node() && current_node()->local_name() == "label"_s) {
                pop_current_element();
            }

            TagToken hr2_token;
            hr2_token.name = "hr"_s;
            process_token(hr2_token);

            while (current_node() && current_node()->local_name() != "form"_s) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            m_form_element = nullptr;
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "li"_s) {
            m_frameset_ok = false;
            if (stack_contains_in_list_item_scope("li"_s)) {
                generate_implied_end_tags("li"_s);
                while (current_node() && current_node()->local_name() != "li"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        if (tag.name == "dd"_s || tag.name == "dt"_s) {
            m_frameset_ok = false;
            if (stack_contains_in_scope("dd"_s) || stack_contains_in_scope("dt"_s)) {
                while (current_node() && current_node()->local_name() != "dd"_s &&
                       current_node()->local_name() != "dt"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            return;
        }

        if (tag.name == "a"_s) {
            for (auto it = m_active_formatting_elements.rbegin();
                 it != m_active_formatting_elements.rend(); ++it) {
                if (it->type == ActiveFormattingElement::Type::Marker) {
                    break;
                }
                if (it->element && it->element->local_name() == "a"_s) {
                    parse_error("Nested <a> element"_s);
                    adoption_agency_algorithm("a"_s);
                    remove_from_active_formatting(it->element.get());
                    remove_from_stack(it->element.get());
                    break;
                }
            }
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            push_active_formatting_element(element, token);
            return;
        }

        if (tag.name == "b"_s || tag.name == "big"_s || tag.name == "code"_s ||
            tag.name == "em"_s || tag.name == "font"_s || tag.name == "i"_s ||
            tag.name == "s"_s || tag.name == "small"_s || tag.name == "strike"_s ||
            tag.name == "strong"_s || tag.name == "tt"_s || tag.name == "u"_s) {
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            push_active_formatting_element(element, token);
            return;
        }

        if (tag.name == "area"_s || tag.name == "br"_s || tag.name == "embed"_s ||
            tag.name == "img"_s || tag.name == "keygen"_s || tag.name == "wbr"_s) {
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            m_frameset_ok = false;
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "input"_s) {
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            auto type_attr = tag.get_attribute("type"_s);
            if (!type_attr || type_attr->to_lowercase() != "hidden"_s) {
                m_frameset_ok = false;
            }
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "hr"_s) {
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            pop_current_element();
            m_frameset_ok = false;
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "select"_s) {
            close_p();
            reconstruct_active_formatting_elements();
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_frameset_ok = false;
            if (m_insertion_mode == InsertionMode::InTable ||
                m_insertion_mode == InsertionMode::InTableBody ||
                m_insertion_mode == InsertionMode::InRow ||
                m_insertion_mode == InsertionMode::InCell) {
                m_insertion_mode = InsertionMode::InSelectInTable;
            } else {
                m_insertion_mode = InsertionMode::InSelect;
            }
            return;
        }

        if (tag.name == "textarea"_s) {
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::RCDATA);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_frameset_ok = false;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "plaintext"_s) {
            close_p();
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (m_tokenizer) {
                m_tokenizer->set_state(TokenizerState::PLAINTEXT);
            }
            m_original_insertion_mode = m_insertion_mode;
            m_frameset_ok = false;
            m_insertion_mode = InsertionMode::Text;
            return;
        }

        if (tag.name == "table"_s) {
            if (m_document->quirks_mode() != dom::Document::QuirksMode::Quirks &&
                stack_contains_in_button_scope("p"_s)) {
                while (current_node() && current_node()->local_name() != "p"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_frameset_ok = false;
            m_insertion_mode = InsertionMode::InTable;
            return;
        }

        reconstruct_active_formatting_elements();
        auto element = create_element_for_token(tag);
        insert_element(element);
        if (tag.self_closing) {
            parse_error("Self-closing non-void element"_s);
            acknowledge_self_closing_flag();
        }
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);

        if (tag.name == "template"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }

        if (tag.name == "body"_s) {
            if (!stack_contains_in_scope("body"_s)) {
                parse_error("No body to close"_s);
                return;
            }
            m_insertion_mode = InsertionMode::AfterBody;
            return;
        }

        if (tag.name == "html"_s) {
            if (!stack_contains_in_scope("body"_s)) {
                parse_error("No body to close"_s);
                return;
            }
            m_insertion_mode = InsertionMode::AfterBody;
            process_token(token);
            return;
        }

        if (tag.name == "address"_s || tag.name == "article"_s ||
            tag.name == "aside"_s || tag.name == "blockquote"_s ||
            tag.name == "button"_s || tag.name == "center"_s ||
            tag.name == "details"_s || tag.name == "dialog"_s ||
            tag.name == "dir"_s || tag.name == "div"_s ||
            tag.name == "dl"_s || tag.name == "fieldset"_s ||
            tag.name == "figcaption"_s || tag.name == "figure"_s ||
            tag.name == "footer"_s || tag.name == "header"_s ||
            tag.name == "hgroup"_s || tag.name == "listing"_s ||
            tag.name == "main"_s || tag.name == "menu"_s ||
            tag.name == "nav"_s || tag.name == "ol"_s ||
            tag.name == "pre"_s || tag.name == "section"_s ||
            tag.name == "summary"_s || tag.name == "ul"_s) {
            if (!stack_contains_in_scope(tag.name)) {
                parse_error("No matching tag in scope"_s);
                return;
            }
            generate_implied_end_tags();
            while (current_node() && current_node()->local_name() != tag.name) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        if (tag.name == "form"_s) {
            if (!stack_contains("template"_s)) {
                auto* form = m_form_element;
                m_form_element = nullptr;
                if (!form || !stack_contains_in_scope("form"_s)) {
                    parse_error("No form to close"_s);
                    return;
                }
                generate_implied_end_tags();
                remove_from_stack(form);
            }
            return;
        }

        if (tag.name == "p"_s) {
            if (!stack_contains_in_button_scope("p"_s)) {
                parse_error("No p to close"_s);
                auto p = m_document->create_element("p"_s);
                insert_element(p);
            }
            generate_implied_end_tags("p"_s);
            if (!current_node() || current_node()->local_name() != "p"_s) {
                parse_error("Closing p but current node is different"_s);
            }
            while (current_node()) {
                auto name = current_node()->local_name();
                pop_current_element();
                if (name == "p"_s) {
                    break;
                }
            }
            return;
        }

        if (tag.name == "li"_s) {
            if (!stack_contains_in_list_item_scope("li"_s)) {
                parse_error("No li to close"_s);
                return;
            }
            generate_implied_end_tags("li"_s);
            while (current_node() && current_node()->local_name() != "li"_s) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        if (tag.name == "dd"_s || tag.name == "dt"_s) {
            if (!stack_contains_in_scope(tag.name)) {
                parse_error("No matching tag to close"_s);
                return;
            }
            generate_implied_end_tags(tag.name);
            while (current_node() && current_node()->local_name() != tag.name) {
                pop_current_element();
            }
            if (current_node()) {
                pop_current_element();
            }
            return;
        }

        if (tag.name == "h1"_s || tag.name == "h2"_s ||
            tag.name == "h3"_s || tag.name == "h4"_s ||
            tag.name == "h5"_s || tag.name == "h6"_s) {
            if (!stack_contains_in_scope("h1"_s) &&
                !stack_contains_in_scope("h2"_s) &&
                !stack_contains_in_scope("h3"_s) &&
                !stack_contains_in_scope("h4"_s) &&
                !stack_contains_in_scope("h5"_s) &&
                !stack_contains_in_scope("h6"_s)) {
                parse_error("No heading to close"_s);
                return;
            }
            generate_implied_end_tags();
            while (current_node()) {
                auto name = current_node()->local_name();
                pop_current_element();
                if (name == "h1"_s || name == "h2"_s || name == "h3"_s ||
                    name == "h4"_s || name == "h5"_s || name == "h6"_s) {
                    break;
                }
            }
            return;
        }

        if (tag.name == "a"_s || tag.name == "b"_s || tag.name == "big"_s ||
            tag.name == "code"_s || tag.name == "em"_s || tag.name == "font"_s ||
            tag.name == "i"_s || tag.name == "nobr"_s || tag.name == "s"_s ||
            tag.name == "small"_s || tag.name == "strike"_s ||
            tag.name == "strong"_s || tag.name == "tt"_s || tag.name == "u"_s) {
            adoption_agency_algorithm(tag.name);
            return;
        }

        static const String SVG_NS = "http://www.w3.org/2000/svg"_s;
        static const String MATHML_NS = "http://www.w3.org/1998/Math/MathML"_s;

        for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
            auto* node = it->get();
            auto expected_name = tag.name;
            if (node->namespace_uri() == SVG_NS) {
                expected_name = svg_camel_case(tag.name.to_lowercase());
            } else if (node->namespace_uri() == MATHML_NS) {
                expected_name = tag.name.to_lowercase();
            }

            if (node->local_name() == expected_name) {
                generate_implied_end_tags(expected_name);
                while (current_node() != node) {
                    pop_current_element();
                }
                pop_current_element();
                return;
            }
            if (is_special_element(node->local_name())) {
                parse_error("Unexpected end tag"_s);
                return;
            }
        }
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return;
    }
}

void TreeBuilder::process_text(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        insert_character(character->code_point);
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        parse_error("Unexpected EOF in text"_s);
        pop_current_element();
        m_insertion_mode = m_original_insertion_mode;
        if (m_insertion_mode == InsertionMode::Text) {
            return;
        }
        process_token(token);
        return;
    }

    if (is_end_tag(token)) {
        pop_current_element();
        m_insertion_mode = m_original_insertion_mode;
        return;
    }
}

void TreeBuilder::process_after_body(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            process_using_rules_for(InsertionMode::InBody, token);
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

    if (is_end_tag_named(token, "html"_s)) {
        set_insertion_mode_if_allowed(InsertionMode::AfterAfterBody, "parser-cannot-change-mode"_s);
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return;
    }

    parse_error("Unexpected token after body"_s);
    set_insertion_mode_if_allowed(InsertionMode::InBody);
    process_token(token);
}

void TreeBuilder::process_after_after_body(const Token& token) {
    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment, m_document.get());
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == '\t' || character->code_point == '\n' ||
            character->code_point == '\f' || character->code_point == '\r' ||
            character->code_point == ' ') {
            process_using_rules_for(InsertionMode::InBody, token);
            return;
        }
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return;
    }

    parse_error("Unexpected token after after body"_s);
    m_insertion_mode = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::process_after_after_frameset(const Token& token) {
    process_after_after_body(token);
}

} // namespace lithium::html
