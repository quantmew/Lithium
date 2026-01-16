/**
 * DOM Document implementation
 */

#include "lithium/dom/document.hpp"
#include "lithium/dom/text.hpp"
#include <algorithm>

namespace lithium::dom {

// ============================================================================
// DocumentType
// ============================================================================

DocumentType::DocumentType(const String& name, const String& public_id, const String& system_id)
    : m_name(name)
    , m_public_id(public_id)
    , m_system_id(system_id)
{
}

RefPtr<Node> DocumentType::clone_node(bool /*deep*/) const {
    return make_ref<DocumentType>(m_name, m_public_id, m_system_id);
}

// ============================================================================
// Document
// ============================================================================

Document::Document() {
    set_owner_document(this);
}

RefPtr<Node> Document::clone_node(bool deep) const {
    auto clone = make_ref<Document>();
    clone->m_title = m_title;
    clone->m_url = m_url;
    clone->m_character_set = m_character_set;
    clone->m_content_type = m_content_type;
    clone->m_quirks_mode = m_quirks_mode;

    if (deep) {
        if (m_doctype) {
            auto dt_clone = static_cast<DocumentType*>(m_doctype->clone_node(false).get());
            clone->m_doctype = RefPtr<DocumentType>(dt_clone);
        }

        for (const auto& child : child_nodes()) {
            auto child_clone = child->clone_node(true);
            clone->append_child(child_clone);
        }
    }

    return clone;
}

Element* Document::document_element() const {
    for (const auto& child : child_nodes()) {
        if (child->is_element()) {
            return child->as_element();
        }
    }
    return nullptr;
}

Element* Document::head() const {
    auto* html = document_element();
    if (!html) return nullptr;

    for (const auto& child : html->child_nodes()) {
        if (child->is_element()) {
            auto* elem = child->as_element();
            if (elem->local_name() == "head"_s) {
                return elem;
            }
        }
    }
    return nullptr;
}

Element* Document::body() const {
    auto* html = document_element();
    if (!html) return nullptr;

    for (const auto& child : html->child_nodes()) {
        if (child->is_element()) {
            auto* elem = child->as_element();
            if (elem->local_name() == "body"_s) {
                return elem;
            }
        }
    }
    return nullptr;
}

void Document::set_title(const String& title) {
    m_title = title;

    // Also update the <title> element if it exists
    auto* head_elem = head();
    if (head_elem) {
        for (const auto& child : head_elem->child_nodes()) {
            if (child->is_element()) {
                auto* elem = child->as_element();
                if (elem->local_name() == "title"_s) {
                    elem->set_text_content(title);
                    return;
                }
            }
        }

        // Create title element if not found
        auto title_elem = create_element("title"_s);
        title_elem->set_text_content(title);
        head_elem->append_child(title_elem);
    }
}

RefPtr<Element> Document::create_element(const String& tag_name) {
    auto elem = make_ref<HTMLElement>(tag_name);
    elem->set_owner_document(this);
    return elem;
}

RefPtr<Element> Document::create_element_ns(const String& namespace_uri, const String& qualified_name) {
    auto elem = make_ref<Element>(namespace_uri, qualified_name);
    elem->set_owner_document(this);
    return elem;
}

RefPtr<Text> Document::create_text_node(const String& data) {
    auto text = make_ref<Text>(data);
    text->set_owner_document(this);
    return text;
}

RefPtr<Node> Document::create_comment(const String& data) {
    auto comment = make_ref<Comment>(data);
    comment->set_owner_document(this);
    return comment;
}

RefPtr<DocumentType> Document::create_document_type(
    const String& name, const String& public_id, const String& system_id)
{
    auto doctype = make_ref<DocumentType>(name, public_id, system_id);
    doctype->set_owner_document(this);
    return doctype;
}

Element* Document::get_element_by_id(const String& id) const {
    std::function<Element*(const Node*)> find = [&](const Node* node) -> Element* {
        for (const auto& child : node->child_nodes()) {
            if (child->is_element()) {
                auto* elem = child->as_element();
                if (elem->id() == id) {
                    return elem;
                }
                if (auto* found = find(elem)) {
                    return found;
                }
            }
        }
        return nullptr;
    };

    return find(this);
}

std::vector<Element*> Document::get_elements_by_tag_name(const String& tag_name) const {
    std::vector<Element*> result;
    auto lower_tag = tag_name.to_lowercase();
    bool match_all = (tag_name == "*"_s);

    std::function<void(const Node*)> collect = [&](const Node* node) {
        for (const auto& child : node->child_nodes()) {
            if (child->is_element()) {
                auto* elem = child->as_element();
                if (match_all || elem->local_name() == lower_tag) {
                    result.push_back(elem);
                }
                collect(elem);
            }
        }
    };

    collect(this);
    return result;
}

std::vector<Element*> Document::get_elements_by_class_name(const String& class_names) const {
    auto* root = document_element();
    if (!root) return {};
    return root->get_elements_by_class_name(class_names);
}

Element* Document::query_selector(const String& selectors) const {
    auto* root = document_element();
    if (!root) return nullptr;
    return root->query_selector(selectors);
}

std::vector<Element*> Document::query_selector_all(const String& selectors) const {
    auto* root = document_element();
    if (!root) return {};
    return root->query_selector_all(selectors);
}

RefPtr<Node> Document::adopt_node(RefPtr<Node> node) {
    if (!node) return nullptr;

    // Remove from previous document
    if (node->parent_node()) {
        node->parent_node()->remove_child(node);
    }

    // Set new owner
    std::function<void(Node*)> set_owner = [this, &set_owner](Node* n) {
        n->set_owner_document(this);
        for (const auto& child : n->child_nodes()) {
            set_owner(child.get());
        }
    };

    set_owner(node.get());
    return node;
}

RefPtr<Node> Document::import_node(const Node* node, bool deep) {
    if (!node) return nullptr;

    auto clone = node->clone_node(deep);
    return adopt_node(clone);
}

} // namespace lithium::dom
