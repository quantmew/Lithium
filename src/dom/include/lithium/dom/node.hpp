#pragma once

#include "lithium/core/types.hpp"
#include "lithium/core/string.hpp"
#include <vector>
#include <memory>

namespace lithium::dom {

// Forward declarations
class Document;
class Element;
class Text;

// ============================================================================
// Node types (as per DOM spec)
// ============================================================================

enum class NodeType : u16 {
    Element = 1,
    Attribute = 2,
    Text = 3,
    CDataSection = 4,
    ProcessingInstruction = 7,
    Comment = 8,
    Document = 9,
    DocumentType = 10,
    DocumentFragment = 11,
};

// ============================================================================
// Node - Base class for all DOM nodes
// ============================================================================

class Node : public RefCounted {
public:
    virtual ~Node() = default;

    // Type information
    [[nodiscard]] virtual NodeType node_type() const = 0;
    [[nodiscard]] virtual String node_name() const = 0;
    [[nodiscard]] virtual String node_value() const { return String(); }

    // Tree structure
    [[nodiscard]] Node* parent_node() const { return m_parent; }
    [[nodiscard]] Node* first_child() const { return m_first_child.get(); }
    [[nodiscard]] Node* last_child() const { return m_last_child; }
    [[nodiscard]] Node* previous_sibling() const { return m_previous_sibling; }
    [[nodiscard]] Node* next_sibling() const { return m_next_sibling.get(); }

    [[nodiscard]] bool has_children() const { return m_first_child != nullptr; }
    [[nodiscard]] const std::vector<RefPtr<Node>>& child_nodes() const { return m_children; }

    // Document access
    [[nodiscard]] Document* owner_document() const { return m_owner_document; }

    // Tree manipulation
    RefPtr<Node> append_child(RefPtr<Node> child);
    RefPtr<Node> insert_before(RefPtr<Node> node, Node* reference);
    RefPtr<Node> remove_child(RefPtr<Node> child);
    RefPtr<Node> replace_child(RefPtr<Node> new_child, RefPtr<Node> old_child);

    // Cloning
    [[nodiscard]] virtual RefPtr<Node> clone_node(bool deep = false) const = 0;

    // Text content
    [[nodiscard]] virtual String text_content() const;
    virtual void set_text_content(const String& text);

    // Comparison
    [[nodiscard]] bool is_same_node(const Node* other) const { return this == other; }
    [[nodiscard]] bool contains(const Node* other) const;

    // Type checking helpers
    [[nodiscard]] bool is_element() const { return node_type() == NodeType::Element; }
    [[nodiscard]] bool is_text() const { return node_type() == NodeType::Text; }
    [[nodiscard]] bool is_document() const { return node_type() == NodeType::Document; }

    // Cast helpers
    [[nodiscard]] Element* as_element();
    [[nodiscard]] const Element* as_element() const;
    [[nodiscard]] Text* as_text();
    [[nodiscard]] const Text* as_text() const;
    [[nodiscard]] Document* as_document();
    [[nodiscard]] const Document* as_document() const;

protected:
    Node() = default;

    void set_owner_document(Document* doc) { m_owner_document = doc; }
    void set_parent(Node* parent) { m_parent = parent; }

private:
    void update_child_list();

    Document* m_owner_document{nullptr};
    Node* m_parent{nullptr};
    RefPtr<Node> m_first_child;
    Node* m_last_child{nullptr};
    Node* m_previous_sibling{nullptr};
    RefPtr<Node> m_next_sibling;
    std::vector<RefPtr<Node>> m_children;

    friend class Document;
    friend class Element;
};

} // namespace lithium::dom
