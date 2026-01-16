/**
 * DOM Text and CharacterData implementation
 */

#include "lithium/dom/text.hpp"
#include "lithium/dom/document.hpp"

namespace lithium::dom {

// ============================================================================
// CharacterData
// ============================================================================

void CharacterData::append_data(const String& data) {
    m_data = m_data + data;
}

void CharacterData::insert_data(usize offset, const String& data) {
    if (offset > m_data.length()) {
        offset = m_data.length();
    }

    m_data = m_data.substring(0, offset) + data + m_data.substring(offset);
}

void CharacterData::delete_data(usize offset, usize count) {
    if (offset >= m_data.length()) {
        return;
    }

    usize end = offset + count;
    if (end > m_data.length()) {
        end = m_data.length();
    }

    m_data = m_data.substring(0, offset) + m_data.substring(end);
}

void CharacterData::replace_data(usize offset, usize count, const String& data) {
    delete_data(offset, count);
    insert_data(offset, data);
}

String CharacterData::substring_data(usize offset, usize count) const {
    if (offset >= m_data.length()) {
        return String();
    }

    return m_data.substring(offset, count);
}

// ============================================================================
// Text
// ============================================================================

Text::Text(const String& data)
    : CharacterData(data)
{
}

RefPtr<Node> Text::clone_node(bool /*deep*/) const {
    return make_ref<Text>(data());
}

String Text::whole_text() const {
    String result;

    // Find the first text node in the sequence
    const Node* first = this;
    while (first->previous_sibling() && first->previous_sibling()->is_text()) {
        first = first->previous_sibling();
    }

    // Concatenate all contiguous text nodes
    const Node* current = first;
    while (current && current->is_text()) {
        result = result + current->as_text()->data();
        current = current->next_sibling();
    }

    return result;
}

RefPtr<Text> Text::split_text(usize offset) {
    if (offset > length()) {
        return nullptr;
    }

    String new_data = substring_data(offset, length() - offset);
    delete_data(offset, length() - offset);

    auto new_text = make_ref<Text>(new_data);
    new_text->set_owner_document(owner_document());

    // Insert after this node
    if (parent_node()) {
        parent_node()->insert_before(new_text, next_sibling());
    }

    return new_text;
}

// ============================================================================
// Comment
// ============================================================================

Comment::Comment(const String& data)
    : CharacterData(data)
{
}

RefPtr<Node> Comment::clone_node(bool /*deep*/) const {
    return make_ref<Comment>(data());
}

} // namespace lithium::dom
