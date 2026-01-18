/**
 * Block Layout implementation (simplified)
 */

#include "lithium/layout/block_layout.hpp"
#include "lithium/layout/inline_layout.hpp"
#include "lithium/dom/element.hpp"
#include <algorithm>
#include <optional>
#include <iostream>

namespace lithium::layout {

namespace {

f32 containing_width_for(const LayoutBox& box, const LayoutContext& context) {
    if (box.parent()) {
        return box.parent()->dimensions().content.width;
    }
    if (context.containing_block_width > 0) {
        return context.containing_block_width;
    }
    if (context.viewport_width > 0) {
        return context.viewport_width;
    }
    return 800.0f;
}

// Get the computed font size in pixels for a layout box
// Walks up the tree to resolve em-based font sizes
f32 computed_font_size_for(const LayoutBox& box, const LayoutContext& context) {
    // Start with root font size
    f32 parent_font_size = context.root_font_size;

    // Walk up from parent to root, accumulating font sizes
    const LayoutBox* current = box.parent();
    while (current) {
        const auto& style = current->style();
        // Compute this element's font size based on its parent
        f32 current_font_size = static_cast<f32>(style.font_size.to_px(
            parent_font_size,  // Use parent's font size for em units
            context.root_font_size,
            context.viewport_width,
            context.viewport_height));
        parent_font_size = current_font_size;
        current = current->parent();
    }

    // Now compute the target element's font size
    const auto& style = box.style();
    return static_cast<f32>(style.font_size.to_px(
        parent_font_size,  // Use computed parent font size
        context.root_font_size,
        context.viewport_width,
        context.viewport_height));
}

} // namespace

BlockFormattingContext::BlockFormattingContext(LayoutBox& root, const LayoutContext& context)
    : m_root(root)
    , m_context(context) {}

void BlockFormattingContext::run() {
    layout_block_level_box(m_root);
}

void BlockFormattingContext::layout_block_level_box(LayoutBox& box) {
    // Step 1: Calculate width (based on containing block)
    calculate_width(box);

    // Step 2: Calculate position (X and Y)
    calculate_position(box);

    // Step 3: Layout block-level children vertically
    f32 current_y = box.dimensions().content.y;
    std::cout << "BFC::layout_block_level_box: tag=";
    if (auto* el = dynamic_cast<dom::Element*>(box.node())) {
        std::cout << "'" << el->tag_name().c_str() << "'";
    } else if (box.is_text()) {
        std::cout << "text";
    } else {
        std::cout << "anonymous";
    }
    std::cout << " x=" << box.dimensions().content.x
              << " y=" << box.dimensions().content.y
              << " w=" << box.dimensions().content.width
              << " h=" << box.dimensions().content.height << std::endl;

    // Track previous child's bottom margin for collapsing
    f32 previous_margin_bottom = 0;

    // Track consecutive inline children to layout together
    std::vector<LayoutBox*> inline_children;

    for (auto& child : box.children()) {
        if (child->is_block() || child->is_anonymous()) {
            // First, layout any accumulated inline children
            if (!inline_children.empty()) {
                current_y = layout_inline_sequence(box, inline_children, current_y);
                inline_children.clear();
                previous_margin_bottom = 0;
            }

            // Collapse top margin with previous bottom margin
            f32 child_margin_top = child->dimensions().margin.top;
            f32 collapsed_margin = collapse_margins(previous_margin_bottom, child_margin_top);
            current_y += collapsed_margin - previous_margin_bottom;  // Adjust for collapsed space

            // Set child's Y position to current cursor
            child->dimensions().content.y = current_y;

            // Recursively layout child
            BlockFormattingContext child_context(*child, m_context);
            child_context.run();

            // Advance cursor past child's margin box
            current_y += child->dimensions().margin_box().height;

            // Store this child's bottom margin for next iteration
            previous_margin_bottom = child->dimensions().margin.bottom;
        } else if (child->is_inline() || child->is_text()) {
            // Accumulate inline children for batch layout
            inline_children.push_back(child.get());
        }
    }

    // Layout any remaining inline children
    if (!inline_children.empty()) {
        current_y = layout_inline_sequence(box, inline_children, current_y);
    }

    // Step 4: Calculate height based on children
    calculate_height(box);

    m_cursor_y = current_y;
}

void BlockFormattingContext::calculate_width(LayoutBox& box) {
    auto& d = box.dimensions();
    const auto& style = box.style();
    f32 containing_width = containing_width_for(box, m_context);

    // Get the element's computed font size in pixels for resolving em units
    f32 font_size_px = computed_font_size_for(box, m_context);

    // For margins/paddings/borders: use font_size as reference for em units
    f32 margin_left = resolve_length_with_font(style.margin_left, containing_width, font_size_px);
    f32 margin_right = resolve_length_with_font(style.margin_right, containing_width, font_size_px);
    f32 padding_left = resolve_length_with_font(style.padding_left, containing_width, font_size_px);
    f32 padding_right = resolve_length_with_font(style.padding_right, containing_width, font_size_px);
    f32 border_left = resolve_length_with_font(style.border_left_width, containing_width, font_size_px);
    f32 border_right = resolve_length_with_font(style.border_right_width, containing_width, font_size_px);

    f32 used_width = !style.width.has_value()
        ? containing_width - margin_left - margin_right - padding_left - padding_right - border_left - border_right
        : resolve_length_with_font(*style.width, containing_width, font_size_px);

    if (used_width < 0) {
        used_width = 0;
    }

    d.margin.left = margin_left;
    d.margin.right = margin_right;
    d.padding.left = padding_left;
    d.padding.right = padding_right;
    d.border.left = border_left;
    d.border.right = border_right;
    d.content.width = used_width;

    // Also set top and bottom margins (using font size for em units)
    d.margin.top = resolve_length_with_font(style.margin_top, containing_width, font_size_px);
    d.margin.bottom = resolve_length_with_font(style.margin_bottom, containing_width, font_size_px);
    d.padding.top = resolve_length_with_font(style.padding_top, containing_width, font_size_px);
    d.padding.bottom = resolve_length_with_font(style.padding_bottom, containing_width, font_size_px);
    d.border.top = resolve_length_with_font(style.border_top_width, containing_width, font_size_px);
    d.border.bottom = resolve_length_with_font(style.border_bottom_width, containing_width, font_size_px);

    if (auto* el = dynamic_cast<dom::Element*>(box.node())) {
        std::cout << "  calculate_width '" << el->tag_name().c_str()
                  << "' margin_t=" << d.margin.top << " margin_b=" << d.margin.bottom
                  << " margin_l=" << margin_left << " margin_r=" << margin_right
                  << " width=" << used_width << std::endl;
    }
}

void BlockFormattingContext::calculate_auto_width(LayoutBox& box) {
    calculate_width(box);
}

void BlockFormattingContext::calculate_position(LayoutBox& box) {
    auto& d = box.dimensions();
    const auto& style = box.style();
    f32 containing_width = containing_width_for(box, m_context);

    // Get the element's computed font size in pixels for resolving em units
    f32 font_size_px = computed_font_size_for(box, m_context);

    d.margin.top = resolve_length_with_font(style.margin_top, containing_width, font_size_px);
    d.margin.bottom = resolve_length_with_font(style.margin_bottom, containing_width, font_size_px);
    d.padding.top = resolve_length_with_font(style.padding_top, containing_width, font_size_px);
    d.padding.bottom = resolve_length_with_font(style.padding_bottom, containing_width, font_size_px);
    d.border.top = resolve_length_with_font(style.border_top_width, containing_width, font_size_px);
    d.border.bottom = resolve_length_with_font(style.border_bottom_width, containing_width, font_size_px);

    // Calculate X position (horizontal)
    if (box.parent()) {
        auto& parent_d = box.parent()->dimensions();
        d.content.x = parent_d.content.x + d.margin.left;
    } else {
        d.content.x = d.margin.left;
    }

    // Note: Y position is set by layout_block_level_box during vertical layout
    // Don't override it here
}

void BlockFormattingContext::calculate_height(LayoutBox& box) {
    auto& d = box.dimensions();
    const auto& style = box.style();

    if (style.height.has_value()) {
        d.content.height = resolve_length(*style.height, d.content.height);
        return;
    }

    f32 total_height = 0;
    for (auto& child : box.children()) {
        const auto& child_dim = child->dimensions();
        total_height = std::max(
            total_height,
            (child_dim.content.y - d.content.y) + child_dim.margin_box().height);
    }

    d.content.height = std::max(total_height, d.content.height);
}

f32 BlockFormattingContext::collapse_margins(f32 margin1, f32 margin2) {
    return std::max(margin1, margin2);
}

f32 BlockFormattingContext::resolve_length(const css::Length& length, f32 reference) const {
    return static_cast<f32>(length.to_px(
        reference,
        m_context.root_font_size,
        m_context.viewport_width,
        m_context.viewport_height));
}

f32 BlockFormattingContext::resolve_length_with_font(const css::Length& length, f32 containing_width, f32 font_size) const {
    // For % units: use containing_width as reference
    // For em/ex/ch units: use font_size as reference
    // For other units: use containing_width (doesn't matter for px/rem/vw/etc)
    f32 reference = containing_width;

    switch (length.unit) {
        case css::LengthUnit::Em:
        case css::LengthUnit::Ex:
        case css::LengthUnit::Ch:
            reference = font_size;
            break;
        case css::LengthUnit::Percent:
            reference = containing_width;
            break;
        default:
            break;
    }

    return static_cast<f32>(length.to_px(
        reference,
        m_context.root_font_size,
        m_context.viewport_width,
        m_context.viewport_height));
}

std::optional<f32> BlockFormattingContext::resolve_length_or_auto(
    const std::optional<css::Length>& length, f32 reference) const {
    if (!length.has_value()) {
        return std::nullopt;
    }
    return resolve_length(*length, reference);
}

f32 BlockFormattingContext::layout_inline_sequence(LayoutBox& container, const std::vector<LayoutBox*>& inline_children, f32 y) {
    if (inline_children.empty()) {
        return y;
    }

    std::cout << "BFC::layout_inline_sequence: " << inline_children.size() << " inline children at y=" << y << std::endl;

    // Set the starting Y position for all inline children
    for (auto* child : inline_children) {
        child->dimensions().content.y = y;
    }

    // Calculate text dimensions first
    f32 total_width = 0;
    f32 max_height = 0;

    for (auto* child : inline_children) {
        if (child->is_text()) {
            const auto& style = child->style();
            f32 font_px = static_cast<f32>(style.font_size.to_px(
                m_context.root_font_size,
                m_context.root_font_size,
                m_context.viewport_width,
                m_context.viewport_height));

            // Use font context for accurate measurement if available
            f32 text_width;
            if (m_context.font_context) {
                text::FontDescription desc;
                desc.size = font_px;
                desc.family = style.font_family.empty() ? String("sans-serif") : style.font_family[0];
                desc.bold = (style.font_weight == css::FontWeight::Bold ||
                             style.font_weight == css::FontWeight::W700);
                desc.italic = (style.font_style == css::FontStyle::Italic);

                if (auto font = m_context.font_context->get_font(desc)) {
                    text_width = font->measure_text(child->text());
                } else {
                    text_width = static_cast<f32>(child->text().size()) * font_px * 0.5f;
                }
            } else {
                text_width = static_cast<f32>(child->text().size()) * font_px * 0.5f;
            }

            f32 text_height = font_px;

            child->dimensions().content.width = text_width;
            child->dimensions().content.height = text_height;

            total_width += text_width;
            max_height = std::max(max_height, text_height);
        } else if (child->is_inline()) {
            // For inline elements, recursively layout their children
            BlockFormattingContext child_context(*child, m_context);
            child_context.run();

            total_width += child->dimensions().margin_box().width;
            max_height = std::max(max_height, child->dimensions().margin_box().height);
        }
    }

    // Now do inline layout with line breaking
    f32 x_cursor = container.dimensions().content.x;
    f32 line_height = max_height;
    f32 available_width = container.dimensions().content.width;

    for (auto* child : inline_children) {
        f32 child_width = child->dimensions().margin_box().width;

        // Check if we need to wrap to next line
        if (x_cursor + child_width > available_width && x_cursor > container.dimensions().content.x) {
            y += line_height;
            x_cursor = container.dimensions().content.x;
        }

        child->dimensions().content.x = x_cursor;
        child->dimensions().content.y = y;
        x_cursor += child_width;
    }

    f32 final_height = y + line_height - container.dimensions().content.y;
    std::cout << "BFC::layout_inline_sequence: final_height=" << final_height << std::endl;

    return y + line_height;
}

namespace block_layout {

void layout(LayoutBox& box, const LayoutContext& context) {
    BlockFormattingContext bfc(box, context);
    bfc.run();
}

void calculate_used_width(LayoutBox& box, f32 containing_width) {
    LayoutContext ctx;
    ctx.containing_block_width = containing_width;
    layout(box, ctx);
}

void calculate_used_height(LayoutBox& box) {
    LayoutContext ctx;
    layout(box, ctx);
}

f32 get_collapsed_margin(f32 margin1, f32 margin2) {
    return std::max(margin1, margin2);
}

} // namespace block_layout

} // namespace lithium::layout
