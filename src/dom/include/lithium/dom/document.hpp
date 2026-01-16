#pragma once

#include "node.hpp"
#include "element.hpp"

namespace lithium::dom {

// ============================================================================
// DocumentType
// ============================================================================

class DocumentType : public Node {
public:
    DocumentType(const String& name, const String& public_id, const String& system_id);

    [[nodiscard]] NodeType node_type() const override { return NodeType::DocumentType; }
    [[nodiscard]] String node_name() const override { return m_name; }
    [[nodiscard]] RefPtr<Node> clone_node(bool deep) const override;

    [[nodiscard]] const String& name() const { return m_name; }
    [[nodiscard]] const String& public_id() const { return m_public_id; }
    [[nodiscard]] const String& system_id() const { return m_system_id; }

private:
    String m_name;
    String m_public_id;
    String m_system_id;
};

// ============================================================================
// Document - Represents the entire HTML document
// ============================================================================

class Document : public Node {
public:
    Document();

    // Node interface
    [[nodiscard]] NodeType node_type() const override { return NodeType::Document; }
    [[nodiscard]] String node_name() const override { return "#document"_s; }
    [[nodiscard]] RefPtr<Node> clone_node(bool deep) const override;

    // Document-specific
    [[nodiscard]] DocumentType* doctype() const { return m_doctype.get(); }
    [[nodiscard]] Element* document_element() const;
    [[nodiscard]] Element* head() const;
    [[nodiscard]] Element* body() const;

    [[nodiscard]] const String& title() const { return m_title; }
    void set_title(const String& title);

    // URL and encoding
    [[nodiscard]] const String& url() const { return m_url; }
    void set_url(const String& url) { m_url = url; }

    [[nodiscard]] const String& character_set() const { return m_character_set; }
    void set_character_set(const String& charset) { m_character_set = charset; }

    [[nodiscard]] const String& content_type() const { return m_content_type; }
    void set_content_type(const String& type) { m_content_type = type; }

    // Element creation
    [[nodiscard]] RefPtr<Element> create_element(const String& tag_name);
    [[nodiscard]] RefPtr<Element> create_element_ns(const String& namespace_uri, const String& qualified_name);
    [[nodiscard]] RefPtr<Text> create_text_node(const String& data);
    [[nodiscard]] RefPtr<Node> create_comment(const String& data);
    [[nodiscard]] RefPtr<DocumentType> create_document_type(
        const String& name, const String& public_id, const String& system_id);

    // Element lookup
    [[nodiscard]] Element* get_element_by_id(const String& id) const;
    [[nodiscard]] std::vector<Element*> get_elements_by_tag_name(const String& tag_name) const;
    [[nodiscard]] std::vector<Element*> get_elements_by_class_name(const String& class_names) const;

    // Query selectors
    [[nodiscard]] Element* query_selector(const String& selectors) const;
    [[nodiscard]] std::vector<Element*> query_selector_all(const String& selectors) const;

    // Adoption
    RefPtr<Node> adopt_node(RefPtr<Node> node);
    RefPtr<Node> import_node(const Node* node, bool deep = false);

    // Document state
    enum class ReadyState { Loading, Interactive, Complete };
    [[nodiscard]] ReadyState ready_state() const { return m_ready_state; }

    // Quirks mode
    enum class QuirksMode { NoQuirks, Quirks, LimitedQuirks };
    [[nodiscard]] QuirksMode quirks_mode() const { return m_quirks_mode; }
    void set_quirks_mode(QuirksMode mode) { m_quirks_mode = mode; }

private:
    RefPtr<DocumentType> m_doctype;
    String m_title;
    String m_url;
    String m_character_set{"UTF-8"};
    String m_content_type{"text/html"};
    ReadyState m_ready_state{ReadyState::Loading};
    QuirksMode m_quirks_mode{QuirksMode::NoQuirks};
};

} // namespace lithium::dom
