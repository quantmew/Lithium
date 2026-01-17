/**
 * HTML Tree Builder core (shared helpers and glue)
 */

#include "lithium/html/tree_builder.hpp"
#include "tree_builder/constants.hpp"
#include "lithium/dom/text.hpp"
#include <algorithm>

namespace lithium::html {

TreeBuilder::TreeBuilder() = default;

void TreeBuilder::set_document(RefPtr<dom::Document> document) {
    m_document = document;
}

void TreeBuilder::process_token(const Token& token) {
    switch (m_insertion_mode) {
        case InsertionMode::Initial:
            process_initial(token);
            break;
        case InsertionMode::BeforeHtml:
            process_before_html(token);
            break;
        case InsertionMode::BeforeHead:
            process_before_head(token);
            break;
        case InsertionMode::InHead:
            process_in_head(token);
            break;
        case InsertionMode::InHeadNoscript:
            process_in_head_noscript(token);
            break;
        case InsertionMode::AfterHead:
            process_after_head(token);
            break;
        case InsertionMode::InBody:
            process_in_body(token);
            break;
        case InsertionMode::Text:
            process_text(token);
            break;
        case InsertionMode::InTable:
            process_in_table(token);
            break;
        case InsertionMode::InTableText:
            process_in_table_text(token);
            break;
        case InsertionMode::InCaption:
            process_in_caption(token);
            break;
        case InsertionMode::InColumnGroup:
            process_in_column_group(token);
            break;
        case InsertionMode::InTableBody:
            process_in_table_body(token);
            break;
        case InsertionMode::InRow:
            process_in_row(token);
            break;
        case InsertionMode::InCell:
            process_in_cell(token);
            break;
        case InsertionMode::InSelect:
            process_in_select(token);
            break;
        case InsertionMode::InSelectInTable:
            process_in_select_in_table(token);
            break;
        case InsertionMode::InTemplate:
            process_in_template(token);
            break;
        case InsertionMode::AfterBody:
            process_after_body(token);
            break;
        case InsertionMode::InFrameset:
            process_in_frameset(token);
            break;
        case InsertionMode::AfterFrameset:
            process_after_frameset(token);
            break;
        case InsertionMode::AfterAfterBody:
            process_after_after_body(token);
            break;
        case InsertionMode::AfterAfterFrameset:
            process_after_after_frameset(token);
            break;
        default:
            process_in_body(token);
            break;
    }
}

void TreeBuilder::process_using_rules_for(InsertionMode mode, const Token& token) {
    auto saved_mode = m_insertion_mode;
    m_insertion_mode = mode;
    process_token(token);
    m_insertion_mode = saved_mode;
}

RefPtr<dom::Element> TreeBuilder::create_element(const TagToken& token, const String& /*namespace_uri*/) {
    auto element = m_document->create_element(token.name);
    for (const auto& [name, value] : token.attributes) {
        element->set_attribute(name, value);
    }
    return element;
}

RefPtr<dom::Element> TreeBuilder::create_element_for_token(const TagToken& token) {
    return create_element(token, String());
}

void TreeBuilder::insert_element(RefPtr<dom::Element> element) {
    auto insertion = appropriate_insertion_place();
    if (insertion.parent) {
        if (insertion.insert_before) {
            insertion.parent->insert_before(element, insertion.insert_before);
        } else {
            insertion.parent->append_child(element);
        }
    }
    push_open_element(element);
}

void TreeBuilder::insert_character(unicode::CodePoint cp) {
    auto insertion = appropriate_insertion_place();
    auto* insert_parent = insertion.parent;
    if (!insert_parent) return;

    dom::Node* adjacent = insertion.insert_before
        ? insertion.insert_before->previous_sibling()
        : insert_parent->last_child();

    if (adjacent && adjacent->is_text()) {
        adjacent->as_text()->append_data(String(1, static_cast<char>(cp)));
        return;
    }

    auto text = m_document->create_text_node(String(1, static_cast<char>(cp)));
    if (insertion.insert_before) {
        insert_parent->insert_before(text, insertion.insert_before);
    } else {
        insert_parent->append_child(text);
    }
}

void TreeBuilder::insert_comment(const CommentToken& token, dom::Node* position) {
    auto comment = m_document->create_comment(token.data);
    if (position) {
        position->append_child(comment);
    } else if (current_node()) {
        current_node()->append_child(comment);
    } else {
        m_document->append_child(comment);
    }
}

dom::Element* TreeBuilder::current_node() const {
    if (m_open_elements.empty()) return nullptr;
    return m_open_elements.back().get();
}

dom::Element* TreeBuilder::adjusted_current_node() const {
    if (m_context_element && m_open_elements.size() == 1) {
        return m_context_element;
    }
    return current_node();
}

void TreeBuilder::push_open_element(RefPtr<dom::Element> element) {
    m_open_elements.push_back(element);
}

void TreeBuilder::pop_current_element() {
    if (!m_open_elements.empty()) {
        m_open_elements.pop_back();
    }
}

void TreeBuilder::remove_from_stack(dom::Element* element) {
    m_open_elements.erase(
        std::remove_if(m_open_elements.begin(), m_open_elements.end(),
            [element](const RefPtr<dom::Element>& e) { return e.get() == element; }),
        m_open_elements.end());
}

bool TreeBuilder::stack_contains(const String& tag_name) const {
    for (const auto& elem : m_open_elements) {
        if (elem->local_name() == tag_name) {
            return true;
        }
    }
    return false;
}

bool TreeBuilder::stack_contains_in_scope(const String& tag_name) const {
    static const std::array<const char*, 10> scope_markers = {
        "applet", "caption", "html", "table", "td", "th", "marquee", "object", "template", "foreignObject"
    };

    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        for (const char* marker : scope_markers) {
            if ((*it)->local_name() == String(marker)) {
                return false;
            }
        }
    }
    return false;
}

bool TreeBuilder::stack_contains_in_list_item_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() == "ol"_s || (*it)->local_name() == "ul"_s) {
            return false;
        }
    }
    return stack_contains_in_scope(tag_name);
}

bool TreeBuilder::stack_contains_in_button_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() == "button"_s) {
            return false;
        }
    }
    return stack_contains_in_scope(tag_name);
}

bool TreeBuilder::stack_contains_in_table_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() == "html"_s || (*it)->local_name() == "table"_s ||
            (*it)->local_name() == "template"_s) {
            return false;
        }
    }
    return false;
}

bool TreeBuilder::stack_contains_in_select_scope(const String& tag_name) const {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            return true;
        }
        if ((*it)->local_name() != "optgroup"_s && (*it)->local_name() != "option"_s) {
            return false;
        }
    }
    return false;
}

void TreeBuilder::push_active_formatting_element(RefPtr<dom::Element> element, const Token& token) {
    m_active_formatting_elements.push_back({
        ActiveFormattingElement::Type::Element,
        element,
        token
    });
}

void TreeBuilder::push_marker() {
    m_active_formatting_elements.push_back(ActiveFormattingElement::marker());
}

void TreeBuilder::reconstruct_active_formatting_elements() {
    if (m_active_formatting_elements.empty()) return;

    auto& last = m_active_formatting_elements.back();
    if (last.type == ActiveFormattingElement::Type::Marker) return;

    for (const auto& elem : m_open_elements) {
        if (elem.get() == last.element.get()) {
            return;
        }
    }

    for (auto it = m_active_formatting_elements.rbegin(); it != m_active_formatting_elements.rend(); ++it) {
        if (it->type == ActiveFormattingElement::Type::Marker) break;

        bool on_stack = false;
        for (const auto& elem : m_open_elements) {
            if (elem.get() == it->element.get()) {
                on_stack = true;
                break;
            }
        }

        if (!on_stack) {
            auto& tag = std::get<TagToken>(it->token);
            auto element = create_element_for_token(tag);
            insert_element(element);
            it->element = element;
        }
    }
}

void TreeBuilder::clear_active_formatting_to_last_marker() {
    while (!m_active_formatting_elements.empty()) {
        auto& last = m_active_formatting_elements.back();
        if (last.type == ActiveFormattingElement::Type::Marker) {
            m_active_formatting_elements.pop_back();
            break;
        }
        m_active_formatting_elements.pop_back();
    }
}

void TreeBuilder::remove_from_active_formatting(dom::Element* element) {
    m_active_formatting_elements.erase(
        std::remove_if(m_active_formatting_elements.begin(), m_active_formatting_elements.end(),
            [element](const ActiveFormattingElement& afe) {
                return afe.element.get() == element;
            }),
        m_active_formatting_elements.end());
}

void TreeBuilder::adoption_agency_algorithm(const String& tag_name) {
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        if ((*it)->local_name() == tag_name) {
            while (current_node() != it->get()) {
                pop_current_element();
            }
            pop_current_element();
            remove_from_active_formatting(it->get());
            return;
        }
        if (is_special_element((*it)->local_name())) {
            parse_error("Unexpected special element"_s);
            return;
        }
    }
}

TreeBuilder::InsertionLocation TreeBuilder::appropriate_insertion_place() {
    if (m_foster_parenting) {
        for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
            if ((*it)->local_name() == "table"_s) {
                auto* table = it->get();
                if (auto* parent = table->parent_node()) {
                    return {parent, table};
                }
                auto next = it + 1;
                if (next != m_open_elements.rend()) {
                    return {next->get(), nullptr};
                }
            }
        }
    }

    return {adjusted_current_node(), nullptr};
}

void TreeBuilder::generate_implied_end_tags(const String& except) {
    while (current_node()) {
        auto name = current_node()->local_name();
        if (name == except) break;

        bool is_implied = false;
        for (const char* tag : detail::IMPLIED_END_TAG_ELEMENTS) {
            if (name == String(tag)) {
                is_implied = true;
                break;
            }
        }

        if (!is_implied) break;
        pop_current_element();
    }
}

void TreeBuilder::generate_all_implied_end_tags_thoroughly() {
    while (current_node()) {
        auto name = current_node()->local_name();

        bool is_implied = false;
        for (const char* tag : detail::IMPLIED_END_TAG_ELEMENTS) {
            if (name == String(tag)) {
                is_implied = true;
                break;
            }
        }
        if (name == "caption"_s || name == "colgroup"_s || name == "tbody"_s ||
            name == "td"_s || name == "tfoot"_s || name == "th"_s ||
            name == "thead"_s || name == "tr"_s) {
            is_implied = true;
        }

        if (!is_implied) break;
        pop_current_element();
    }
}

bool TreeBuilder::is_special_element(const String& tag_name) {
    for (const char* special : detail::SPECIAL_ELEMENTS) {
        if (tag_name == String(special)) {
            return true;
        }
    }
    return false;
}

bool TreeBuilder::is_formatting_element(const String& tag_name) {
    for (const char* fmt : detail::FORMATTING_ELEMENTS) {
        if (tag_name == String(fmt)) {
            return true;
        }
    }
    return false;
}

void TreeBuilder::parse_error(const String& message) {
    if (m_error_callback) {
        m_error_callback(message);
    }
}

void TreeBuilder::reset_insertion_mode_appropriately() {
    bool last = false;
    for (auto it = m_open_elements.rbegin(); it != m_open_elements.rend(); ++it) {
        auto* node = it->get();

        if (it == m_open_elements.rend() - 1) {
            last = true;
            if (m_context_element) {
                node = m_context_element;
            }
        }

        auto name = node->local_name();

        if (name == "select"_s) {
            m_insertion_mode = InsertionMode::InSelect;
            return;
        }
        if (name == "td"_s || name == "th"_s) {
            if (!last) {
                m_insertion_mode = InsertionMode::InCell;
                return;
            }
        }
        if (name == "tr"_s) {
            m_insertion_mode = InsertionMode::InRow;
            return;
        }
        if (name == "tbody"_s || name == "thead"_s || name == "tfoot"_s) {
            m_insertion_mode = InsertionMode::InTableBody;
            return;
        }
        if (name == "caption"_s) {
            m_insertion_mode = InsertionMode::InCaption;
            return;
        }
        if (name == "colgroup"_s) {
            m_insertion_mode = InsertionMode::InColumnGroup;
            return;
        }
        if (name == "table"_s) {
            m_insertion_mode = InsertionMode::InTable;
            return;
        }
        if (name == "template"_s) {
            m_insertion_mode = m_template_insertion_modes.empty()
                ? InsertionMode::InTemplate
                : m_template_insertion_modes.top();
            return;
        }
        if (name == "head"_s) {
            if (!last) {
                m_insertion_mode = InsertionMode::InHead;
                return;
            }
        }
        if (name == "body"_s) {
            m_insertion_mode = InsertionMode::InBody;
            return;
        }
        if (name == "frameset"_s) {
            m_insertion_mode = InsertionMode::InFrameset;
            return;
        }
        if (name == "html"_s) {
            if (!m_head_element) {
                m_insertion_mode = InsertionMode::BeforeHead;
            } else {
                m_insertion_mode = InsertionMode::AfterHead;
            }
            return;
        }
        if (last) {
            m_insertion_mode = InsertionMode::InBody;
            return;
        }
    }
}

} // namespace lithium::html
