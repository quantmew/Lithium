/**
 * HTML Tree Builder - table-related insertion modes
 */

#include "lithium/html/tree_builder.hpp"
#include "tree_builder/constants.hpp"

namespace lithium::html {

void TreeBuilder::process_in_table(const Token& token) {
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
        if (tag.name == "caption"_s || tag.name == "colgroup"_s ||
            tag.name == "tbody"_s || tag.name == "tfoot"_s ||
            tag.name == "thead"_s) {
            auto element = create_element_for_token(tag);
            insert_element(element);
            if (tag.name == "tbody"_s || tag.name == "tfoot"_s ||
                tag.name == "thead"_s) {
                m_insertion_mode = InsertionMode::InTableBody;
            }
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
    process_in_body(token);
}

void TreeBuilder::process_in_column_group(const Token& token) {
    process_in_body(token);
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
    process_in_body(token);
}

void TreeBuilder::process_in_select_in_table(const Token& token) {
    process_in_select(token);
}

void TreeBuilder::process_in_template(const Token& token) {
    process_in_body(token);
}

} // namespace lithium::html
