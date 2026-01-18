/**
 * Block Layout implementation (simplified)
 */

#include "lithium/layout/block_layout.hpp"
#include "lithium/layout/inline_layout.hpp"
#include <algorithm>
#include <optional>

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

} // namespace

BlockFormattingContext::BlockFormattingContext(LayoutBox& root, const LayoutContext& context)
    : m_root(root)
    , m_context(context) {}

void BlockFormattingContext::run() {
    layout_block_level_box(m_root);
}

void BlockFormattingContext::layout_block_level_box(LayoutBox& box) {
    calculate_width(box);
    calculate_position(box);

    f32 current_y = box.dimensions().content.y;

    for (auto& child : box.children()) {
        if (child->is_block() || child->is_anonymous()) {
            BlockFormattingContext child_context(*child, m_context);
            child_context.m_cursor_y = 0;
            child_context.run();
            current_y = std::max(
                current_y,
                child->dimensions().content.y + child->dimensions().margin_box().height);
        }
    }

    calculate_height(box);
    m_cursor_y = std::max(m_cursor_y, current_y);
}

void BlockFormattingContext::calculate_width(LayoutBox& box) {
    auto& d = box.dimensions();
    const auto& style = box.style();
    f32 containing_width = containing_width_for(box, m_context);

    f32 margin_left = resolve_length(style.margin_left, containing_width);
    f32 margin_right = resolve_length(style.margin_right, containing_width);
    f32 padding_left = resolve_length(style.padding_left, containing_width);
    f32 padding_right = resolve_length(style.padding_right, containing_width);
    f32 border_left = resolve_length(style.border_left_width, containing_width);
    f32 border_right = resolve_length(style.border_right_width, containing_width);

    f32 used_width = !style.width.has_value()
        ? containing_width - margin_left - margin_right - padding_left - padding_right - border_left - border_right
        : resolve_length(*style.width, containing_width);

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
}

void BlockFormattingContext::calculate_auto_width(LayoutBox& box) {
    calculate_width(box);
}

void BlockFormattingContext::calculate_position(LayoutBox& box) {
    auto& d = box.dimensions();
    const auto& style = box.style();
    f32 containing_width = containing_width_for(box, m_context);

    d.margin.top = resolve_length(style.margin_top, containing_width);
    d.margin.bottom = resolve_length(style.margin_bottom, containing_width);
    d.padding.top = resolve_length(style.padding_top, containing_width);
    d.padding.bottom = resolve_length(style.padding_bottom, containing_width);
    d.border.top = resolve_length(style.border_top_width, containing_width);
    d.border.bottom = resolve_length(style.border_bottom_width, containing_width);

    if (box.parent()) {
        auto& parent_d = box.parent()->dimensions();
        d.content.x = parent_d.content.x + d.margin.left + d.border.left + d.padding.left;
        d.content.y = parent_d.content.y + parent_d.content.height +
                      d.margin.top + d.border.top + d.padding.top;
    } else {
        d.content.x = d.margin.left + d.border.left + d.padding.left;
        d.content.y = d.margin.top + d.border.top + d.padding.top;
    }
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

std::optional<f32> BlockFormattingContext::resolve_length_or_auto(
    const std::optional<css::Length>& length, f32 reference) const {
    if (!length.has_value()) {
        return std::nullopt;
    }
    return resolve_length(*length, reference);
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
