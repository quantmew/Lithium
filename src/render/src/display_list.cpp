/**
 * Display List implementation
 */

#include "lithium/render/display_list.hpp"
#include <algorithm>

namespace lithium::render {

// ============================================================================
// DisplayList optimization
// ============================================================================

void DisplayList::optimize() {
    if (m_commands.empty()) return;

    // Simple optimizations:
    // 1. Remove redundant push/pop pairs
    // 2. Merge adjacent fill rects with same color

    std::vector<DisplayCommand> optimized;
    optimized.reserve(m_commands.size());

    for (usize i = 0; i < m_commands.size(); ++i) {
        const auto& cmd = m_commands[i];

        // Skip empty push/pop clip pairs
        if (i + 1 < m_commands.size()) {
            if (std::holds_alternative<PushClipCommand>(cmd) &&
                std::holds_alternative<PopClipCommand>(m_commands[i + 1])) {
                ++i;
                continue;
            }
            if (std::holds_alternative<PushOpacityCommand>(cmd) &&
                std::holds_alternative<PopOpacityCommand>(m_commands[i + 1])) {
                ++i;
                continue;
            }
            if (std::holds_alternative<PushTransformCommand>(cmd) &&
                std::holds_alternative<PopTransformCommand>(m_commands[i + 1])) {
                ++i;
                continue;
            }
        }

        optimized.push_back(cmd);
    }

    m_commands = std::move(optimized);
}

// ============================================================================
// DisplayListBuilder implementation
// ============================================================================

DisplayListBuilder::DisplayListBuilder() = default;

DisplayList DisplayListBuilder::build(const layout::LayoutBox& root) {
    m_display_list.clear();
    paint_box(root);
    m_display_list.optimize();
    return std::move(m_display_list);
}

void DisplayListBuilder::paint_box(const layout::LayoutBox& box) {
    // Skip boxes with no dimensions
    if (box.dimensions().border_box().width <= 0 ||
        box.dimensions().border_box().height <= 0) {
        return;
    }

    // Paint in order:
    // 1. Background
    paint_background(box);

    // 2. Borders
    paint_borders(box);

    // 3. Content (text)
    if (box.is_text()) {
        paint_text(box);
    }

    // 4. Children
    paint_children(box);
}

void DisplayListBuilder::paint_background(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    // Skip transparent backgrounds
    if (style.background_color.a == 0) {
        return;
    }

    RectF rect = d.border_box();
    m_display_list.push(FillRectCommand{rect, style.background_color});
}

void DisplayListBuilder::paint_borders(const layout::LayoutBox& box) {
    const auto& style = box.style();

    // Skip if no borders
    if (style.border_top_width == 0 &&
        style.border_right_width == 0 &&
        style.border_bottom_width == 0 &&
        style.border_left_width == 0) {
        return;
    }

    paint_border_top(box);
    paint_border_right(box);
    paint_border_bottom(box);
    paint_border_left(box);
}

void DisplayListBuilder::paint_border_top(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (style.border_top_width == 0 || style.border_top_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x,
        border_box.y,
        border_box.width,
        style.border_top_width
    };
    m_display_list.push(FillRectCommand{rect, style.border_top_color});
}

void DisplayListBuilder::paint_border_right(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (style.border_right_width == 0 || style.border_right_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x + border_box.width - style.border_right_width,
        border_box.y,
        style.border_right_width,
        border_box.height
    };
    m_display_list.push(FillRectCommand{rect, style.border_right_color});
}

void DisplayListBuilder::paint_border_bottom(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (style.border_bottom_width == 0 || style.border_bottom_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x,
        border_box.y + border_box.height - style.border_bottom_width,
        border_box.width,
        style.border_bottom_width
    };
    m_display_list.push(FillRectCommand{rect, style.border_bottom_color});
}

void DisplayListBuilder::paint_border_left(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (style.border_left_width == 0 || style.border_left_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x,
        border_box.y,
        style.border_left_width,
        border_box.height
    };
    m_display_list.push(FillRectCommand{rect, style.border_left_color});
}

void DisplayListBuilder::paint_text(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();
    const String& text = box.text();

    if (text.empty()) {
        return;
    }

    // Get font properties from style
    String font_family = "sans-serif"_s;  // Default
    f32 font_size = 16.0f;

    // Extract from style if available
    if (!style.font_family.empty()) {
        font_family = style.font_family;
    }
    if (style.font_size.has_value()) {
        font_size = style.font_size->value;
    }

    m_display_list.push(DrawTextCommand{
        {d.content.x, d.content.y + font_size},  // Baseline position
        text,
        font_family,
        font_size,
        style.color
    });
}

void DisplayListBuilder::paint_children(const layout::LayoutBox& box) {
    for (const auto& child : box.children()) {
        paint_box(*child);
    }
}

} // namespace lithium::render
