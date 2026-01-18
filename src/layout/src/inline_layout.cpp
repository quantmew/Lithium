/**
 * Inline Layout implementation (simplified)
 */

#include "lithium/layout/inline_layout.hpp"
#include <algorithm>

namespace lithium::layout {

namespace {

f32 to_pixels(const css::Length& length, const LayoutContext& context, f32 reference) {
    return static_cast<f32>(length.to_px(
        reference,
        context.root_font_size,
        context.viewport_width,
        context.viewport_height));
}

} // namespace

InlineFormattingContext::InlineFormattingContext(LayoutBox& container, const LayoutContext& context)
    : m_container(container)
    , m_context(context) {}

void InlineFormattingContext::run() {
    std::vector<LayoutBox*> boxes;
    collect_inline_boxes(m_container, boxes);

    m_available_width = m_context.containing_block_width > 0
        ? m_context.containing_block_width
        : m_container.dimensions().content.width;

    break_lines(boxes);

    f32 line_y = m_container.dimensions().content.y;
    for (auto& line : m_lines) {
        align_line_vertically(line);
        align_line_horizontally(line, m_available_width);

        f32 x_cursor = m_container.dimensions().content.x;
        for (auto& fragment : line.fragments) {
            auto& d = fragment.box->dimensions();
            d.content.x = x_cursor + fragment.x;
            d.content.y = line_y + line.y;
            x_cursor += fragment.width;
        }

        line_y += line.height;
    }

    m_container.dimensions().content.height = line_y - m_container.dimensions().content.y;
}

void InlineFormattingContext::collect_inline_boxes(LayoutBox& box, std::vector<LayoutBox*>& boxes) {
    for (auto& child : box.children()) {
        if (child->is_inline() || child->is_text()) {
            boxes.push_back(child.get());
        } else if (child->is_anonymous()) {
            collect_inline_boxes(*child, boxes);
        }
    }
}

void InlineFormattingContext::layout_line(std::vector<LayoutBox*>& boxes, usize start, usize end) {
    std::vector<LayoutBox*> slice;
    slice.insert(slice.end(), boxes.begin() + static_cast<isize>(start), boxes.begin() + static_cast<isize>(end));
    break_lines(slice);
}

void InlineFormattingContext::break_lines(std::vector<LayoutBox*>& boxes) {
    if (boxes.empty()) {
        return;
    }

    LineBox current;
    current.y = m_current_y;
    f32 x = 0;

    for (auto* box : boxes) {
        f32 width = inline_layout::measure_inline_width(*box, m_context);
        f32 height = box->dimensions().content.height;
        if (height == 0) {
            height = calculate_line_height(box->style());
        }
        f32 baseline = height * 0.8f;

        if (x + width > m_available_width && !current.fragments.empty()) {
            current.height = std::max(current.height, height);
            m_lines.push_back(current);
            current = LineBox{};
            m_current_y += current.height;
            current.y = m_current_y;
            x = 0;
        }

        current.fragments.push_back({box, x, width, baseline});
        current.height = std::max(current.height, height);
        current.baseline = std::max(current.baseline, baseline);
        x += width;
    }

    if (!current.fragments.empty()) {
        m_lines.push_back(current);
        m_current_y += current.height;
    }
}

f32 InlineFormattingContext::measure_text(const String& text, const css::ComputedValue& style) {
    f32 font_px = to_pixels(style.font_size, m_context, m_context.root_font_size);
    return static_cast<f32>(text.size()) * font_px * 0.6f;
}

f32 InlineFormattingContext::calculate_line_height(const css::ComputedValue& style) {
    f32 font_px = to_pixels(style.font_size, m_context, m_context.root_font_size);
    f32 lh = to_pixels(style.line_height, m_context, font_px);
    if (lh <= 0) {
        lh = font_px * 1.2f;
    }
    return lh;
}

void InlineFormattingContext::align_line_vertically(LineBox& line) {
    if (line.baseline == 0) {
        line.baseline = line.height * 0.8f;
    }
}

void InlineFormattingContext::align_line_horizontally(LineBox& line, f32 available_width) {
    (void)line;
    (void)available_width;
    // Left aligned by default; nothing to do here for the simplified layout.
}

namespace inline_layout {

f32 measure_inline_width(LayoutBox& box, const LayoutContext& context) {
    if (box.dimensions().content.width > 0) {
        return box.dimensions().content.width;
    }

    const auto& style = box.style();
    f32 font_px = to_pixels(style.font_size, context, context.root_font_size);

    if (box.is_text()) {
        return static_cast<f32>(box.text().size()) * font_px * 0.6f;
    }

    return font_px;  // Fallback width
}

f32 calculate_baseline(const LayoutBox& box) {
    return box.dimensions().content.height * 0.8f;
}

std::vector<BreakOpportunity> find_break_opportunities(const String& text, const css::ComputedValue& style) {
    std::vector<BreakOpportunity> breaks;
    f32 accumulated_width = 0;
    f32 char_width = static_cast<f32>(text.size()) > 0
        ? (to_pixels(style.font_size, LayoutContext{}, 16.0f) * 0.6f)
        : 0.0f;

    for (usize i = 0; i < text.size(); ++i) {
        accumulated_width += char_width;
        if (text[i] == ' ') {
            breaks.push_back({i + 1, accumulated_width, false});
        }
    }

    return breaks;
}

} // namespace inline_layout

} // namespace lithium::layout
