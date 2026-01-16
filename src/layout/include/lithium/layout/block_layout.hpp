#pragma once

#include "box.hpp"
#include "layout_context.hpp"

namespace lithium::layout {

// ============================================================================
// Block Formatting Context
// ============================================================================

class BlockFormattingContext {
public:
    explicit BlockFormattingContext(LayoutBox& root, const LayoutContext& context);

    void run();

private:
    void layout_block_level_box(LayoutBox& box);

    // Width calculation
    void calculate_width(LayoutBox& box);
    void calculate_auto_width(LayoutBox& box);

    // Position calculation
    void calculate_position(LayoutBox& box);

    // Height calculation
    void calculate_height(LayoutBox& box);

    // Margin collapsing
    f32 collapse_margins(f32 margin1, f32 margin2);

    // Length resolution
    [[nodiscard]] f32 resolve_length(const css::Length& length, f32 reference) const;
    [[nodiscard]] std::optional<f32> resolve_length_or_auto(
        const std::optional<css::Length>& length, f32 reference) const;

    LayoutBox& m_root;
    const LayoutContext& m_context;

    // Current vertical position
    f32 m_cursor_y{0};

    // Margin state for collapsing
    f32 m_previous_margin_bottom{0};
};

// ============================================================================
// Block Layout Algorithm
// ============================================================================

namespace block_layout {

// Layout a single block box
void layout(LayoutBox& box, const LayoutContext& context);

// Calculate used width for a block box
void calculate_used_width(LayoutBox& box, f32 containing_width);

// Calculate used height for a block box
void calculate_used_height(LayoutBox& box);

// Handle margin collapsing
f32 get_collapsed_margin(f32 margin1, f32 margin2);

} // namespace block_layout

} // namespace lithium::layout
