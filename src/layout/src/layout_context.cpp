/**
 * Layout Context implementation
 */

#include "lithium/layout/layout_context.hpp"
#include "lithium/layout/box.hpp"
#include "lithium/layout/block_layout.hpp"
#include "lithium/layout/inline_layout.hpp"

namespace lithium::layout {

// ============================================================================
// Layout Context
// ============================================================================

class LayoutContext {
public:
    LayoutContext(f32 viewport_width, f32 viewport_height)
        : m_viewport_width(viewport_width)
        , m_viewport_height(viewport_height)
    {}

    void layout(LayoutBox& root) {
        // Set initial containing block
        root.dimensions().content.x = 0;
        root.dimensions().content.y = 0;
        root.dimensions().content.width = m_viewport_width;

        // Layout the tree
        layout_box(root, m_viewport_width);
    }

private:
    void layout_box(LayoutBox& box, f32 containing_width) {
        switch (box.box_type()) {
            case BoxType::Block:
                layout_block(box, containing_width);
                break;
            case BoxType::Inline:
            case BoxType::InlineBlock:
            case BoxType::Text:
                layout_inline(box, containing_width);
                break;
            case BoxType::Anonymous:
                layout_anonymous(box, containing_width);
                break;
        }
    }

    void layout_block(LayoutBox& box, f32 containing_width) {
        // Calculate width
        calculate_block_width(box, containing_width);

        // Calculate position
        calculate_block_position(box);

        // Layout children
        layout_children(box);

        // Calculate height
        calculate_block_height(box);
    }

    void calculate_block_width(LayoutBox& box, f32 containing_width) {
        const auto& style = box.style();
        auto& d = box.dimensions();

        // Margin, padding, border from style
        d.margin.left = resolve_length(style.margin_left, containing_width);
        d.margin.right = resolve_length(style.margin_right, containing_width);
        d.padding.left = resolve_length(style.padding_left, containing_width);
        d.padding.right = resolve_length(style.padding_right, containing_width);
        d.border.left = style.border_left_width;
        d.border.right = style.border_right_width;

        f32 total = d.margin.horizontal() + d.padding.horizontal() + d.border.horizontal();

        // Width
        if (style.width.has_value()) {
            d.content.width = resolve_length(*style.width, containing_width);
        } else {
            // Auto width - fill available space
            d.content.width = containing_width - total;
        }

        // Ensure non-negative
        if (d.content.width < 0) {
            d.content.width = 0;
        }
    }

    void calculate_block_position(LayoutBox& box) {
        const auto& style = box.style();
        auto& d = box.dimensions();

        // Vertical margin, padding, border
        d.margin.top = resolve_length(style.margin_top, 0);
        d.margin.bottom = resolve_length(style.margin_bottom, 0);
        d.padding.top = resolve_length(style.padding_top, 0);
        d.padding.bottom = resolve_length(style.padding_bottom, 0);
        d.border.top = style.border_top_width;
        d.border.bottom = style.border_bottom_width;

        // Position relative to parent
        if (box.parent()) {
            auto& parent_d = box.parent()->dimensions();

            // X position
            d.content.x = parent_d.content.x +
                         d.margin.left + d.border.left + d.padding.left;

            // Y position (after parent's content and previous siblings)
            d.content.y = parent_d.content.y + parent_d.content.height +
                         d.margin.top + d.border.top + d.padding.top;
        }
    }

    void calculate_block_height(LayoutBox& box) {
        const auto& style = box.style();
        auto& d = box.dimensions();

        if (style.height.has_value()) {
            d.content.height = resolve_length(*style.height, 0);
        }
        // Otherwise height is determined by children (already calculated)
    }

    void layout_inline(LayoutBox& box, f32 containing_width) {
        // Simplified inline layout
        auto& d = box.dimensions();
        const auto& style = box.style();

        d.padding.left = resolve_length(style.padding_left, containing_width);
        d.padding.right = resolve_length(style.padding_right, containing_width);

        if (box.is_text()) {
            // Text width would be calculated from font metrics
            d.content.width = box.text().size() * 8.0f; // Approximation
            d.content.height = 16.0f; // Line height approximation
        }

        layout_children(box);
    }

    void layout_anonymous(LayoutBox& box, f32 containing_width) {
        // Anonymous boxes inherit layout from context
        auto& d = box.dimensions();

        if (box.parent()) {
            d.content.x = box.parent()->dimensions().content.x;
            d.content.y = box.parent()->dimensions().content.y +
                         box.parent()->dimensions().content.height;
            d.content.width = containing_width;
        }

        layout_children(box);
    }

    void layout_children(LayoutBox& box) {
        auto& d = box.dimensions();
        f32 child_width = d.content.width;

        for (auto& child : box.children()) {
            layout_box(*child, child_width);

            // Update parent's height
            auto& child_d = child->dimensions();
            f32 child_height = child_d.margin_box().height;
            d.content.height += child_height;
        }
    }

    f32 resolve_length(const css::Length& length, f32 reference) const {
        switch (length.unit) {
            case css::Length::Unit::Px:
                return length.value;
            case css::Length::Unit::Em:
                return length.value * 16.0f; // Base font size
            case css::Length::Unit::Rem:
                return length.value * 16.0f;
            case css::Length::Unit::Percent:
                return length.value / 100.0f * reference;
            case css::Length::Unit::Vw:
                return length.value / 100.0f * m_viewport_width;
            case css::Length::Unit::Vh:
                return length.value / 100.0f * m_viewport_height;
            default:
                return length.value;
        }
    }

    f32 m_viewport_width;
    f32 m_viewport_height;
};

// ============================================================================
// Public layout function
// ============================================================================

void perform_layout(LayoutBox& root, f32 viewport_width, f32 viewport_height) {
    LayoutContext context(viewport_width, viewport_height);
    context.layout(root);
}

} // namespace lithium::layout
