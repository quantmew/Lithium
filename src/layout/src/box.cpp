/**
 * Layout Box implementation
 */

#include "lithium/layout/box.hpp"
#include "lithium/dom/element.hpp"
#include "lithium/dom/text.hpp"
#include "lithium/dom/document.hpp"
#include "lithium/css/selector.hpp"
#include <sstream>
#include <iostream>

namespace lithium::layout {

// ============================================================================
// LayoutBox implementation
// ============================================================================

LayoutBox::LayoutBox(BoxType type) : m_box_type(type) {}

LayoutBox::LayoutBox(BoxType type, dom::Node* node)
    : m_box_type(type)
    , m_node(node)
{}

void LayoutBox::add_child(std::unique_ptr<LayoutBox> child) {
    child->m_parent = this;
    m_children.push_back(std::move(child));
}

void LayoutBox::insert_child(usize index, std::unique_ptr<LayoutBox> child) {
    child->m_parent = this;
    if (index >= m_children.size()) {
        m_children.push_back(std::move(child));
    } else {
        m_children.insert(m_children.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
    }
}

std::unique_ptr<LayoutBox> LayoutBox::remove_child(usize index) {
    if (index >= m_children.size()) {
        return nullptr;
    }
    auto child = std::move(m_children[index]);
    child->m_parent = nullptr;
    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));
    return child;
}

LayoutBox* LayoutBox::get_inline_container() {
    // If this is an inline box, return it
    if (is_inline()) {
        return this;
    }

    // If this is a block box, check if last child is anonymous inline container
    if (!m_children.empty()) {
        auto* last = m_children.back().get();
        if (last->is_anonymous() && last->is_inline()) {
            return last;
        }
    }

    // Create new anonymous inline container
    auto container = std::make_unique<LayoutBox>(BoxType::Anonymous);
    auto* ptr = container.get();
    add_child(std::move(container));
    return ptr;
}

String LayoutBox::debug_string(i32 indent) const {
    std::ostringstream oss;
    std::string pad(indent * 2, ' ');

    oss << pad;

    switch (m_box_type) {
        case BoxType::Block: oss << "Block"; break;
        case BoxType::Inline: oss << "Inline"; break;
        case BoxType::InlineBlock: oss << "InlineBlock"; break;
        case BoxType::Anonymous: oss << "Anonymous"; break;
        case BoxType::Text: oss << "Text"; break;
    }

    if (m_node) {
        if (auto* element = dynamic_cast<dom::Element*>(m_node)) {
            oss << " <" << element->tag_name().data() << ">";
        }
    }

    if (is_text()) {
        oss << " \"" << m_text.data() << "\"";
    }

    oss << " [";
    oss << "x=" << m_dimensions.content.x << ", ";
    oss << "y=" << m_dimensions.content.y << ", ";
    oss << "w=" << m_dimensions.content.width << ", ";
    oss << "h=" << m_dimensions.content.height;
    oss << "]\n";

    for (const auto& child : m_children) {
        auto child_str = child->debug_string(indent + 1);
        oss << child_str.c_str();
    }

    return String(oss.str());
}

// ============================================================================
// LayoutTreeBuilder implementation
// ============================================================================

std::unique_ptr<LayoutBox> LayoutTreeBuilder::build(
    dom::Document& document,
    const css::StyleResolver& resolver) {

    // Create root block box
    auto root = std::make_unique<LayoutBox>(BoxType::Block);

    // Build from document element
    if (auto* doc_element = document.document_element()) {
        auto element_box = build_element_box(*doc_element, resolver);
        if (element_box) {
            root->add_child(std::move(element_box));
        }
    }

    return root;
}

std::unique_ptr<LayoutBox> LayoutTreeBuilder::build_box(
    dom::Node& node,
    const css::StyleResolver& resolver) {

    switch (node.node_type()) {
        case dom::NodeType::Element:
            return build_element_box(static_cast<dom::Element&>(node), resolver);
        case dom::NodeType::Text:
            {
                // Get parent style
                const css::ComputedValue* parent_style = nullptr;
                if (node.parent_node() && node.parent_node()->node_type() == dom::NodeType::Element) {
                    parent_style = resolver.get_computed_style(
                        *static_cast<dom::Element*>(node.parent_node()));
                }
                // Use default empty style if no parent
                css::ComputedValue default_style{};
                if (!parent_style) {
                    parent_style = &default_style;
                }
                return build_text_box(
                    static_cast<dom::Text&>(node),
                    *parent_style);
            }
        default:
            return nullptr;
    }
}

std::unique_ptr<LayoutBox> LayoutTreeBuilder::build_element_box(
    dom::Element& element,
    const css::StyleResolver& resolver) {

    // Use resolve() to compute (or recompute after cache invalidation)
    css::ComputedValue computed = resolver.resolve(element);

    // Check display property
    if (computed.display == css::Display::None) {
        std::cout << "build_element_box: element '" << element.tag_name().c_str() << "' has display:none" << std::endl;
        return nullptr;
    }

    BoxType type = determine_box_type(computed);
    auto box = std::make_unique<LayoutBox>(type, &element);
    box->set_style(computed);

    std::cout << "build_element_box: created box for '" << element.tag_name().c_str()
              << "' type=" << (type == BoxType::Block ? "Block" : "Inline") << std::endl;

    // Build children
    for (auto* child = element.first_child(); child; child = child->next_sibling()) {
        auto child_box = build_box(*child, resolver);
        if (child_box) {
            // Handle inline/block mixing
            if (type == BoxType::Block && child_box->is_inline()) {
                // Add to inline container
                auto* container = box->get_inline_container();
                container->add_child(std::move(child_box));
            } else {
                box->add_child(std::move(child_box));
            }
        }
    }

    std::cout << "build_element_box: finished building '" << element.tag_name().c_str()
              << "' layout_box children=" << box->children().size() << std::endl;

    return box;
}

std::unique_ptr<LayoutBox> LayoutTreeBuilder::build_text_box(
    dom::Text& text,
    const css::ComputedValue& parent_style) {

    String content = text.data();

    // Skip whitespace-only text in block context
    // (simplified - real implementation needs more complex whitespace handling)
    bool all_whitespace = true;
    for (usize i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            all_whitespace = false;
            break;
        }
    }

    if (all_whitespace && content.size() > 1) {
        return nullptr;
    }

    auto box = std::make_unique<LayoutBox>(BoxType::Text, &text);
    box->set_text(content);
    box->set_style(parent_style);
    return box;
}

BoxType LayoutTreeBuilder::determine_box_type(const css::ComputedValue& style) {
    switch (style.display) {
        case css::Display::Block:
            return BoxType::Block;
        case css::Display::Inline:
            return BoxType::Inline;
        case css::Display::InlineBlock:
            return BoxType::InlineBlock;
        case css::Display::None:
            return BoxType::Block; // Won't be used
        default:
            return BoxType::Block;
    }
}

} // namespace lithium::layout
