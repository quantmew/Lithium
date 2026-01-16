/**
 * Block Layout implementation
 */

#include "lithium/layout/block_layout.hpp"
#include "lithium/layout/box.hpp"

namespace lithium::layout {

// ============================================================================
// Block Formatting Context
// ============================================================================

class BlockFormattingContext {
public:
    BlockFormattingContext(LayoutBox& root, f32 containing_width)
        : m_root(root)
        , m_containing_width(containing_width)
    {}

    void layout() {
        layout_block_box(m_root);
    }

private:
    void layout_block_box(LayoutBox& box) {
        // 1. Calculate width
        calculate_width(box);

        // 2. Position box in parent
        position_box(box);

        // 3. Layout children
        f32 cursor_y = box.dimensions().content.y;

        for (auto& child : box.children()) {
            if (child->is_block() || child->is_anonymous()) {
                layout_block_box(*child);
            } else {
                // Inline children handled separately
                layout_inline_children(box);
                break;
            }
        }

        // 4. Calculate height
        calculate_height(box);
    }

    void calculate_width(LayoutBox& box) {
        auto& d = box.dimensions();
        const auto& style = box.style();

        // Get values (default to 0 for auto)
        f32 margin_left = resolve_auto_length(style.margin_left, 0);
        f32 margin_right = resolve_auto_length(style.margin_right, 0);
        f32 padding_left = resolve_length(style.padding_left);
        f32 padding_right = resolve_length(style.padding_right);
        f32 border_left = style.border_left_width;
        f32 border_right = style.border_right_width;

        f32 width = 0;
        bool width_auto = !style.width.has_value();

        if (!width_auto) {
            width = resolve_length(*style.width);
        }

        f32 total = margin_left + border_left + padding_left +
                   width + padding_right + border_right + margin_right;

        // Adjust for over-constrained
        if (!width_auto && total > m_containing_width) {
            // Reduce margins
            if (margin_left == 0 && margin_right == 0) {
                margin_right = m_containing_width - total + margin_right;
            }
        }

        // Handle auto values
        f32 underflow = m_containing_width - total;

        if (width_auto) {
            if (underflow >= 0) {
                width = underflow;
            } else {
                width = 0;
                margin_right = margin_right + underflow;
            }
        } else if (margin_left == 0 && margin_right == 0) {
            // Both margins auto - center
            margin_left = underflow / 2;
            margin_right = underflow / 2;
        } else if (margin_left == 0) {
            margin_left = underflow;
        } else if (margin_right == 0) {
            margin_right = underflow;
        }

        // Set dimensions
        d.content.width = width;
        d.margin.left = margin_left;
        d.margin.right = margin_right;
        d.padding.left = padding_left;
        d.padding.right = padding_right;
        d.border.left = border_left;
        d.border.right = border_right;
    }

    void position_box(LayoutBox& box) {
        auto& d = box.dimensions();
        const auto& style = box.style();

        // Vertical dimensions
        d.margin.top = resolve_length(style.margin_top);
        d.margin.bottom = resolve_length(style.margin_bottom);
        d.padding.top = resolve_length(style.padding_top);
        d.padding.bottom = resolve_length(style.padding_bottom);
        d.border.top = style.border_top_width;
        d.border.bottom = style.border_bottom_width;

        // X position
        if (box.parent()) {
            auto& parent_d = box.parent()->dimensions();
            d.content.x = parent_d.content.x +
                         d.margin.left + d.border.left + d.padding.left;
        } else {
            d.content.x = d.margin.left + d.border.left + d.padding.left;
        }

        // Y position
        if (box.parent()) {
            auto& parent_d = box.parent()->dimensions();
            d.content.y = parent_d.content.y + parent_d.content.height +
                         d.margin.top + d.border.top + d.padding.top;

            // Margin collapsing (simplified)
            // ...
        } else {
            d.content.y = d.margin.top + d.border.top + d.padding.top;
        }
    }

    void calculate_height(LayoutBox& box) {
        const auto& style = box.style();
        auto& d = box.dimensions();

        // If height is specified, use it
        if (style.height.has_value()) {
            d.content.height = resolve_length(*style.height);
        }
        // Otherwise height was already set by children
    }

    void layout_inline_children(LayoutBox& box) {
        // Collect inline children into line boxes
        // (simplified - real implementation needs line breaking)
        auto& d = box.dimensions();
        f32 x = d.content.x;
        f32 y = d.content.y;
        f32 line_height = 16.0f;  // Default line height
        f32 max_height = 0;

        for (auto& child : box.children()) {
            auto& child_d = child->dimensions();

            if (x + child_d.content.width > d.content.x + d.content.width) {
                // Wrap to next line
                x = d.content.x;
                y += line_height;
                line_height = 16.0f;
            }

            child_d.content.x = x;
            child_d.content.y = y;

            x += child_d.margin_box().width;
            max_height = std::max(max_height, child_d.margin_box().height);
            line_height = std::max(line_height, max_height);
        }

        d.content.height = (y - d.content.y) + line_height;
    }

    f32 resolve_length(const css::Length& length) const {
        switch (length.unit) {
            case css::Length::Unit::Px:
                return length.value;
            case css::Length::Unit::Em:
            case css::Length::Unit::Rem:
                return length.value * 16.0f;
            case css::Length::Unit::Percent:
                return length.value / 100.0f * m_containing_width;
            default:
                return length.value;
        }
    }

    f32 resolve_auto_length(const css::Length& length, f32 default_value) const {
        if (length.unit == css::Length::Unit::Auto) {
            return default_value;
        }
        return resolve_length(length);
    }

    LayoutBox& m_root;
    f32 m_containing_width;
};

// ============================================================================
// Public functions
// ============================================================================

void layout_block_box(LayoutBox& box, f32 containing_width) {
    BlockFormattingContext bfc(box, containing_width);
    bfc.layout();
}

} // namespace lithium::layout
