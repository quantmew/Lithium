#pragma once

#include "node.hpp"

namespace lithium::dom {

// ============================================================================
// CharacterData - Base class for text-based nodes
// ============================================================================

class CharacterData : public Node {
public:
    [[nodiscard]] String node_value() const override { return m_data; }
    [[nodiscard]] String text_content() const override { return m_data; }
    void set_text_content(const String& text) override { m_data = text; }

    // CharacterData interface
    [[nodiscard]] const String& data() const { return m_data; }
    void set_data(const String& data) { m_data = data; }

    [[nodiscard]] usize length() const { return m_data.length(); }

    void append_data(const String& data);
    void insert_data(usize offset, const String& data);
    void delete_data(usize offset, usize count);
    void replace_data(usize offset, usize count, const String& data);

    [[nodiscard]] String substring_data(usize offset, usize count) const;

protected:
    CharacterData() = default;
    explicit CharacterData(const String& data) : m_data(data) {}

private:
    String m_data;
};

// ============================================================================
// Text - Represents a text node
// ============================================================================

class Text : public CharacterData {
public:
    Text() = default;
    explicit Text(const String& data);

    // Node interface
    [[nodiscard]] NodeType node_type() const override { return NodeType::Text; }
    [[nodiscard]] String node_name() const override { return "#text"_s; }
    [[nodiscard]] RefPtr<Node> clone_node(bool deep) const override;

    // Text-specific
    [[nodiscard]] String whole_text() const;
    [[nodiscard]] RefPtr<Text> split_text(usize offset);
};

// ============================================================================
// Comment - Represents a comment node
// ============================================================================

class Comment : public CharacterData {
public:
    Comment() = default;
    explicit Comment(const String& data);

    // Node interface
    [[nodiscard]] NodeType node_type() const override { return NodeType::Comment; }
    [[nodiscard]] String node_name() const override { return "#comment"_s; }
    [[nodiscard]] RefPtr<Node> clone_node(bool deep) const override;
};

} // namespace lithium::dom
