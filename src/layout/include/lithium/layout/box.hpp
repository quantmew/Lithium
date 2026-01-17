#pragma once

#include "lithium/core/types.hpp"
#include "lithium/css/style_resolver.hpp"
#include "lithium/css/value.hpp"
#include "lithium/dom/node.hpp"
#include <memory>
#include <vector>

namespace lithium::layout {

// ============================================================================
// Box Model
// ============================================================================

struct EdgeSizes {
    f32 top{0};
    f32 right{0};
    f32 bottom{0};
    f32 left{0};

    [[nodiscard]] f32 horizontal() const { return left + right; }
    [[nodiscard]] f32 vertical() const { return top + bottom; }
};

struct Dimensions {
    RectF content;
    EdgeSizes padding;
    EdgeSizes border;
    EdgeSizes margin;

    [[nodiscard]] RectF padding_box() const {
        return {
            content.x - padding.left,
            content.y - padding.top,
            content.width + padding.horizontal(),
            content.height + padding.vertical()
        };
    }

    [[nodiscard]] RectF border_box() const {
        auto p = padding_box();
        return {
            p.x - border.left,
            p.y - border.top,
            p.width + border.horizontal(),
            p.height + border.vertical()
        };
    }

    [[nodiscard]] RectF margin_box() const {
        auto b = border_box();
        return {
            b.x - margin.left,
            b.y - margin.top,
            b.width + margin.horizontal(),
            b.height + margin.vertical()
        };
    }
};

// ============================================================================
// Box Types
// ============================================================================

enum class BoxType {
    Block,
    Inline,
    InlineBlock,
    Anonymous,  // Generated wrapper boxes
    Text,       // Text run
};

enum class DisplayOutside {
    Block,
    Inline,
    None,
};

enum class DisplayInside {
    Flow,
    FlowRoot,
    Flex,
    Grid,
    Table,
};

// ============================================================================
// Layout Box
// ============================================================================

class LayoutBox {
public:
    explicit LayoutBox(BoxType type);
    LayoutBox(BoxType type, dom::Node* node);

    // Box type
    [[nodiscard]] BoxType box_type() const { return m_box_type; }
    [[nodiscard]] bool is_block() const { return m_box_type == BoxType::Block; }
    [[nodiscard]] bool is_inline() const { return m_box_type == BoxType::Inline || m_box_type == BoxType::InlineBlock; }
    [[nodiscard]] bool is_anonymous() const { return m_box_type == BoxType::Anonymous; }
    [[nodiscard]] bool is_text() const { return m_box_type == BoxType::Text; }

    // DOM node (null for anonymous boxes)
    [[nodiscard]] dom::Node* node() const { return m_node; }

    // Style
    [[nodiscard]] const css::ComputedValue& style() const { return m_style; }
    void set_style(const css::ComputedValue& style) { m_style = style; }

    // Dimensions
    [[nodiscard]] const Dimensions& dimensions() const { return m_dimensions; }
    [[nodiscard]] Dimensions& dimensions() { return m_dimensions; }

    // For text boxes
    [[nodiscard]] const String& text() const { return m_text; }
    void set_text(const String& text) { m_text = text; }

    // Tree structure
    [[nodiscard]] LayoutBox* parent() const { return m_parent; }
    [[nodiscard]] const std::vector<std::unique_ptr<LayoutBox>>& children() const { return m_children; }

    void add_child(std::unique_ptr<LayoutBox> child);
    void insert_child(usize index, std::unique_ptr<LayoutBox> child);
    std::unique_ptr<LayoutBox> remove_child(usize index);

    // Anonymous box helpers
    [[nodiscard]] LayoutBox* get_inline_container();

    // Debug
    [[nodiscard]] String debug_string(i32 indent = 0) const;

private:
    BoxType m_box_type;
    dom::Node* m_node{nullptr};
    css::ComputedValue m_style;
    Dimensions m_dimensions;
    String m_text;  // For text boxes

    LayoutBox* m_parent{nullptr};
    std::vector<std::unique_ptr<LayoutBox>> m_children;
};

// ============================================================================
// Layout Tree Builder
// ============================================================================

class LayoutTreeBuilder {
public:
    LayoutTreeBuilder() = default;

    // Build layout tree from DOM + styles
    [[nodiscard]] std::unique_ptr<LayoutBox> build(
        dom::Document& document,
        const css::StyleResolver& resolver);

private:
    std::unique_ptr<LayoutBox> build_box(dom::Node& node, const css::StyleResolver& resolver);
    std::unique_ptr<LayoutBox> build_element_box(dom::Element& element, const css::StyleResolver& resolver);
    std::unique_ptr<LayoutBox> build_text_box(dom::Text& text, const css::ComputedValue& parent_style);

    [[nodiscard]] static BoxType determine_box_type(const css::ComputedValue& style);
};

} // namespace lithium::layout
