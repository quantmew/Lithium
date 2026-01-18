/**
 * Layout engine glue code (simplified)
 */

#include "lithium/layout/layout_context.hpp"
#include "lithium/layout/block_layout.hpp"
#include "lithium/layout/inline_layout.hpp"
#include <algorithm>

namespace lithium::layout {

LayoutEngine::LayoutEngine()
    : m_font_context(std::make_unique<text::FontContext>()) {}

void LayoutEngine::layout(LayoutBox& root, const LayoutContext& context) {
    layout_box(root, context);
}

void LayoutEngine::layout_box(LayoutBox& box, const LayoutContext& context) {
    if (box.is_block() || box.is_anonymous()) {
        layout_block(box, context);
    } else if (box.is_inline() || box.is_text()) {
        layout_inline(box, context);
    }
}

void LayoutEngine::layout_block(LayoutBox& box, const LayoutContext& context) {
    block_layout::layout(box, context);
    layout_block_children(box, context);
    calculate_block_height(box);
}

void LayoutEngine::layout_inline(LayoutBox& box, const LayoutContext& context) {
    layout_inline_children(box, context);
}

void LayoutEngine::layout_text(LayoutBox& box, const LayoutContext& context) {
    const auto& style = box.style();
    f32 font_px = resolve_length(
        style.font_size,
        context.containing_block_width,
        context.root_font_size,
        context.viewport_width,
        context.viewport_height);
    box.dimensions().content.width = static_cast<f32>(box.text().size()) * font_px * 0.6f;
    box.dimensions().content.height = font_px;
}

void LayoutEngine::calculate_block_width(LayoutBox& box, const LayoutContext& context) {
    block_layout::calculate_used_width(box, context.containing_block_width);
}

void LayoutEngine::calculate_block_position(LayoutBox&, const LayoutContext&) {
    // Positioning handled inside block layout; nothing additional for the stub.
}

void LayoutEngine::layout_block_children(LayoutBox& box, const LayoutContext& context) {
    for (auto& child : box.children()) {
        LayoutContext child_ctx = context;
        child_ctx.containing_block_width = box.dimensions().content.width;
        child_ctx.containing_block_height = box.dimensions().content.height;
        layout_box(*child, child_ctx);
    }
}

void LayoutEngine::calculate_block_height(LayoutBox& box) {
    f32 max_height = box.dimensions().content.height;
    for (auto& child : box.children()) {
        const auto& d = child->dimensions();
        max_height = std::max(max_height, d.margin_box().height);
    }
    box.dimensions().content.height = max_height;
}

void LayoutEngine::layout_inline_children(LayoutBox& box, const LayoutContext& context) {
    InlineFormattingContext ifc(box, context);
    ifc.run();
}

f32 LayoutEngine::resolve_length(const css::Length& length, f32 reference,
                                 f32 root_font_size, f32 viewport_width,
                                 f32 viewport_height) const {
    (void)viewport_height;
    return static_cast<f32>(length.to_px(reference, root_font_size, viewport_width, viewport_height));
}

} // namespace lithium::layout
