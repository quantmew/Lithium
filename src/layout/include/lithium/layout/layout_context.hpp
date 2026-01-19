#pragma once

#include "box.hpp"
#include "lithium/beryl/beryl.hpp"

namespace lithium::layout {

// ============================================================================
// Layout Context
// ============================================================================

struct LayoutContext {
    // Containing block dimensions
    f32 containing_block_width{0};
    f32 containing_block_height{0};

    // Viewport
    f32 viewport_width{0};
    f32 viewport_height{0};

    // Root font size (for rem units)
    f32 root_font_size{16};

    // Font backend for text measurement (using beryl)
    beryl::IFontBackend* font_backend{nullptr};
};

// ============================================================================
// Layout Engine
// ============================================================================

class LayoutEngine {
public:
    LayoutEngine();

    // Perform layout on a tree
    void layout(LayoutBox& root, const LayoutContext& context);

    // Get font backend
    [[nodiscard]] beryl::IFontBackend* font_backend() { return m_font_backend.get(); }

private:
    void layout_box(LayoutBox& box, const LayoutContext& context);
    void layout_block(LayoutBox& box, const LayoutContext& context);
    void layout_inline(LayoutBox& box, const LayoutContext& context);
    void layout_text(LayoutBox& box, const LayoutContext& context);

    // Block layout helpers
    void calculate_block_width(LayoutBox& box, const LayoutContext& context);
    void calculate_block_position(LayoutBox& box, const LayoutContext& context);
    void layout_block_children(LayoutBox& box, const LayoutContext& context);
    void calculate_block_height(LayoutBox& box);

    // Inline layout
    void layout_inline_children(LayoutBox& box, const LayoutContext& context);

    // Value resolution
    [[nodiscard]] f32 resolve_length(const css::Length& length, f32 reference,
                                      f32 root_font_size, f32 viewport_width,
                                      f32 viewport_height) const;

    std::unique_ptr<beryl::IFontBackend> m_font_backend;
};

} // namespace lithium::layout
