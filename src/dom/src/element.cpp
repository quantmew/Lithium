/**
 * DOM Element implementation
 */

#include "lithium/dom/element.hpp"
#include "lithium/dom/document.hpp"
#include "lithium/dom/text.hpp"
#include <algorithm>

namespace lithium::dom {

namespace {

HTMLFragmentParser g_fragment_parser{nullptr};

} // namespace

void register_html_fragment_parser(HTMLFragmentParser parser) {
    g_fragment_parser = parser;
}

// ============================================================================
// Element
// ============================================================================

Element::Element(const String& tag_name)
    : m_tag_name(tag_name)
    , m_local_name(tag_name.to_lowercase())
{
}

Element::Element(const String& namespace_uri, const String& qualified_name)
    : m_namespace_uri(namespace_uri)
{
    auto colon_pos = qualified_name.find(':');
    if (colon_pos.has_value()) {
        m_prefix = qualified_name.substring(0, colon_pos.value());
        m_local_name = qualified_name.substring(colon_pos.value() + 1);
    } else {
        m_local_name = qualified_name;
    }
    m_tag_name = qualified_name;
}

RefPtr<Node> Element::clone_node(bool deep) const {
    auto clone = make_ref<Element>(m_tag_name);
    clone->m_local_name = m_local_name;
    clone->m_namespace_uri = m_namespace_uri;
    clone->m_prefix = m_prefix;
    clone->m_attributes = m_attributes;
    clone->m_form_owner = m_form_owner;

    if (deep) {
        for (const auto& child : child_nodes()) {
            auto child_clone = child->clone_node(true);
            clone->append_child(child_clone);
        }
    }

    return clone;
}

std::vector<String> Element::class_list() const {
    auto class_attr = get_attribute("class"_s);
    if (!class_attr) {
        return {};
    }

    std::vector<String> classes;
    auto class_str = class_attr.value();

    usize start = 0;
    for (usize i = 0; i <= class_str.length(); ++i) {
        if (i == class_str.length() || class_str[i] == ' ' || class_str[i] == '\t') {
            if (i > start) {
                classes.push_back(class_str.substring(start, i - start));
            }
            start = i + 1;
        }
    }

    return classes;
}

bool Element::has_attribute(const String& name) const {
    auto lower_name = name.to_lowercase();
    return std::any_of(m_attributes.begin(), m_attributes.end(),
        [&lower_name](const Attribute& attr) {
            return attr.name.to_lowercase() == lower_name;
        });
}

bool Element::has_attribute_ns(const String& namespace_uri, const String& local_name) const {
    return std::any_of(m_attributes.begin(), m_attributes.end(),
        [&namespace_uri, &local_name](const Attribute& attr) {
            return attr.namespace_uri == namespace_uri && attr.local_name == local_name;
        });
}

std::optional<String> Element::get_attribute(const String& name) const {
    auto lower_name = name.to_lowercase();
    for (const auto& attr : m_attributes) {
        if (attr.name.to_lowercase() == lower_name) {
            return attr.value;
        }
    }
    return std::nullopt;
}

std::optional<String> Element::get_attribute_ns(const String& namespace_uri, const String& local_name) const {
    for (const auto& attr : m_attributes) {
        if (attr.namespace_uri == namespace_uri && attr.local_name == local_name) {
            return attr.value;
        }
    }
    return std::nullopt;
}

void Element::set_attribute(const String& name, const String& value) {
    auto lower_name = name.to_lowercase();

    for (auto& attr : m_attributes) {
        if (attr.name.to_lowercase() == lower_name) {
            attr.value = value;
            return;
        }
    }

    Attribute attr;
    attr.name = name;
    attr.value = value;
    attr.local_name = lower_name;
    m_attributes.push_back(attr);
}

void Element::set_attribute_ns(const String& namespace_uri, const String& qualified_name, const String& value) {
    String prefix;
    String local_name;

    auto colon_pos = qualified_name.find(':');
    if (colon_pos.has_value()) {
        prefix = qualified_name.substring(0, colon_pos.value());
        local_name = qualified_name.substring(colon_pos.value() + 1);
    } else {
        local_name = qualified_name;
    }

    for (auto& attr : m_attributes) {
        if (attr.namespace_uri == namespace_uri && attr.local_name == local_name) {
            attr.value = value;
            attr.prefix = prefix;
            return;
        }
    }

    Attribute attr;
    attr.name = qualified_name;
    attr.value = value;
    attr.namespace_uri = namespace_uri;
    attr.prefix = prefix;
    attr.local_name = local_name;
    m_attributes.push_back(attr);
}

void Element::remove_attribute(const String& name) {
    auto lower_name = name.to_lowercase();
    m_attributes.erase(
        std::remove_if(m_attributes.begin(), m_attributes.end(),
            [&lower_name](const Attribute& attr) {
                return attr.name.to_lowercase() == lower_name;
            }),
        m_attributes.end());
}

void Element::remove_attribute_ns(const String& namespace_uri, const String& local_name) {
    m_attributes.erase(
        std::remove_if(m_attributes.begin(), m_attributes.end(),
            [&namespace_uri, &local_name](const Attribute& attr) {
                return attr.namespace_uri == namespace_uri && attr.local_name == local_name;
            }),
        m_attributes.end());
}

Element* Element::first_element_child() const {
    for (const auto& child : child_nodes()) {
        if (child->is_element()) {
            return child->as_element();
        }
    }
    return nullptr;
}

Element* Element::last_element_child() const {
    const auto& children = child_nodes();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if ((*it)->is_element()) {
            return (*it)->as_element();
        }
    }
    return nullptr;
}

Element* Element::previous_element_sibling() const {
    Node* sibling = previous_sibling();
    while (sibling) {
        if (sibling->is_element()) {
            return sibling->as_element();
        }
        sibling = sibling->previous_sibling();
    }
    return nullptr;
}

Element* Element::next_element_sibling() const {
    Node* sibling = next_sibling();
    while (sibling) {
        if (sibling->is_element()) {
            return sibling->as_element();
        }
        sibling = sibling->next_sibling();
    }
    return nullptr;
}

u32 Element::child_element_count() const {
    u32 count = 0;
    for (const auto& child : child_nodes()) {
        if (child->is_element()) {
            ++count;
        }
    }
    return count;
}

Element* Element::query_selector(const String& /*selectors*/) const {
    // TODO: Implement CSS selector matching
    return nullptr;
}

std::vector<Element*> Element::query_selector_all(const String& /*selectors*/) const {
    // TODO: Implement CSS selector matching
    return {};
}

std::vector<Element*> Element::get_elements_by_tag_name(const String& tag_name) const {
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

std::vector<Element*> Element::get_elements_by_class_name(const String& class_names) const {
    std::vector<Element*> result;

    // Parse class names to search for
    std::vector<String> search_classes;
    usize start = 0;
    for (usize i = 0; i <= class_names.length(); ++i) {
        if (i == class_names.length() || class_names[i] == ' ') {
            if (i > start) {
                search_classes.push_back(class_names.substring(start, i - start));
            }
            start = i + 1;
        }
    }

    if (search_classes.empty()) {
        return result;
    }

    std::function<void(const Node*)> collect = [&](const Node* node) {
        for (const auto& child : node->child_nodes()) {
            if (child->is_element()) {
                auto* elem = child->as_element();
                auto elem_classes = elem->class_list();

                bool has_all = std::all_of(search_classes.begin(), search_classes.end(),
                    [&elem_classes](const String& cls) {
                        return std::find(elem_classes.begin(), elem_classes.end(), cls) != elem_classes.end();
                    });

                if (has_all) {
                    result.push_back(elem);
                }
                collect(elem);
            }
        }
    };

    collect(this);
    return result;
}

String Element::inner_html() const {
    String result;

    for (const auto& child : child_nodes()) {
        if (child->is_text()) {
            result = result + child->text_content();
        } else if (child->is_element()) {
            result = result + child->as_element()->outer_html();
        } else if (child->node_type() == NodeType::Comment) {
            result = result + "<!--"_s + child->text_content() + "-->"_s;
        }
    }

    return result;
}

void Element::set_inner_html(const String& html) {
    // Remove existing children first
    while (first_child()) {
        remove_child(RefPtr<Node>(first_child()));
    }

    auto* doc = owner_document();
    if (!g_fragment_parser || !doc) {
        if (doc && !html.empty()) {
            append_child(doc->create_text_node(html));
        }
        return;
    }

    auto fragment = g_fragment_parser(html, this);
    if (!fragment) {
        return;
    }

    while (fragment->first_child()) {
        auto child = RefPtr<Node>(fragment->first_child());
        fragment->remove_child(child);

        auto adopted = doc->adopt_node(child);
        if (adopted) {
            append_child(adopted);
        }
    }
}

String Element::outer_html() const {
    String result = "<"_s + m_tag_name;

    for (const auto& attr : m_attributes) {
        result = result + " "_s + attr.name + "=\""_s + attr.value + "\""_s;
    }

    // Check for void elements
    static const char* void_elements[] = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr"
    };

    auto lower_tag = m_tag_name.to_lowercase();
    for (const char* ve : void_elements) {
        if (lower_tag == String(ve)) {
            return result + ">"_s;
        }
    }

    result = result + ">"_s + inner_html() + "</"_s + m_tag_name + ">"_s;
    return result;
}

bool Element::matches(const String& /*selectors*/) const {
    // TODO: Implement CSS selector matching
    return false;
}

// ============================================================================
// HTMLElement
// ============================================================================

HTMLElement::HTMLElement(const String& tag_name)
    : Element(tag_name)
{
}

void HTMLElement::set_hidden(bool hidden) {
    if (hidden) {
        set_attribute("hidden"_s, ""_s);
    } else {
        remove_attribute("hidden"_s);
    }
}

std::optional<String> HTMLElement::get_data_attribute(const String& name) const {
    return get_attribute("data-"_s + name);
}

void HTMLElement::set_data_attribute(const String& name, const String& value) {
    set_attribute("data-"_s + name, value);
}

} // namespace lithium::dom
