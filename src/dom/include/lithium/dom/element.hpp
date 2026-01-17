#pragma once

#include "node.hpp"
#include <unordered_map>
#include <optional>

namespace lithium::dom {

class DocumentFragment;

// ============================================================================
// Attribute
// ============================================================================

struct Attribute {
    String name;
    String value;
    String namespace_uri;
    String prefix;
    String local_name;
};

// ============================================================================
// Element - Represents an HTML/XML element
// ============================================================================

class Element : public Node {
public:
    explicit Element(const String& tag_name);
    Element(const String& namespace_uri, const String& qualified_name);

    // Node interface
    [[nodiscard]] NodeType node_type() const override { return NodeType::Element; }
    [[nodiscard]] String node_name() const override { return m_tag_name.to_uppercase(); }
    [[nodiscard]] RefPtr<Node> clone_node(bool deep) const override;

    // Element-specific
    [[nodiscard]] const String& tag_name() const { return m_tag_name; }
    [[nodiscard]] const String& local_name() const { return m_local_name; }
    [[nodiscard]] const String& namespace_uri() const { return m_namespace_uri; }
    [[nodiscard]] const String& prefix() const { return m_prefix; }
    [[nodiscard]] Element* form_owner() const { return m_form_owner; }
    void set_form_owner(Element* form) { m_form_owner = form; }

    // ID and class
    [[nodiscard]] String id() const { return get_attribute("id"_s).value_or(String()); }
    void set_id(const String& id) { set_attribute("id"_s, id); }

    [[nodiscard]] String class_name() const { return get_attribute("class"_s).value_or(String()); }
    void set_class_name(const String& class_name) { set_attribute("class"_s, class_name); }

    [[nodiscard]] std::vector<String> class_list() const;

    // Attributes
    [[nodiscard]] bool has_attribute(const String& name) const;
    [[nodiscard]] bool has_attribute_ns(const String& namespace_uri, const String& local_name) const;

    [[nodiscard]] std::optional<String> get_attribute(const String& name) const;
    [[nodiscard]] std::optional<String> get_attribute_ns(const String& namespace_uri, const String& local_name) const;

    void set_attribute(const String& name, const String& value);
    void set_attribute_ns(const String& namespace_uri, const String& qualified_name, const String& value);

    void remove_attribute(const String& name);
    void remove_attribute_ns(const String& namespace_uri, const String& local_name);

    [[nodiscard]] const std::vector<Attribute>& attributes() const { return m_attributes; }
    [[nodiscard]] bool has_attributes() const { return !m_attributes.empty(); }

    // Element traversal
    [[nodiscard]] Element* first_element_child() const;
    [[nodiscard]] Element* last_element_child() const;
    [[nodiscard]] Element* previous_element_sibling() const;
    [[nodiscard]] Element* next_element_sibling() const;
    [[nodiscard]] u32 child_element_count() const;

    // Query methods
    [[nodiscard]] Element* query_selector(const String& selectors) const;
    [[nodiscard]] std::vector<Element*> query_selector_all(const String& selectors) const;
    [[nodiscard]] std::vector<Element*> get_elements_by_tag_name(const String& tag_name) const;
    [[nodiscard]] std::vector<Element*> get_elements_by_class_name(const String& class_names) const;

    // Inner/Outer HTML
    [[nodiscard]] String inner_html() const;
    void set_inner_html(const String& html);

    [[nodiscard]] String outer_html() const;

    // Matching
    [[nodiscard]] bool matches(const String& selectors) const;

private:
    String m_tag_name;
    String m_local_name;
    String m_namespace_uri;
    String m_prefix;
    std::vector<Attribute> m_attributes;
    Element* m_form_owner{nullptr};
};

// ============================================================================
// HTMLElement - Base for all HTML elements
// ============================================================================

class HTMLElement : public Element {
public:
    explicit HTMLElement(const String& tag_name);

    // Global attributes
    [[nodiscard]] String title() const { return get_attribute("title"_s).value_or(String()); }
    void set_title(const String& title) { set_attribute("title"_s, title); }

    [[nodiscard]] String lang() const { return get_attribute("lang"_s).value_or(String()); }
    void set_lang(const String& lang) { set_attribute("lang"_s, lang); }

    [[nodiscard]] bool hidden() const { return has_attribute("hidden"_s); }
    void set_hidden(bool hidden);

    // Style
    [[nodiscard]] String style() const { return get_attribute("style"_s).value_or(String()); }
    void set_style(const String& style) { set_attribute("style"_s, style); }

    // Data attributes
    [[nodiscard]] std::optional<String> get_data_attribute(const String& name) const;
    void set_data_attribute(const String& name, const String& value);
};

// ============================================================================
// HTML integration hooks
// ============================================================================

using HTMLFragmentParser = RefPtr<DocumentFragment> (*)(const String& html, Element* context);
void register_html_fragment_parser(HTMLFragmentParser parser);

} // namespace lithium::dom
