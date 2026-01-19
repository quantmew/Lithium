/**
 * Inline Layout implementation (simplified)
 */

#include "lithium/layout/inline_layout.hpp"
#include <algorithm>
#include <iostream>

namespace lithium::layout {

namespace {

f32 to_pixels(const css::Length& length, const LayoutContext& context, f32 reference) {
    return static_cast<f32>(length.to_px(
        reference,
        context.root_font_size,
        context.viewport_width,
        context.viewport_height));
}

// Helper to compute the actual font size in pixels for a layout box
f32 computed_font_size_for(const LayoutBox& box, const LayoutContext& context) {
    // Start with root font size
    f32 parent_font_size = context.root_font_size;

    // Walk up from parent to root, accumulating font sizes
    const LayoutBox* current = box.parent();
    while (current) {
        const auto& style = current->style();
        f32 current_font_size = static_cast<f32>(style.font_size.to_px(
            parent_font_size,
            context.root_font_size,
            context.viewport_width,
            context.viewport_height));
        parent_font_size = current_font_size;
        current = current->parent();
    }

    // Now compute the target element's font size
    const auto& style = box.style();
    return static_cast<f32>(style.font_size.to_px(
        parent_font_size,
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

    std::cout << "InlineFormattingContext::run: collected " << boxes.size() << " inline boxes" << std::endl;

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
            d.content.width = fragment.width;  // Set width
            d.content.height = line.height;    // Set height
            std::cout << "place_line_boxes: fragment width=" << fragment.width
                      << " height=" << line.height
                      << " text='" << fragment.box->text().c_str() << "'" << std::endl;
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
        if (box->is_text()) {
            // Handle text nodes with word wrapping
            String text = box->text();
            f32 font_px = computed_font_size_for(*box, m_context);
            f32 height = box->dimensions().content.height;
            if (height == 0) {
                height = calculate_line_height(*box);
            }
            f32 baseline = height * 0.8f;

            // Try to use font backend for accurate measurement
            f32 space_width = font_px * 0.25f;  // Default approximation
            f32 avg_char_width = font_px * 0.5f;  // Default approximation

            if (m_context.font_backend) {
                // TODO: Use beryl FontManager to get font and measure text
                // For now, use simple approximation
                space_width = font_px * 0.25f;
                avg_char_width = font_px * 0.5f;
            }

            // Split text into words
            std::vector<String> words;
            usize word_start = 0;
            for (usize i = 0; i <= text.size(); ++i) {
                if (i == text.size() || text[i] == ' ' || text[i] == '\t' || text[i] == '\n') {
                    if (i > word_start) {
                        words.push_back(text.substring(word_start, i - word_start));
                    }
                    word_start = i + 1;
                }
            }

            // Layout words with line breaking
            for (const auto& word : words) {
                // Measure word accurately if font context available
                f32 word_width = static_cast<f32>(word.size()) * avg_char_width;

                if (m_context.font_backend) {
                    beryl::FontDescription desc;
                    desc.size = font_px;
                    desc.family = box->style().font_family.empty() ? String("sans-serif") : box->style().font_family[0];
                    desc.weight = (box->style().font_weight == css::FontWeight::Bold ||
                                  box->style().font_weight == css::FontWeight::W700)
                        ? beryl::FontWeight::Bold : beryl::FontWeight::Normal;
                    desc.style = (box->style().font_style == css::FontStyle::Italic)
                        ? beryl::FontStyle::Italic : beryl::FontStyle::Normal;

                    if (auto font = m_context.font_backend->get_system_font(desc)) {
                        word_width = font->measure_text(word);
                    }
                }

                // Check if we need to wrap to next line
                if (x > 0 && x + word_width > m_available_width) {
                    // Finish current line
                    m_lines.push_back(current);
                    current = LineBox{};
                    m_current_y += current.height;
                    current.y = m_current_y;
                    x = 0;
                }

                // Add word as a fragment
                current.fragments.push_back({box, x, word_width, baseline});
                current.height = std::max(current.height, height);
                current.baseline = std::max(current.baseline, baseline);
                x += word_width + space_width;
            }

            // Update the text box dimensions to span all lines
            // (This is simplified - a real implementation would split the text node)
            box->dimensions().content.width = std::min(x, m_available_width);
        } else {
            // Handle inline boxes (elements)
            f32 width = inline_layout::measure_inline_width(*box, m_context);
            f32 height = box->dimensions().content.height;
            if (height == 0) {
                height = calculate_line_height(*box);
            }
            f32 baseline = height * 0.8f;

            if (x + width > m_available_width && !current.fragments.empty()) {
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
    }

    if (!current.fragments.empty()) {
        m_lines.push_back(current);
        m_current_y += current.height;
    }
}

f32 InlineFormattingContext::measure_text(const String& text, const LayoutBox& box) {
    f32 font_px = computed_font_size_for(box, m_context);

    // Try to use font context for accurate measurement
    if (m_context.font_backend) {
        beryl::FontDescription desc;
        desc.size = font_px;
        desc.family = box.style().font_family.empty() ? String("sans-serif") : box.style().font_family[0];
        desc.weight = (box.style().font_weight == css::FontWeight::Bold ||
                      box.style().font_weight == css::FontWeight::W700)
            ? beryl::FontWeight::Bold : beryl::FontWeight::Normal;
        desc.style = (box.style().font_style == css::FontStyle::Italic)
            ? beryl::FontStyle::Italic : beryl::FontStyle::Normal;

        if (auto font = m_context.font_backend->get_system_font(desc)) {
            return font->measure_text(text);
        }
    }

    // Fallback to approximation
    return static_cast<f32>(text.size()) * font_px * 0.5f;
}

f32 InlineFormattingContext::calculate_line_height(const LayoutBox& box) {
    f32 font_px = computed_font_size_for(box, m_context);
    const auto& style = box.style();
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
        // Try to use font context for accurate measurement
        if (context.font_backend) {
            beryl::FontDescription desc;
            desc.size = font_px;
            desc.family = style.font_family.empty() ? String("sans-serif") : style.font_family[0];
            desc.weight = (style.font_weight == css::FontWeight::Bold ||
                          style.font_weight == css::FontWeight::W700)
                ? beryl::FontWeight::Bold : beryl::FontWeight::Normal;
            desc.style = (style.font_style == css::FontStyle::Italic)
                ? beryl::FontStyle::Italic : beryl::FontStyle::Normal;

            if (auto font = context.font_backend->get_system_font(desc)) {
                return font->measure_text(box.text());
            }
        }

        // Fallback to approximation
        return static_cast<f32>(box.text().size()) * font_px * 0.5f;
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
