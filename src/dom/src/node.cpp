/**
 * DOM Node implementation
 */

#include "lithium/dom/node.hpp"
#include "lithium/dom/element.hpp"
#include "lithium/dom/document.hpp"
#include "lithium/dom/text.hpp"
#include <algorithm>
#include <array>

namespace lithium::dom {

namespace {

bool is_form_associated_element(const Element* element) {
    if (!element) return false;
    static const std::array<const char*, 12> FORM_ASSOCIATED = {
        "button", "fieldset", "input", "label", "object", "output",
        "select", "textarea", "option", "optgroup", "meter", "progress"
    };
    auto local = element->local_name();
    for (const auto* name : FORM_ASSOCIATED) {
        if (local == String(name)) {
            return true;
        }
    }
    return false;
}

Element* resolve_form_owner(Element* element) {
    if (!element || !is_form_associated_element(element)) {
        return nullptr;
    }

    if (element->local_name() == "option"_s || element->local_name() == "optgroup"_s) {
        Node* ancestor = element->parent_node();
        while (ancestor) {
            if (ancestor->is_element()) {
                auto* ancestor_el = ancestor->as_element();
                if (ancestor_el->local_name() == "select"_s) {
                    if (ancestor_el->form_owner()) {
                        return ancestor_el->form_owner();
                    }
                    break;
                }
            }
            ancestor = ancestor->parent_node();
        }
    }

    if (auto attr = element->get_attribute("form"_s)) {
        auto* doc = element->owner_document();
        auto* target = doc ? doc->get_element_by_id(*attr) : nullptr;
        if (target && target->local_name() == "form"_s) {
            return target;
        }
        return nullptr;
    }

    Node* ancestor = element->parent_node();
    while (ancestor) {
        if (ancestor->is_element() && ancestor->as_element()->local_name() == "form"_s) {
            return ancestor->as_element();
        }
        ancestor = ancestor->parent_node();
    }

    return nullptr;
}

void refresh_form_owners(Node* node) {
    if (!node) return;
    if (node->is_element()) {
        auto* element = node->as_element();
        if (is_form_associated_element(element)) {
            element->set_form_owner(resolve_form_owner(element));
        }
    }
    for (const auto& child : node->child_nodes()) {
        refresh_form_owners(child.get());
    }
}

} // namespace

RefPtr<Node> Node::append_child(RefPtr<Node> child) {
    if (!child) return nullptr;

    // Remove from previous parent
    if (child->m_parent) {
        child->m_parent->remove_child(child);
    }

    // Set relationships
    child->m_parent = this;
    child->set_owner_document(m_owner_document);

    if (m_last_child) {
        m_last_child->m_next_sibling = child;
        child->m_previous_sibling = m_last_child;
    } else {
        m_first_child = child;
    }

    m_last_child = child.get();
    m_children.push_back(child);

    refresh_form_owners(child.get());

    return child;
}

RefPtr<Node> Node::insert_before(RefPtr<Node> node, Node* reference) {
    if (!node) return nullptr;

    if (!reference) {
        return append_child(node);
    }

    // Check if reference is a child
    auto it = std::find_if(m_children.begin(), m_children.end(),
        [reference](const RefPtr<Node>& n) { return n.get() == reference; });

    if (it == m_children.end()) {
        return nullptr;
    }

    // Remove from previous parent
    if (node->m_parent) {
        node->m_parent->remove_child(node);
    }

    // Set relationships
    node->m_parent = this;
    node->set_owner_document(m_owner_document);

    node->m_next_sibling = RefPtr<Node>(reference);
    node->m_previous_sibling = reference->m_previous_sibling;

    if (reference->m_previous_sibling) {
        reference->m_previous_sibling->m_next_sibling = node;
    } else {
        m_first_child = node;
    }

    reference->m_previous_sibling = node.get();

    m_children.insert(it, node);

    refresh_form_owners(node.get());

    return node;
}

RefPtr<Node> Node::remove_child(RefPtr<Node> child) {
    if (!child || child->m_parent != this) {
        return nullptr;
    }

    // Update sibling links
    if (child->m_previous_sibling) {
        child->m_previous_sibling->m_next_sibling = child->m_next_sibling;
    } else {
        m_first_child = child->m_next_sibling;
    }

    if (child->m_next_sibling) {
        child->m_next_sibling->m_previous_sibling = child->m_previous_sibling;
    } else {
        m_last_child = child->m_previous_sibling;
    }

    child->m_parent = nullptr;
    child->m_previous_sibling = nullptr;
    child->m_next_sibling = nullptr;

    // Remove from children vector
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }

    refresh_form_owners(child.get());

    return child;
}

RefPtr<Node> Node::replace_child(RefPtr<Node> new_child, RefPtr<Node> old_child) {
    if (!new_child || !old_child || old_child->m_parent != this) {
        return nullptr;
    }

    insert_before(new_child, old_child.get());
    return remove_child(old_child);
}

String Node::text_content() const {
    String result;

    for (const auto& child : m_children) {
        if (child->is_text()) {
            result = result + child->text_content();
        } else if (child->is_element()) {
            result = result + child->text_content();
        }
    }

    return result;
}

void Node::set_text_content(const String& text) {
    // Remove all children
    while (m_first_child) {
        remove_child(m_first_child);
    }

    // Add text node if non-empty
    if (!text.empty() && m_owner_document) {
        auto text_node = m_owner_document->create_text_node(text);
        append_child(text_node);
    }
}

bool Node::contains(const Node* other) const {
    if (!other) return false;

    const Node* current = other;
    while (current) {
        if (current == this) return true;
        current = current->m_parent;
    }

    return false;
}

Element* Node::as_element() {
    return is_element() ? static_cast<Element*>(this) : nullptr;
}

const Element* Node::as_element() const {
    return is_element() ? static_cast<const Element*>(this) : nullptr;
}

Text* Node::as_text() {
    return is_text() ? static_cast<Text*>(this) : nullptr;
}

const Text* Node::as_text() const {
    return is_text() ? static_cast<const Text*>(this) : nullptr;
}

Document* Node::as_document() {
    return is_document() ? static_cast<Document*>(this) : nullptr;
}

const Document* Node::as_document() const {
    return is_document() ? static_cast<const Document*>(this) : nullptr;
}

} // namespace lithium::dom
