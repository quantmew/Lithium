/**
 * Layout engine glue code (simplified)
 */

#include "lithium/layout/layout_context.hpp"
#include "lithium/layout/block_layout.hpp"
#include "lithium/layout/inline_layout.hpp"
#include "lithium/dom/element.hpp"
#include <algorithm>
#include <iostream>

namespace lithium::layout {

namespace {

// Helper to compute the actual font size in pixels for a layout box
// Similar to the one in block_layout.cpp
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

} // anonymous namespace

LayoutEngine::LayoutEngine() {
    // Initialize beryl font backend
    m_font_backend = beryl::initialize_default_font_backend();
    if (m_font_backend) {
        std::cout << "LayoutEngine: Initialized beryl font backend ("
                  << beryl::font_backend_name(m_font_backend->type()) << ")" << std::endl;
    } else {
        std::cerr << "LayoutEngine: Failed to initialize font backend" << std::endl;
    }
}

void LayoutEngine::layout(LayoutBox& root, const LayoutContext& context) {
    layout_box(root, context);
}

void LayoutEngine::layout_box(LayoutBox& box, const LayoutContext& context) {
    if (box.is_block() || box.is_anonymous()) {
        layout_block(box, context);
    } else if (box.is_inline() || box.is_text()) {
        if (box.is_text()) {
            std::cout << "layout_box: routing text node to layout_inline: '" << box.text().c_str() << "'" << std::endl;
        }
        layout_inline(box, context);
    }
}

void LayoutEngine::layout_block(LayoutBox& box, const LayoutContext& context) {
    std::cout << "layout_block: ";
    if (auto* element = dynamic_cast<dom::Element*>(box.node())) {
        std::cout << "tag='" << element->tag_name().c_str() << "'";
    } else if (box.is_anonymous()) {
        std::cout << "anonymous";
    }
    std::cout << " children=" << box.children().size() << std::endl;
    block_layout::layout(box, context);
    layout_block_children(box, context);
    calculate_block_height(box);
}

void LayoutEngine::layout_inline(LayoutBox& box, const LayoutContext& context) {
    // If box is a text node, set its dimensions
    if (box.is_text()) {
        layout_text(box, context);
    } else {
        // Otherwise, layout inline children
        layout_inline_children(box, context);
    }
}

void LayoutEngine::layout_text(LayoutBox& box, const LayoutContext& context) {
    const auto& style = box.style();
    // Use computed font size (properly resolves em units)
    f32 font_px = computed_font_size_for(box, context);
    box.dimensions().content.width = static_cast<f32>(box.text().size()) * font_px * 0.6f;
    box.dimensions().content.height = font_px;
    std::cout << "layout_text: '" << box.text().c_str() << "' size="
              << box.text().size() << " font_px=" << font_px
              << " width=" << box.dimensions().content.width
              << " height=" << box.dimensions().content.height << std::endl;
}

void LayoutEngine::calculate_block_width(LayoutBox& box, const LayoutContext& context) {
    block_layout::calculate_used_width(box, context.containing_block_width);
}

void LayoutEngine::calculate_block_position(LayoutBox&, const LayoutContext&) {
    // Positioning handled inside block layout; nothing additional for the stub.
}

void LayoutEngine::layout_block_children(LayoutBox& box, const LayoutContext& context) {
    std::cout << "layout_block_children: box has " << box.children().size() << " children" << std::endl;
    for (auto& child : box.children()) {
        std::cout << "  child: is_text=" << child->is_text() << " is_inline=" << child->is_inline() << " is_block=" << child->is_block();
        if (child->is_text()) {
            std::cout << " text='" << child->text().c_str() << "'";
        }
        std::cout << std::endl;
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
