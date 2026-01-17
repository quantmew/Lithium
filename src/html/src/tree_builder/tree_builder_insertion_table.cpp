/**
 * HTML Tree Builder - table-related insertion modes
 */

#include "lithium/html/tree_builder.hpp"
#include "tree_builder/constants.hpp"

namespace lithium::html {

void TreeBuilder::process_in_table(const Token& token) {
    auto clear_stack_back_to_table_context = [&] {
        while (current_node() &&
               current_node()->local_name() != "table"_s &&
               current_node()->local_name() != "html"_s) {
            pop_current_element();
        }
    };

    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == 0) {
            parse_error("Unexpected null character in table"_s);
            return;
        }
        m_pending_table_characters.clear();
        m_pending_table_characters.push_back(character->code_point);
        m_original_insertion_mode = m_insertion_mode;
        m_insertion_mode = InsertionMode::InTableText;
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE inside table"_s);
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "caption"_s) {
            clear_stack_back_to_table_context();
            clear_active_formatting_to_last_marker();
            auto element = create_element_for_token(tag);
            insert_element(element);
            push_marker();
            m_insertion_mode = InsertionMode::InCaption;
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "colgroup"_s) {
            clear_stack_back_to_table_context();
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_insertion_mode = InsertionMode::InColumnGroup;
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "col"_s) {
            TagToken colgroup_token;
            colgroup_token.name = "colgroup"_s;
            colgroup_token.is_end_tag = false;
            process_token(colgroup_token);
            process_token(token);
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }

        if (tag.name == "tbody"_s || tag.name == "tfoot"_s || tag.name == "thead"_s) {
            clear_stack_back_to_table_context();
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_insertion_mode = InsertionMode::InTableBody;
            return;
        }
        if (tag.name == "tr"_s) {
            auto tbody = m_document->create_element("tbody"_s);
            insert_element(tbody);
            m_insertion_mode = InsertionMode::InTableBody;
            process_token(token);
            return;
        }
    }

    if (is_end_tag_named(token, "table"_s)) {
        while (current_node() && current_node()->local_name() != "table"_s) {
            pop_current_element();
        }
        if (current_node()) {
            pop_current_element();
        }
        reset_insertion_mode_appropriately();
        return;
    }

    m_foster_parenting = true;
    process_using_rules_for(InsertionMode::InBody, token);
    m_foster_parenting = false;
}

void TreeBuilder::process_in_table_text(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == 0) {
            parse_error("Unexpected null character in table text"_s);
            return;
        }
        m_pending_table_characters.push_back(character->code_point);
        return;
    }

    bool previous_foster = m_foster_parenting;
    bool any_non_whitespace = false;
    for (auto cp : m_pending_table_characters) {
        if (!detail::is_ascii_whitespace(cp)) {
            any_non_whitespace = true;
            break;
        }
    }

    if (any_non_whitespace) {
        parse_error("Non-whitespace text in table context"_s);
        m_foster_parenting = true;
    }

    for (auto cp : m_pending_table_characters) {
        insert_character(cp);
    }

    m_pending_table_characters.clear();
    m_foster_parenting = previous_foster;
    m_insertion_mode = m_original_insertion_mode;
    process_token(token);
}

void TreeBuilder::process_in_caption(const Token& token) {
    if (is_end_tag_named(token, "caption"_s)) {
        if (!stack_contains_in_table_scope("caption"_s)) {
            parse_error("No caption to close in table scope"_s);
            return;
        }
        generate_implied_end_tags();
        while (current_node() && current_node()->local_name() != "caption"_s) {
            pop_current_element();
        }
        if (current_node()) {
            pop_current_element();
        }
        clear_active_formatting_to_last_marker();
        m_insertion_mode = InsertionMode::InTable;
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "table"_s || tag.name == "caption"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s || tag.name == "tr"_s ||
            tag.name == "td"_s || tag.name == "th"_s) {
            parse_error("Unexpected table tag inside caption"_s);
            if (stack_contains_in_table_scope("caption"_s)) {
                while (current_node() && current_node()->local_name() != "caption"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
                clear_active_formatting_to_last_marker();
                m_insertion_mode = InsertionMode::InTable;
                process_token(token);
            }
            return;
        }
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "body"_s || tag.name == "col"_s ||
            tag.name == "colgroup"_s || tag.name == "html"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s || tag.name == "tr"_s) {
            parse_error("Ignoring end tag in caption context"_s);
            return;
        }
    }

    process_in_body(token);
}

void TreeBuilder::process_in_column_group(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (detail::is_ascii_whitespace(character->code_point)) {
            insert_character(character->code_point);
            return;
        }
        parse_error("Non-whitespace character in colgroup"_s);
        if (current_node() && current_node()->local_name() == "colgroup"_s) {
            pop_current_element();
            m_insertion_mode = InsertionMode::InTable;
            process_token(token);
        }
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE in colgroup"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag_named(token, "col"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        pop_current_element();
        return;
    }

    if (is_end_tag_named(token, "colgroup"_s)) {
        if (current_node() && current_node()->local_name() == "colgroup"_s) {
            pop_current_element();
            m_insertion_mode = InsertionMode::InTable;
        } else {
            parse_error("No colgroup to close"_s);
        }
        return;
    }

    if (is_end_tag(token)) {
        parse_error("Unexpected end tag in colgroup"_s);
        return;
    }

    parse_error("Unexpected token in colgroup"_s);
    if (current_node() && current_node()->local_name() == "colgroup"_s) {
        pop_current_element();
        m_insertion_mode = InsertionMode::InTable;
        process_token(token);
    }
}

void TreeBuilder::process_in_table_body(const Token& token) {
    if (is_start_tag_named(token, "tr"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        m_insertion_mode = InsertionMode::InRow;
        return;
    }
    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "th"_s || tag.name == "td"_s) {
            auto tr = m_document->create_element("tr"_s);
            insert_element(tr);
            m_insertion_mode = InsertionMode::InRow;
            process_token(token);
            return;
        }
    }
    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s) {
            pop_current_element();
            m_insertion_mode = InsertionMode::InTable;
            return;
        }
        if (tag.name == "table"_s) {
            pop_current_element();
            m_insertion_mode = InsertionMode::InTable;
            process_token(token);
            return;
        }
    }
    process_in_table(token);
}

void TreeBuilder::process_in_row(const Token& token) {
    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "th"_s || tag.name == "td"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            m_insertion_mode = InsertionMode::InCell;
            push_marker();
            return;
        }
    }
    if (is_end_tag_named(token, "tr"_s)) {
        pop_current_element();
        m_insertion_mode = InsertionMode::InTableBody;
        return;
    }
    process_in_table(token);
}

void TreeBuilder::process_in_cell(const Token& token) {
    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "td"_s || tag.name == "th"_s) {
            pop_current_element();
            clear_active_formatting_to_last_marker();
            m_insertion_mode = InsertionMode::InRow;
            return;
        }
    }
    process_in_body(token);
}

void TreeBuilder::process_in_select(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (character->code_point == 0) {
            parse_error("Unexpected null in select"_s);
            return;
        }
        insert_character(character->code_point);
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE in select"_s);
        return;
    }

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "html"_s) {
            process_using_rules_for(InsertionMode::InBody, token);
            return;
        }
        if (tag.name == "option"_s) {
            if (current_node() && current_node()->local_name() == "option"_s) {
                pop_current_element();
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }
        if (tag.name == "optgroup"_s) {
            if (current_node() && current_node()->local_name() == "option"_s) {
                pop_current_element();
            }
            if (current_node() && current_node()->local_name() == "optgroup"_s) {
                pop_current_element();
            }
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (tag.self_closing) acknowledge_self_closing_flag();
            return;
        }
        if (tag.name == "select"_s) {
            parse_error("Nested select"_s);
            while (current_node()) {
                auto name = current_node()->local_name();
                pop_current_element();
                if (name == "select"_s) break;
            }
            reset_insertion_mode_appropriately();
            return;
        }
        if (tag.name == "input"_s || tag.name == "keygen"_s || tag.name == "textarea"_s) {
            parse_error("Form control inside select"_s);
            if (stack_contains("select"_s)) {
                while (current_node() && current_node()->local_name() != "select"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
                reset_insertion_mode_appropriately();
                process_token(token);
            }
            return;
        }
        if (tag.name == "script"_s || tag.name == "template"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }
        parse_error("Unexpected tag in select"_s);
        return;
    }

    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "optgroup"_s) {
            if (current_node() && current_node()->local_name() == "option"_s &&
                current_node()->parent_node() && current_node()->parent_node()->is_element() &&
                current_node()->parent_node()->as_element()->local_name() == "optgroup"_s) {
                pop_current_element();
            }
            if (current_node() && current_node()->local_name() == "optgroup"_s) {
                pop_current_element();
                return;
            }
            parse_error("No optgroup to close"_s);
            return;
        }
        if (tag.name == "option"_s) {
            if (current_node() && current_node()->local_name() == "option"_s) {
                pop_current_element();
                return;
            }
            parse_error("No option to close"_s);
            return;
        }
        if (tag.name == "select"_s) {
            if (stack_contains("select"_s)) {
                while (current_node() && current_node()->local_name() != "select"_s) {
                    pop_current_element();
                }
                if (current_node()) {
                    pop_current_element();
                }
                reset_insertion_mode_appropriately();
            } else {
                parse_error("No select to close"_s);
            }
            return;
        }
        if (tag.name == "template"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            reset_insertion_mode_appropriately();
            return;
        }
        parse_error("Unexpected end tag in select"_s);
        return;
    }

    process_in_body(token);
}

void TreeBuilder::process_in_select_in_table(const Token& token) {
    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "caption"_s || tag.name == "table"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s || tag.name == "tr"_s ||
            tag.name == "td"_s || tag.name == "th"_s) {
            parse_error("Table element inside select"_s);
            if (stack_contains("select"_s)) {
                while (current_node() && current_node()->local_name() != "select"_s) {
                    pop_current_element();
                }
                if (current_node()) pop_current_element();
                reset_insertion_mode_appropriately();
                process_token(token);
            }
            return;
        }
    }
    if (is_end_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "caption"_s || tag.name == "table"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s || tag.name == "tr"_s ||
            tag.name == "td"_s || tag.name == "th"_s) {
            parse_error("Table end tag inside select"_s);
            if (stack_contains("select"_s)) {
                while (current_node() && current_node()->local_name() != "select"_s) {
                    pop_current_element();
                }
                if (current_node()) pop_current_element();
                reset_insertion_mode_appropriately();
                process_token(token);
            }
            return;
        }
    }
    process_in_select(token);
}

void TreeBuilder::process_in_template(const Token& token) {
    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE in template"_s);
        return;
    }

    if (is_start_tag_named(token, "template"_s)) {
        process_using_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (is_end_tag_named(token, "template"_s)) {
        if (!stack_contains("template"_s)) {
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

    if (is_start_tag(token)) {
        auto& tag = std::get<TagToken>(token);
        if (tag.name == "base"_s || tag.name == "basefont"_s ||
            tag.name == "bgsound"_s || tag.name == "link"_s ||
            tag.name == "meta"_s || tag.name == "noframes"_s ||
            tag.name == "script"_s || tag.name == "style"_s ||
            tag.name == "title"_s) {
            process_using_rules_for(InsertionMode::InHead, token);
            return;
        }

        if (tag.name == "select"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (!m_template_insertion_modes.empty()) {
                m_template_insertion_modes.top() = InsertionMode::InSelect;
            }
            m_insertion_mode = InsertionMode::InSelect;
            return;
        }

        if (tag.name == "caption"_s || tag.name == "colgroup"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s || tag.name == "table"_s) {
            if (!m_template_insertion_modes.empty()) {
                m_template_insertion_modes.top() = InsertionMode::InTable;
            }
            m_insertion_mode = InsertionMode::InTable;
            process_token(token);
            return;
        }

        if (tag.name == "tr"_s) {
            if (!m_template_insertion_modes.empty()) {
                m_template_insertion_modes.top() = InsertionMode::InTableBody;
            }
            m_insertion_mode = InsertionMode::InTableBody;
            process_token(token);
            return;
        }

        if (tag.name == "td"_s || tag.name == "th"_s) {
            if (!m_template_insertion_modes.empty()) {
                m_template_insertion_modes.top() = InsertionMode::InRow;
            }
            m_insertion_mode = InsertionMode::InRow;
            process_token(token);
            return;
        }
    }

    // Anything else: switch to InBody and reprocess
    if (!m_template_insertion_modes.empty()) {
        m_template_insertion_modes.top() = InsertionMode::InBody;
    }
    m_insertion_mode = InsertionMode::InBody;
    process_token(token);
}

void TreeBuilder::process_in_frameset(const Token& token) {
    if (auto* character = std::get_if<CharacterToken>(&token)) {
        if (detail::is_ascii_whitespace(character->code_point)) {
            insert_character(character->code_point);
        } else {
            parse_error("Non-whitespace in frameset"_s);
        }
        return;
    }

    if (auto* comment = std::get_if<CommentToken>(&token)) {
        insert_comment(*comment);
        return;
    }

    if (auto* doctype = std::get_if<DoctypeToken>(&token)) {
        parse_error("Unexpected DOCTYPE in frameset"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_start_tag_named(token, "frameset"_s)) {
        auto& tag = std::get<TagToken>(token);
        auto element = create_element_for_token(tag);
        insert_element(element);
        return;
    }

    if (is_start_tag_named(token, "frame"_s)) {
        auto element = create_element_for_token(std::get<TagToken>(token));
        insert_element(element);
        pop_current_element();
        m_frameset_ok = false;
        return;
    }

    if (is_start_tag_named(token, "noframes"_s)) {
        process_using_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (is_end_tag_named(token, "frameset"_s)) {
        if (!current_node() || current_node()->local_name() != "frameset"_s) {
            parse_error("No frameset to close"_s);
            return;
        }
        pop_current_element();
        if (!current_node() || current_node()->local_name() != "frameset"_s) {
            m_insertion_mode = InsertionMode::AfterFrameset;
        }
        return;
    }

    if (std::holds_alternative<EndOfFileToken>(token)) {
        return;
    }

    parse_error("Unexpected token in frameset"_s);
}

void TreeBuilder::process_after_frameset(const Token& token) {
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
        parse_error("Unexpected DOCTYPE after frameset"_s);
        return;
    }

    if (is_start_tag_named(token, "html"_s)) {
        process_using_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (is_end_tag_named(token, "html"_s)) {
        m_insertion_mode = InsertionMode::AfterAfterFrameset;
        return;
    }

    if (is_start_tag_named(token, "noframes"_s)) {
        process_using_rules_for(InsertionMode::InHead, token);
        return;
    }

    parse_error("Unexpected token after frameset"_s);
}

} // namespace lithium::html
