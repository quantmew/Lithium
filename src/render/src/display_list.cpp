/**
 * Display List implementation
 */

#include "lithium/render/display_list.hpp"
#include "lithium/core/logger.hpp"
#include <algorithm>
#include <iostream>

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
    std::cout << "DisplayListBuilder::build: starting build, root is_text=" << root.is_text() << std::endl;
    paint_box(root);
    std::cout << "DisplayListBuilder::build: before optimize, " << m_display_list.commands().size() << " commands" << std::endl;
    m_display_list.optimize();
    std::cout << "DisplayListBuilder::build: after optimize, " << m_display_list.commands().size() << " commands" << std::endl;
    return std::move(m_display_list);
}

void DisplayListBuilder::paint_box(const layout::LayoutBox& box) {
    // Skip boxes with no dimensions
    f32 width = box.dimensions().border_box().width;
    f32 height = box.dimensions().border_box().height;
    bool is_text = box.is_text();

    // TEMPORARY: Skip dimension check to debug layout tree structure
    // if (!is_text && (width <= 0 || height <= 0)) {
    //     return;
    // }

    std::cout << "DisplayListBuilder::paint_box: box "
              << width << "x" << height << " is_text=" << is_text;

    if (is_text) {
        std::cout << " text='" << box.text().c_str() << "'";
    }
    std::cout << std::endl;

    // Paint in order:
    // 1. Background (only for non-transparent backgrounds)
    paint_background(box);

    // 2. Borders
    paint_borders(box);

    // 3. Content (text)
    if (is_text && !box.text().empty()) {
        paint_text(box);
    }

    // 4. Children
    paint_children(box);
}

void DisplayListBuilder::paint_background(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    // Use white background if transparent (temporary fix)
    Color bg_color = style.background_color;
    if (bg_color.a == 0) {
        bg_color = Color::white();  // Default to white
        std::cout << "DisplayListBuilder::paint_background: using default white background" << std::endl;
    }

    RectF rect = d.border_box();
    std::cout << "DisplayListBuilder::paint_background: filling rect ("
              << rect.x << "," << rect.y << "," << rect.width << "," << rect.height
              << ") color=(" << (int)bg_color.r << ","
              << (int)bg_color.g << "," << (int)bg_color.b << ","
              << (int)bg_color.a << ")" << std::endl;
    m_display_list.push(FillRectCommand{rect, bg_color});
}

void DisplayListBuilder::paint_borders(const layout::LayoutBox& box) {
    const auto& d = box.dimensions();

    // Skip if no borders
    if (d.border.top == 0 &&
        d.border.right == 0 &&
        d.border.bottom == 0 &&
        d.border.left == 0) {
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

    if (d.border.top == 0 || style.border_top_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x,
        border_box.y,
        border_box.width,
        d.border.top
    };
    m_display_list.push(FillRectCommand{rect, style.border_top_color});
}

void DisplayListBuilder::paint_border_right(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (d.border.right == 0 || style.border_right_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x + border_box.width - d.border.right,
        border_box.y,
        d.border.right,
        border_box.height
    };
    m_display_list.push(FillRectCommand{rect, style.border_right_color});
}

void DisplayListBuilder::paint_border_bottom(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (d.border.bottom == 0 || style.border_bottom_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x,
        border_box.y + border_box.height - d.border.bottom,
        border_box.width,
        d.border.bottom
    };
    m_display_list.push(FillRectCommand{rect, style.border_bottom_color});
}

void DisplayListBuilder::paint_border_left(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();

    if (d.border.left == 0 || style.border_left_color.a == 0) {
        return;
    }

    RectF border_box = d.border_box();
    RectF rect{
        border_box.x,
        border_box.y,
        d.border.left,
        border_box.height
    };
    m_display_list.push(FillRectCommand{rect, style.border_left_color});
}

void DisplayListBuilder::paint_text(const layout::LayoutBox& box) {
    const auto& style = box.style();
    const auto& d = box.dimensions();
    const String& text = box.text();

    if (text.empty()) {
        std::cout << "DisplayListBuilder::paint_text: skipping empty text" << std::endl;
        return;
    }

    // Get font properties from style
    String font_family = "sans-serif"_s;  // Default
    f32 font_size = 16.0f;

    // Extract from style if available
    if (!style.font_family.empty()) {
        font_family = style.font_family.front();
    }
    font_size = static_cast<f32>(style.font_size.to_px(0, 16.0, 0, 0));

    std::cout << "DisplayListBuilder::paint_text: text='" << text.c_str()
              << "' pos=(" << d.content.x << "," << (d.content.y + font_size)
              << ") font_family='" << font_family.c_str()
              << "' font_size=" << font_size
              << " color=(" << (int)style.color.r << "," << (int)style.color.g << ","
              << (int)style.color.b << "," << (int)style.color.a << ")" << std::endl;

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
