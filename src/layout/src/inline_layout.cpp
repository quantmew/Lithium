/**
 * Inline Layout implementation
 */

#include "lithium/layout/inline_layout.hpp"
#include "lithium/layout/box.hpp"

namespace lithium::layout {

// ============================================================================
// Line Box
// ============================================================================

struct LineBox {
    f32 x{0};
    f32 y{0};
    f32 width{0};
    f32 height{0};
    f32 baseline{0};
    std::vector<LayoutBox*> fragments;
};

// ============================================================================
// Inline Formatting Context
// ============================================================================

class InlineFormattingContext {
public:
    InlineFormattingContext(LayoutBox& container, f32 containing_width)
        : m_container(container)
        , m_containing_width(containing_width)
    {}

    void layout() {
        // Collect all inline-level boxes
        std::vector<LayoutBox*> inline_boxes;
        collect_inline_boxes(m_container, inline_boxes);

        if (inline_boxes.empty()) {
            return;
        }

        // Create line boxes
        create_line_boxes(inline_boxes);

        // Position fragments
        position_line_boxes();

        // Update container height
        if (!m_lines.empty()) {
            m_container.dimensions().content.height = m_current_y;
        }
    }

private:
    void collect_inline_boxes(LayoutBox& box, std::vector<LayoutBox*>& result) {
        for (auto& child : box.children()) {
            if (child->is_inline() || child->is_text()) {
                result.push_back(child.get());
            } else if (child->is_anonymous()) {
                collect_inline_boxes(*child, result);
            }
            // Skip block-level boxes
        }
    }

    void create_line_boxes(const std::vector<LayoutBox*>& inline_boxes) {
        LineBox current_line;
        auto& container_d = m_container.dimensions();
        current_line.x = container_d.content.x;
        current_line.y = container_d.content.y;

        f32 available_width = m_containing_width;
        f32 x = 0;

        for (LayoutBox* box : inline_boxes) {
            f32 box_width = calculate_inline_width(*box);
            f32 box_height = calculate_inline_height(*box);

            // Check if we need to wrap
            if (x + box_width > available_width && x > 0) {
                // Finish current line
                current_line.width = x;
                m_lines.push_back(current_line);

                // Start new line
                m_current_y += current_line.height;
                current_line = LineBox{};
                current_line.x = container_d.content.x;
                current_line.y = container_d.content.y + m_current_y;
                x = 0;
            }

            // Add to current line
            current_line.fragments.push_back(box);
            current_line.height = std::max(current_line.height, box_height);

            // Calculate baseline
            f32 baseline = box_height * 0.8f;  // Approximation
            current_line.baseline = std::max(current_line.baseline, baseline);

            x += box_width;
        }

        // Add final line
        if (!current_line.fragments.empty()) {
            current_line.width = x;
            m_lines.push_back(current_line);
            m_current_y += current_line.height;
        }
    }

    void position_line_boxes() {
        auto& container_d = m_container.dimensions();

        f32 y = container_d.content.y;

        for (auto& line : m_lines) {
            f32 x = container_d.content.x;

            // Horizontal alignment (simplified - left aligned)
            for (LayoutBox* box : line.fragments) {
                auto& d = box->dimensions();
                d.content.x = x;
                d.content.y = y + (line.baseline - d.content.height * 0.8f);

                x += d.margin_box().width;
            }

            y += line.height;
        }
    }

    f32 calculate_inline_width(LayoutBox& box) {
        auto& d = box.dimensions();
        const auto& style = box.style();

        if (box.is_text()) {
            // Simple calculation - real implementation uses font metrics
            d.content.width = box.text().size() * 8.0f;
        } else if (style.width.has_value()) {
            d.content.width = resolve_length(*style.width);
        }

        // Add horizontal spacing
        d.margin.left = resolve_length(style.margin_left);
        d.margin.right = resolve_length(style.margin_right);
        d.padding.left = resolve_length(style.padding_left);
        d.padding.right = resolve_length(style.padding_right);
        d.border.left = style.border_left_width;
        d.border.right = style.border_right_width;

        return d.margin_box().width;
    }

    f32 calculate_inline_height(LayoutBox& box) {
        auto& d = box.dimensions();
        const auto& style = box.style();

        if (box.is_text()) {
            d.content.height = 16.0f;  // Line height approximation
        } else if (style.height.has_value()) {
            d.content.height = resolve_length(*style.height);
        } else {
            d.content.height = 16.0f;
        }

        // Vertical padding/border for inline-block
        if (box.box_type() == BoxType::InlineBlock) {
            d.margin.top = resolve_length(style.margin_top);
            d.margin.bottom = resolve_length(style.margin_bottom);
            d.padding.top = resolve_length(style.padding_top);
            d.padding.bottom = resolve_length(style.padding_bottom);
            d.border.top = style.border_top_width;
            d.border.bottom = style.border_bottom_width;
        }

        return d.margin_box().height;
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

    LayoutBox& m_container;
    f32 m_containing_width;
    f32 m_current_y{0};
    std::vector<LineBox> m_lines;
};

// ============================================================================
// Public functions
// ============================================================================

void layout_inline_boxes(LayoutBox& container, f32 containing_width) {
    InlineFormattingContext ifc(container, containing_width);
    ifc.layout();
}

} // namespace lithium::layout
